#include "dcpdoctor/premium.h"
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <AS_DCP.h>
#include <KM_fileio.h>
#include <openssl/sha.h>
#include <fstream>
#include <functional>
#include <sstream>
#include <regex>
#include <cstring>
#include <algorithm>

namespace dcpdoctor {
namespace fs = std::filesystem;

// ============================================================================
// TTML / IMSC Subtitle Validation
// ============================================================================

namespace {
std::string xml_get_text(xmlNodePtr node, const char* name) {
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
    }
    return {};
}

std::string xml_get_attr(xmlNodePtr node, const char* attr) {
    auto val = xmlGetProp(node, BAD_CAST attr);
    if (!val) return {};
    std::string r(reinterpret_cast<const char*>(val));
    xmlFree(val);
    return r;
}

// Parse TTML time format: HH:MM:SS.mmm or HH:MM:SS:FF
double parse_ttml_time(const std::string& time_str) {
    if (time_str.empty()) return -1.0;

    double hours = 0, minutes = 0, seconds = 0, frames = 0;
    if (sscanf(time_str.c_str(), "%lf:%lf:%lf:%lf", &hours, &minutes, &seconds, &frames) >= 3) {
        return hours * 3600.0 + minutes * 60.0 + seconds + frames / 24.0;
    }
    if (sscanf(time_str.c_str(), "%lf:%lf:%lf", &hours, &minutes, &seconds) == 3) {
        return hours * 3600.0 + minutes * 60.0 + seconds;
    }
    return -1.0;
}

void collect_ttml_entries(xmlNodePtr node, std::vector<TtmlTimingEntry>& entries) {
    for (auto cur = node; cur; cur = cur->next) {
        if (cur->type == XML_ELEMENT_NODE) {
            std::string name(reinterpret_cast<const char*>(cur->name));
            if (name == "p" || name == "span") {
                TtmlTimingEntry entry;
                entry.begin = xml_get_attr(cur, "begin");
                entry.end = xml_get_attr(cur, "end");
                entry.region = xml_get_attr(cur, "region");
                entry.line_number = cur->line;

                auto content = xmlNodeGetContent(cur);
                if (content) {
                    entry.text_content = reinterpret_cast<const char*>(content);
                    xmlFree(content);
                }
                entries.push_back(std::move(entry));
            }
        }
        collect_ttml_entries(cur->children, entries);
    }
}

} // namespace

TtmlInfo validate_ttml(const fs::path& ttml_path) {
    TtmlInfo info;

    auto doc = xmlReadFile(ttml_path.string().c_str(), nullptr,
                          XML_PARSE_NOERROR | XML_PARSE_NOWARNING | XML_PARSE_NONET);
    if (!doc) {
        info.error = "Failed to parse TTML XML";
        return info;
    }

    auto root = xmlDocGetRootElement(doc);
    if (!root) {
        info.error = "Empty document";
        xmlFreeDoc(doc);
        return info;
    }

    std::string root_name(reinterpret_cast<const char*>(root->name));
    if (root_name != "tt") {
        info.error = "Not a TTML document (root: " + root_name + ")";
        xmlFreeDoc(doc);
        return info;
    }

    // Get profile from namespace or ttp:profile attribute
    auto profile_attr = xml_get_attr(root, "profile");
    if (!profile_attr.empty()) {
        info.profile = profile_attr;
    } else {
        // Check namespace for IMSC
        auto ns = root->ns;
        while (ns) {
            std::string href(reinterpret_cast<const char*>(ns->href));
            if (href.find("imsc") != std::string::npos) {
                info.profile = "imsc1";
                break;
            } else if (href.find("smpte") != std::string::npos) {
                info.profile = "smpte-tt";
                break;
            }
            ns = ns->next;
        }
    }

    // Get language
    info.language = xml_get_attr(root, "lang");
    if (info.language.empty())
        info.language = xml_get_attr(root, "xml:lang");

    // Count regions
    for (auto child = root->children; child; child = child->next) {
        if (child->type == XML_ELEMENT_NODE) {
            std::string name(reinterpret_cast<const char*>(child->name));
            if (name == "head") {
                for (auto hchild = child->children; hchild; hchild = hchild->next) {
                    if (hchild->type == XML_ELEMENT_NODE) {
                        std::string hname(reinterpret_cast<const char*>(hchild->name));
                        if (hname == "layout") {
                            for (auto r = hchild->children; r; r = r->next) {
                                if (r->type == XML_ELEMENT_NODE &&
                                    xmlStrcmp(r->name, BAD_CAST "region") == 0)
                                    info.region_count++;
                            }
                        }
                        if (hname == "styling") info.has_style_refs = true;
                    }
                }
            }
            if (name == "body") {
                collect_ttml_entries(child->children, info.entries);
            }
        }
    }

    info.subtitle_count = info.entries.size();

    // Check timing order
    double prev_end = 0.0;
    for (const auto& entry : info.entries) {
        double begin = parse_ttml_time(entry.begin);
        double end = parse_ttml_time(entry.end);
        if (begin >= 0 && end >= 0 && begin >= end) {
            info.has_timing_errors = true;
            break;
        }
    }

    info.valid = true;
    xmlFreeDoc(doc);
    return info;
}

