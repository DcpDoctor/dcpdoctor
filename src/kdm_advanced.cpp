#include "dcpdoctor/kdm_advanced.h"
#include <libxml/parser.h>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <fstream>

namespace dcpdoctor {
namespace fs = std::filesystem;
namespace {

std::string find_text_recursive(xmlNodePtr node, const char* name) {
    for (auto cur = node; cur; cur = cur->next) {
        if (cur->type == XML_ELEMENT_NODE &&
            xmlStrcmp(cur->name, BAD_CAST name) == 0) {
            auto content = xmlNodeGetContent(cur);
            if (content) {
                std::string r(reinterpret_cast<const char*>(content));
                xmlFree(content);
                return r;
            }
        }
        auto child_result = find_text_recursive(cur->children, name);
        if (!child_result.empty()) return child_result;
    }
    return {};
}

std::chrono::system_clock::time_point parse_iso_time(const std::string& str) {
    std::tm tm{};
    std::istringstream ss(str);
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    if (ss.fail()) return {};
    return std::chrono::system_clock::from_time_t(timegm(&tm));
}

} // namespace

DkdmInfo parse_dkdm(const fs::path& dkdm_path) {
    DkdmInfo info;

    auto doc = xmlReadFile(dkdm_path.c_str(), nullptr,
                          XML_PARSE_NOERROR | XML_PARSE_NOWARNING | XML_PARSE_NONET);
    if (!doc) {
        info.error = "Failed to parse DKDM XML";
        return info;
    }

    auto root = xmlDocGetRootElement(doc);
    if (!root) {
        info.error = "Empty document";
        xmlFreeDoc(doc);
        return info;
    }

    std::string root_name(reinterpret_cast<const char*>(root->name));
    if (root_name != "DCinemaSecurityMessage") {
        info.error = "Not a KDM/DKDM (root: " + root_name + ")";
        xmlFreeDoc(doc);
        return info;
    }

    info.cpl_id = find_text_recursive(root->children, "CompositionPlaylistId");
    info.content_title = find_text_recursive(root->children, "ContentTitleText");
    info.issuer = find_text_recursive(root->children, "X509IssuerName");
    info.recipient = find_text_recursive(root->children, "X509SerialNumber");

    auto not_before = find_text_recursive(root->children, "ContentKeysNotValidBefore");
    auto not_after = find_text_recursive(root->children, "ContentKeysNotValidAfter");

    if (!not_before.empty()) info.not_valid_before = parse_iso_time(not_before);
    if (!not_after.empty()) info.not_valid_after = parse_iso_time(not_after);

    // DKDM detection: DKDMs typically have a specific AuthenticatedPublic structure
    // where the recipient is another certificate (not a playback device)
    auto device_list = find_text_recursive(root->children, "DeviceListIdentifier");
    info.is_dkdm = device_list.empty();  // DKDMs don't have DeviceListIdentifier

    info.valid = true;
    xmlFreeDoc(doc);
    return info;
}

std::vector<Note> validate_dkdm(const fs::path& dkdm_path) {
    std::vector<Note> notes;

    auto info = parse_dkdm(dkdm_path);
    if (!info.valid) {
        notes.push_back(Note{Severity::error, Code::encryption_detected,
                        "Invalid DKDM: " + info.error, dkdm_path});
        return notes;
    }

    if (!info.is_dkdm) {
        notes.push_back(Note{Severity::info, Code::encryption_detected,
                        "File is a KDM (not a DKDM) - has DeviceListIdentifier",
                        dkdm_path});
    }

    auto now = std::chrono::system_clock::now();
    if (info.not_valid_after != std::chrono::system_clock::time_point{} &&
        now > info.not_valid_after) {
        notes.push_back(Note{Severity::error, Code::encryption_detected,
                        "DKDM has expired", dkdm_path});
    }

    return notes;
}

std::vector<TdlEntry> load_trusted_device_list(const fs::path& tdl_path) {
    std::vector<TdlEntry> entries;

    auto doc = xmlReadFile(tdl_path.c_str(), nullptr,
                          XML_PARSE_NOERROR | XML_PARSE_NOWARNING | XML_PARSE_NONET);
    if (!doc) return entries;

    auto root = xmlDocGetRootElement(doc);
    if (!root) { xmlFreeDoc(doc); return entries; }

    // Parse TDL XML format (varies by implementation)
    // Common format: <TrustedDeviceList><TrustedDevice><CertificateThumbprint>...</>...</>...</>
    for (auto td = root->children; td; td = td->next) {
        if (td->type != XML_ELEMENT_NODE) continue;
        if (xmlStrcmp(td->name, BAD_CAST "TrustedDevice") != 0 &&
            xmlStrcmp(td->name, BAD_CAST "Device") != 0) continue;

        TdlEntry entry;
        entry.thumbprint = find_text_recursive(td->children, "CertificateThumbprint");
        if (entry.thumbprint.empty())
            entry.thumbprint = find_text_recursive(td->children, "Thumbprint");
        entry.common_name = find_text_recursive(td->children, "CommonName");
        if (entry.common_name.empty())
            entry.common_name = find_text_recursive(td->children, "DeviceName");
        entry.organization = find_text_recursive(td->children, "Organization");

        if (!entry.thumbprint.empty())
            entries.push_back(std::move(entry));
    }

    xmlFreeDoc(doc);
    return entries;
}

std::vector<Note> validate_kdm_against_tdl(const fs::path& kdm_path,
                                            const std::vector<TdlEntry>& tdl) {
    std::vector<Note> notes;

    if (tdl.empty()) {
        notes.push_back(Note{Severity::warning, Code::encryption_detected,
                        "Trusted Device List is empty", kdm_path});
        return notes;
    }

    // Parse KDM to get recipient certificate info
    auto doc = xmlReadFile(kdm_path.c_str(), nullptr,
                          XML_PARSE_NOERROR | XML_PARSE_NOWARNING | XML_PARSE_NONET);
    if (!doc) {
        notes.push_back(Note{Severity::error, Code::encryption_detected,
                        "Cannot parse KDM for TDL validation", kdm_path});
        return notes;
    }

    auto root = xmlDocGetRootElement(doc);
    auto recipient_serial = find_text_recursive(root->children, "X509SerialNumber");
    xmlFreeDoc(doc);

    if (recipient_serial.empty()) {
        notes.push_back(Note{Severity::warning, Code::encryption_detected,
                        "KDM has no recipient certificate serial number", kdm_path});
        return notes;
    }

    // Check if recipient is in TDL
    bool found = false;
    for (const auto& entry : tdl) {
        if (entry.thumbprint == recipient_serial ||
            entry.common_name == recipient_serial) {
            found = true;
            break;
        }
    }

    if (!found) {
        notes.push_back(Note{Severity::warning, Code::encryption_detected,
                        "KDM recipient not found in Trusted Device List",
                        kdm_path});
    }

    return notes;
}

KdmAnnotation parse_kdm_annotation(const std::string& annotation_text) {
    KdmAnnotation ann;

    // Standard KDM annotation format:
    // "FacilityName.ScreenName_ContentTitle_ValidFrom_ValidTo"
    // or variations with different separators

    auto parts = std::vector<std::string>{};
    std::string current;
    for (char c : annotation_text) {
        if (c == '_' || c == '.') {
            if (!current.empty()) parts.push_back(current);
            current.clear();
        } else {
            current += c;
        }
    }
    if (!current.empty()) parts.push_back(current);

    if (parts.size() >= 4) {
        ann.facility_name = parts[0];
        ann.screen_name = parts[1];
        ann.content_title = parts[2];
        ann.valid_from = parts.size() > 3 ? parts[3] : "";
        ann.valid_to = parts.size() > 4 ? parts[4] : "";
        ann.valid_format = true;
    } else if (parts.size() >= 2) {
        ann.content_title = parts[0];
        ann.facility_name = parts.size() > 1 ? parts[1] : "";
        ann.valid_format = false;  // Non-standard format
    }

    return ann;
}

TimezoneKdmResult check_kdm_timezone(const fs::path& kdm_path, int utc_offset_hours) {
    TimezoneKdmResult result;
    result.utc_offset_hours = utc_offset_hours;

    auto doc = xmlReadFile(kdm_path.c_str(), nullptr,
                          XML_PARSE_NOERROR | XML_PARSE_NOWARNING | XML_PARSE_NONET);
    if (!doc) return result;

    auto root = xmlDocGetRootElement(doc);
    auto not_before = find_text_recursive(root->children, "ContentKeysNotValidBefore");
    auto not_after = find_text_recursive(root->children, "ContentKeysNotValidAfter");
    xmlFreeDoc(doc);

    if (not_before.empty() || not_after.empty()) return result;

    auto utc_start = parse_iso_time(not_before);
    auto utc_end = parse_iso_time(not_after);

    // Apply timezone offset
    auto offset = std::chrono::hours(utc_offset_hours);
    result.local_start = utc_start + offset;
    result.local_end = utc_end + offset;

    auto now = std::chrono::system_clock::now();
    result.valid_now_utc = (now >= utc_start && now <= utc_end);
    result.valid_now = (now >= result.local_start && now <= result.local_end);

    return result;
}

} // namespace dcpdoctor