std::vector<Note> check_imsc_compliance(const TtmlInfo& info,
                                         const fs::path& ttml_path) {
    std::vector<Note> notes;

    if (!info.valid) {
        notes.push_back(Note{Severity::error, Code::subtitle_parse_error,
                        "TTML parse error: " + info.error, ttml_path});
        return notes;
    }

    notes.push_back(Note{Severity::info, Code::subtitle_parse_error,
                    "TTML: " + std::to_string(info.subtitle_count) + " subtitles, profile: " +
                    (info.profile.empty() ? "unknown" : info.profile), ttml_path});

    if (info.has_timing_errors) {
        notes.push_back(Note{Severity::error, Code::subtitle_invalid_timing,
                        "TTML has timing errors (begin >= end)", ttml_path});
    }

    if (info.language.empty()) {
        notes.push_back(Note{Severity::warning, Code::subtitle_parse_error,
                        "TTML missing xml:lang attribute", ttml_path});
    }

    if (info.region_count == 0) {
        notes.push_back(Note{Severity::warning, Code::subtitle_parse_error,
                        "TTML has no region definitions", ttml_path});
    }

    // IMSC-specific checks
    if (info.profile.find("imsc") != std::string::npos) {
        if (info.subtitle_count > 0 && info.region_count == 0) {
            notes.push_back(Note{Severity::error, Code::subtitle_parse_error,
                            "IMSC requires at least one region definition",
                            ttml_path});
        }
    }

    return notes;
}

// ============================================================================
// Dolby Vision 4.0 Metadata
// ============================================================================

DolbyVisionMetadata parse_dolby_vision(const fs::path& mxf_path) {
    DolbyVisionMetadata dv;

    Kumu::FileReaderFactory factory;
    ASDCP::JP2K::MXFReader reader(factory);

    auto result = reader.OpenRead(mxf_path.string());
    if (ASDCP_FAILURE(result)) return dv;

    ASDCP::WriterInfo winfo;
    reader.FillWriterInfo(winfo);

    // Check for DV-specific essence coding labels
    // Dolby Vision Profile 5 uses a specific SubDescriptor UL
    // For detection, check writer product info
    std::string product(reinterpret_cast<const char*>(winfo.ProductName.c_str()));

    if (product.find("Dolby") != std::string::npos) {
        dv.detected = true;

        // Parse version info for profile detection
        if (product.find("Vision") != std::string::npos) {
            dv.rpu_present_flag = 1;

            // Profile detection heuristics based on writer info
            if (product.find("Profile 5") != std::string::npos) dv.profile = 5;
            else if (product.find("Profile 8") != std::string::npos) dv.profile = 8;
            else if (product.find("MEL") != std::string::npos) dv.profile = 5;
            else dv.profile = 8;  // Default to profile 8 (single-layer)

            dv.bl_present_flag = 1;
            dv.is_tunnel = (dv.profile == 5);  // Profile 5 = dual-layer tunnel

            // DV 4.0 uses MEF (Multi-resolution Enhancement Framework)
            if (product.find("4.0") != std::string::npos ||
                product.find("MEF") != std::string::npos) {
                dv.is_mef = true;
            }
        }
    }

    return dv;
}

std::vector<Note> check_dolby_vision_compliance(const DolbyVisionMetadata& dv,
                                                 const fs::path& source) {
    std::vector<Note> notes;
    if (!dv.detected) return notes;

    notes.push_back(Note{Severity::info, Code::mxf_invalid_structure,
                    "Dolby Vision detected: Profile " + std::to_string(dv.profile) +
                    (dv.is_tunnel ? " (dual-layer tunnel)" : " (single-layer)"),
                    source});

    if (dv.is_mef) {
        notes.push_back(Note{Severity::info, Code::mxf_invalid_structure,
                        "Dolby Vision 4.0 MEF (Multi-resolution Enhancement) detected",
                        source});
    }

    // DCI compatibility: only Profile 8 (single-layer) is commonly supported in theatres
    if (dv.profile == 5) {
        notes.push_back(Note{Severity::warning, Code::mxf_invalid_structure,
                        "Dolby Vision Profile 5 (dual-layer) may not be supported by all servers",
                        source});
    }

    if (dv.rpu_present_flag && dv.rpu_count == 0) {
        notes.push_back(Note{Severity::info, Code::mxf_invalid_structure,
                        "Dolby Vision RPU flagged but frame count not available from metadata",
                        source});
    }

    return notes;
}

// ============================================================================
// Dolby Atmos IAB Deep Inspection
// ============================================================================

AtmosIabInfo parse_atmos_iab(const fs::path& mxf_path) {
    AtmosIabInfo info;

    // IAB is carried as DC Data Essence in MXF
    // UL: 06.0e.2b.34.04.01.01.0d.0d.01.03.01.02.16.xx.xx
    Kumu::FileReaderFactory factory;
    ASDCP::PCM::MXFReader reader(factory);

    auto result = reader.OpenRead(mxf_path.string());
    if (ASDCP_FAILURE(result)) return info;

    ASDCP::WriterInfo winfo;
    reader.FillWriterInfo(winfo);

    ASDCP::PCM::AudioDescriptor adesc;
    reader.FillAudioDescriptor(adesc);

    // Detect Atmos based on channel count and writer info
    std::string product(reinterpret_cast<const char*>(winfo.ProductName.c_str()));

    if (product.find("Atmos") != std::string::npos ||
        product.find("Dolby") != std::string::npos) {
        // Check for Atmos-specific indicators
        if (adesc.ChannelCount > 8 || product.find("Atmos") != std::string::npos) {
            info.detected = true;
            info.channel_count = adesc.ChannelCount;
            info.sample_rate = adesc.AudioSamplingRate.Numerator;
            info.bit_depth = adesc.QuantizationBits;
            info.frame_count = adesc.ContainerDuration;
            info.version = product;

            // IAB typically has 10+ objects in an Atmos mix
            // The exact count requires parsing IAB frame headers
            // For now, estimate from channel count
            if (adesc.ChannelCount >= 16) {
                info.object_count = adesc.ChannelCount - 10;  // rough estimate
                info.bed_count = 10;  // 7.1.4 bed = 10 discrete channels
            } else {
                info.bed_count = std::min(uint32_t(10), uint32_t(adesc.ChannelCount));
                info.object_count = (adesc.ChannelCount > 10) ?
                                     adesc.ChannelCount - 10 : 0;
            }
        }
    }

    return info;
}

std::vector<Note> check_atmos_compliance(const AtmosIabInfo& info,
                                          const fs::path& source) {
    std::vector<Note> notes;
    if (!info.detected) return notes;

    std::ostringstream oss;
    oss << "Dolby Atmos IAB: " << info.channel_count << " channels, "
        << info.bed_count << " beds, ~" << info.object_count << " objects";
    notes.push_back(Note{Severity::info, Code::sound_invalid_channel_count,
                    oss.str(), source});

    // ST 2098-2 constraints
    if (info.sample_rate != 48000 && info.sample_rate != 96000) {
        notes.push_back(Note{Severity::warning, Code::sound_invalid_sample_rate,
                        "Atmos IAB sample rate should be 48kHz or 96kHz, got " +
                        std::to_string(int(info.sample_rate)) + "Hz", source});
    }

    if (info.bit_depth != 24) {
        notes.push_back(Note{Severity::warning, Code::sound_invalid_channel_count,
                        "Atmos IAB typically uses 24-bit audio, got " +
                        std::to_string(info.bit_depth) + "-bit", source});
    }

    if (info.object_count > 118) {
        notes.push_back(Note{Severity::error, Code::sound_invalid_channel_count,
                        "Atmos IAB exceeds maximum object count (118), has " +
                        std::to_string(info.object_count), source});
    }

    return notes;
}

// ============================================================================
// HDR Metadata (ST 2098)
// ============================================================================

HdrMetadata detect_hdr_metadata(const fs::path& mxf_path) {
    HdrMetadata hdr;

    Kumu::FileReaderFactory factory;
    ASDCP::JP2K::MXFReader reader(factory);

    auto result = reader.OpenRead(mxf_path.string());
    if (ASDCP_FAILURE(result)) return hdr;

    ASDCP::WriterInfo winfo;
    reader.FillWriterInfo(winfo);

    // Check for HDR-related labeling in writer info and descriptors
    ASDCP::JP2K::PictureDescriptor pdesc;
    reader.FillPictureDescriptor(pdesc);

    // HDR detection heuristics:
    // - High bit depth (12-bit) suggests HDR workflow
    // - Specific color space labels (BT.2020)
    // - MaxCLL/MaxFALL presence in metadata

    std::string product(reinterpret_cast<const char*>(winfo.ProductName.c_str()));

    // Check component bit depth
    if (pdesc.ImageComponents[0].Ssize > 0) {
        uint8_t bit_depth = pdesc.ImageComponents[0].Ssize + 1;
        if (bit_depth >= 12) {
            // 12-bit content is likely HDR or high dynamic range workflow
            hdr.detected = true;
            hdr.type = HdrType::pq;  // Most common for cinema
            hdr.transfer_function = "PQ";
            hdr.color_primaries = "BT.2020";
        }
    }

    // Check product name for HDR indicators
    if (product.find("HDR") != std::string::npos ||
        product.find("PQ") != std::string::npos) {
        hdr.detected = true;
        hdr.type = HdrType::pq;
        hdr.transfer_function = "PQ";
    }
    if (product.find("HLG") != std::string::npos) {
        hdr.detected = true;
        hdr.type = HdrType::hlg;
        hdr.transfer_function = "HLG";
    }

    return hdr;
}

std::vector<Note> check_hdr_compliance(const HdrMetadata& hdr,
                                        const fs::path& source) {
    std::vector<Note> notes;
    if (!hdr.detected) return notes;

    std::string type_str;
    switch (hdr.type) {
    case HdrType::pq: type_str = "PQ (SMPTE ST 2084)"; break;
    case HdrType::hlg: type_str = "HLG (ARIB STD-B67)"; break;
    case HdrType::hdr10: type_str = "HDR10"; break;
    case HdrType::hdr10plus: type_str = "HDR10+"; break;
    case HdrType::dolby_vision: type_str = "Dolby Vision"; break;
    default: type_str = "Unknown"; break;
    }

    notes.push_back(Note{Severity::info, Code::picture_invalid_resolution,
                    "HDR content: " + type_str + " (" + hdr.transfer_function + ")",
                    source});

    if (hdr.color_primaries == "BT.2020") {
        notes.push_back(Note{Severity::info, Code::picture_invalid_resolution,
                        "Wide color gamut: BT.2020", source});
    }

    // DCI theatrical: PQ is the standard transfer function
    if (hdr.type == HdrType::hlg) {
        notes.push_back(Note{Severity::warning, Code::picture_invalid_resolution,
                        "HLG transfer function uncommon for DCI theatrical release",
                        source});
    }

    if (hdr.max_cll > 0) {
        notes.push_back(Note{Severity::info, Code::picture_invalid_resolution,
                        "MaxCLL: " + std::to_string(hdr.max_cll) +
                        " nits, MaxFALL: " + std::to_string(hdr.max_fall) + " nits",
                        source});
    }

    return notes;
}

// ============================================================================
// Netflix Delivery Specification
// ============================================================================

NetflixDeliveryResult check_netflix_delivery(const fs::path& imf_dir) {
    NetflixDeliveryResult result;

    // Netflix requires:
    // 1. IMF App2E profile
    // 2. JPEG 2000 or ProRes encoding
    // 3. Specific frame rates (23.976, 24, 25, 29.97, 50, 59.94)
    // 4. Audio: 48kHz, 24-bit, PCM
    // 5. Proper MCA labels
    // 6. Specific color space metadata
    // 7. ASSETMAP.xml (not ASSETMAP without extension)

    std::error_code ec;

    // Check ASSETMAP naming
    if (fs::exists(imf_dir / "ASSETMAP", ec) && !fs::exists(imf_dir / "ASSETMAP.xml", ec)) {
        result.violations.push_back("Netflix requires ASSETMAP.xml (not ASSETMAP without extension)");
    }

    // Check for CPL with proper Application ID
    for (auto& entry : fs::directory_iterator(imf_dir, ec)) {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ".xml") continue;

        auto doc = xmlReadFile(entry.path().string().c_str(), nullptr,
                              XML_PARSE_NOERROR | XML_PARSE_NOWARNING | XML_PARSE_NONET);
        if (!doc) continue;

        auto root = xmlDocGetRootElement(doc);
        if (!root) { xmlFreeDoc(doc); continue; }

        std::string rn(reinterpret_cast<const char*>(root->name));
        if (rn == "CompositionPlaylist") {
            // Check ApplicationIdentification
            auto app_id = xml_get_text(root->children, "ApplicationIdentification");
            if (app_id.empty()) {
                result.violations.push_back("CPL missing ApplicationIdentification (Netflix requires App2E)");
            } else {
                result.app_id = app_id;
                // Netflix accepts App2E: http://www.smpte-ra.org/schemas/2067-21/2016
                if (app_id.find("2067-21") == std::string::npos &&
                    app_id.find("2067-20") == std::string::npos) {
                    result.violations.push_back("ApplicationIdentification '" + app_id +
                                               "' may not be Netflix-accepted (expected App2E/ST 2067-21)");
                }
            }

            // Check for EditRate
            auto edit_rate = xml_get_text(root->children, "EditRate");
            if (!edit_rate.empty()) {
                // Netflix accepted rates
                static const std::string accepted_rates[] = {
                    "24000 1001", "24 1", "25 1", "30000 1001",
                    "50 1", "60000 1001", "48 1"
                };
                bool rate_ok = false;
                for (const auto& r : accepted_rates) {
                    if (edit_rate.find(r) != std::string::npos) {
                        rate_ok = true;
                        break;
                    }
                }
                if (!rate_ok) {
                    result.violations.push_back("Edit rate '" + edit_rate +
                                               "' not in Netflix accepted rates");
                }
            }
        }

        xmlFreeDoc(doc);
    }

    result.compliant = result.violations.empty();
    return result;
}

std::vector<Note> netflix_to_notes(const NetflixDeliveryResult& result,
                                    const fs::path& source) {
    std::vector<Note> notes;

    if (result.compliant) {
        notes.push_back(Note{Severity::info, Code::missing_assetmap,
                        "Netflix delivery spec: PASS", source});
    } else {
        notes.push_back(Note{Severity::warning, Code::missing_assetmap,
                        "Netflix delivery spec: " + std::to_string(result.violations.size()) +
                        " violation(s)", source});

        for (const auto& v : result.violations) {
            notes.push_back(Note{Severity::warning, Code::missing_assetmap,
                            "[Netflix] " + v, source});
        }
    }

    return notes;
}

// ============================================================================
// ProRes Detection
// ============================================================================

ProResInfo detect_prores(const fs::path& mxf_path) {
    ProResInfo info;

    // ProRes in MXF uses specific essence coding labels
    // UL: 06.0e.2b.34.04.01.01.01.04.01.02.02.03.06.xx.xx (Apple ProRes)
    Kumu::FileReaderFactory factory;
    ASDCP::JP2K::MXFReader reader(factory);

    auto result = reader.OpenRead(mxf_path.string());
    if (ASDCP_FAILURE(result)) {
        // Try reading as generic MXF to check for ProRes
        // ProRes won't open as JP2K, so check writer info from raw file
        return info;
    }

    ASDCP::WriterInfo winfo;
    reader.FillWriterInfo(winfo);

    std::string product(reinterpret_cast<const char*>(winfo.ProductName.c_str()));
    if (product.find("ProRes") != std::string::npos ||
        product.find("Apple") != std::string::npos) {
        info.detected = true;

        if (product.find("4444") != std::string::npos) info.codec_variant = "ProRes 4444";
        else if (product.find("422 HQ") != std::string::npos) info.codec_variant = "ProRes 422 HQ";
        else if (product.find("422") != std::string::npos) info.codec_variant = "ProRes 422";
        else info.codec_variant = "ProRes";

        ASDCP::JP2K::PictureDescriptor pdesc;
        reader.FillPictureDescriptor(pdesc);
        info.width = pdesc.StoredWidth;
        info.height = pdesc.StoredHeight;
        info.frame_rate = double(pdesc.EditRate.Numerator) / double(pdesc.EditRate.Denominator);
    }

    return info;
}

// ============================================================================
// Extended HFR / HBR
// ============================================================================

std::vector<Note> check_extended_hfr(const fs::path& cpl_path) {
    std::vector<Note> notes;

    auto doc = xmlReadFile(cpl_path.string().c_str(), nullptr,
                          XML_PARSE_NOERROR | XML_PARSE_NOWARNING | XML_PARSE_NONET);
    if (!doc) return notes;

    auto root = xmlDocGetRootElement(doc);
    if (!root) { xmlFreeDoc(doc); return notes; }

    auto edit_rate = xml_get_text(root->children, "EditRate");
    if (edit_rate.empty()) { xmlFreeDoc(doc); return notes; }

    // Parse edit rate "N D" format
    int num = 0, den = 1;
    std::istringstream iss(edit_rate);
    iss >> num >> den;
    if (den <= 0) den = 1;
    double fps = double(num) / double(den);

    if (fps > 60.0) {
        notes.push_back(Note{Severity::info, Code::cpl_invalid_edit_rate,
                        "Ultra-HFR content: " + std::to_string(int(fps)) + " fps",
                        cpl_path});

        // 120fps is supported by some next-gen systems
        if (fps > 120.0) {
            notes.push_back(Note{Severity::error, Code::cpl_invalid_edit_rate,
                            "Frame rate " + std::to_string(int(fps)) +
                            " fps exceeds maximum supported rate (120fps)",
                            cpl_path});
        }

        // At 120fps, maximum DCI bitrate applies
        notes.push_back(Note{Severity::info, Code::j2k_bitrate_exceeded,
                        "Ultra-HFR: DCI maximum bitrate is 500 Mbps", cpl_path});
    }

    xmlFreeDoc(doc);
    return notes;
}

// ============================================================================
// Accessibility Track Validation
// ============================================================================

std::vector<Note> check_accessibility(const fs::path& package_dir) {
    std::vector<Note> notes;

    std::error_code ec;
    bool has_audio_desc = false;
    bool has_hi_subtitles = false;
    bool has_closed_captions = false;

    for (auto& entry : fs::directory_iterator(package_dir, ec)) {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ".xml") continue;

        auto doc = xmlReadFile(entry.path().string().c_str(), nullptr,
                              XML_PARSE_NOERROR | XML_PARSE_NOWARNING | XML_PARSE_NONET);
        if (!doc) continue;

        auto root = xmlDocGetRootElement(doc);
        if (!root) { xmlFreeDoc(doc); continue; }

        std::string rn(reinterpret_cast<const char*>(root->name));
        if (rn != "CompositionPlaylist") {
            xmlFreeDoc(doc);
            continue;
        }

        // Search for accessibility markers in CPL
        // Look for MCA labels indicating audio description, HI, VI
        std::function<void(xmlNodePtr)> scan = [&](xmlNodePtr node) {
            for (auto cur = node; cur; cur = cur->next) {
                if (cur->type == XML_ELEMENT_NODE) {
                    std::string name(reinterpret_cast<const char*>(cur->name));

                    // Check for MCA Sound Field labels
                    if (name == "MCATagSymbol" || name == "MCATagName") {
                        auto content = xmlNodeGetContent(cur);
                        if (content) {
                            std::string val(reinterpret_cast<const char*>(content));
                            xmlFree(content);

                            if (val.find("VI") != std::string::npos ||
                                val.find("VisuallyImpaired") != std::string::npos ||
                                val.find("AudioDescription") != std::string::npos) {
                                has_audio_desc = true;
                            }
                            if (val.find("HI") != std::string::npos ||
                                val.find("HearingImpaired") != std::string::npos) {
                                has_hi_subtitles = true;
                            }
                        }
                    }

                    // Check for closed caption assets
                    if (name == "MainClosedCaption" || name == "ClosedCaption") {
                        has_closed_captions = true;
                    }

                    // Check subtitle annotation for HI/SDH indicators
                    if (name == "AnnotationText" || name == "ContentTitleText") {
                        auto content = xmlNodeGetContent(cur);
                        if (content) {
                            std::string val(reinterpret_cast<const char*>(content));
                            xmlFree(content);
                            if (val.find("-HI") != std::string::npos ||
                                val.find("_HI") != std::string::npos) {
                                has_hi_subtitles = true;
                            }
                        }
                    }
                }
                scan(cur->children);
            }
        };

        scan(root->children);
        xmlFreeDoc(doc);
    }

    // Report findings
    if (has_audio_desc) {
        notes.push_back(Note{Severity::info, Code::sound_invalid_channel_count,
                        "Accessibility: Audio Description (VI) track present",
                        package_dir});
    }
    if (has_hi_subtitles) {
        notes.push_back(Note{Severity::info, Code::subtitle_parse_error,
                        "Accessibility: Hearing Impaired (HI) subtitles present",
                        package_dir});
    }
    if (has_closed_captions) {
        notes.push_back(Note{Severity::info, Code::subtitle_parse_error,
                        "Accessibility: Closed Captions present", package_dir});
    }

    if (!has_audio_desc && !has_hi_subtitles && !has_closed_captions) {
        notes.push_back(Note{Severity::info, Code::subtitle_parse_error,
                        "No accessibility tracks detected (AD/HI/CC)", package_dir});
    }

    return notes;
}

// ============================================================================
// Content Fingerprinting
// ============================================================================

ContentFingerprint generate_fingerprint(const fs::path& mxf_path) {
    ContentFingerprint fp;

    Kumu::FileReaderFactory factory;
    ASDCP::JP2K::MXFReader reader(factory);

    auto result = reader.OpenRead(mxf_path.string());
    if (ASDCP_FAILURE(result)) return fp;

    ASDCP::JP2K::PictureDescriptor pdesc;
    reader.FillPictureDescriptor(pdesc);

    fp.width = pdesc.StoredWidth;
    fp.height = pdesc.StoredHeight;

    // Read first frame codestream for fingerprint
    ASDCP::JP2K::FrameBuffer frame_buf(1024 * 1024 * 4);  // 4MB buffer
    result = reader.ReadFrame(0, frame_buf);
    if (ASDCP_FAILURE(result)) return fp;

    // Generate SHA-256 of first frame as a simple fingerprint
    // (A real perceptual hash would decode and compute pHash, but that
    //  requires a J2K decoder which we don't want to bundle)
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(frame_buf.RoData(), frame_buf.Size(), hash);

    // Convert to hex string
    std::ostringstream oss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i)
        oss << std::hex << std::setfill('0') << std::setw(2) << int(hash[i]);

    fp.hash = oss.str();
    fp.frame_sampled = 0;

    return fp;
}

double compare_fingerprints(const ContentFingerprint& a, const ContentFingerprint& b) {
    if (a.hash.empty() || b.hash.empty()) return 1.0;
    if (a.hash == b.hash) return 0.0;

    // For SHA-based fingerprints, any difference means different content
    // A real perceptual hash would return hamming distance / bits
    return 1.0;
}

} // namespace dcpdoctor
