#include "dcpdoctor/validators.h"
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include <algorithm>
#include <set>
#include <string>

namespace dcpdoctor {
namespace {

// Helper to get text content of first matching child element
std::string get_child_text(xmlNodePtr parent, const char* name) {
    for (auto child = parent->children; child; child = child->next) {
        if (child->type == XML_ELEMENT_NODE &&
            xmlStrcmp(child->name, BAD_CAST name) == 0) {
            auto content = xmlNodeGetContent(child);
            if (content) {
                std::string result(reinterpret_cast<const char*>(content));
                xmlFree(content);
                return result;
            }
        }
    }
    return {};
}

// Find all elements with a given local name (ignoring namespace)
void find_elements(xmlNodePtr node, const char* name, std::vector<xmlNodePtr>& out) {
    for (auto cur = node; cur; cur = cur->next) {
        if (cur->type == XML_ELEMENT_NODE &&
            xmlStrcmp(cur->name, BAD_CAST name) == 0) {
            out.push_back(cur);
        }
        find_elements(cur->children, name, out);
    }
}

} // namespace

// --- Encryption Detection ---
std::vector<Note> check_encryption(const std::filesystem::path& dcp_dir,
                                    const std::vector<std::filesystem::path>& cpl_paths) {
    std::vector<Note> notes;
    namespace fs = std::filesystem;

    for (const auto& cpl_path : cpl_paths) {
        auto doc = xmlReadFile(cpl_path.c_str(), nullptr,
                              XML_PARSE_NOERROR | XML_PARSE_NOWARNING | XML_PARSE_NONET);
        if (!doc) continue;

        auto root = xmlDocGetRootElement(doc);
        if (!root) { xmlFreeDoc(doc); continue; }

        // Look for EncryptedDocumentKey or KeyId elements
        std::vector<xmlNodePtr> key_ids;
        find_elements(root, "KeyId", key_ids);

        std::vector<xmlNodePtr> enc_keys;
        find_elements(root, "EncryptedDocumentKey", enc_keys);

        if (!key_ids.empty() || !enc_keys.empty()) {
            notes.push_back({Severity::info, Code::encryption_detected,
                            "CPL contains encrypted content (" +
                            std::to_string(key_ids.size()) + " encrypted assets)",
                            cpl_path});

            // Check if KDM is present in the DCP directory
            bool kdm_found = false;
            std::error_code ec;
            for (auto& entry : fs::directory_iterator(dcp_dir, ec)) {
                auto fname = entry.path().filename().string();
                // KDM files typically named with "KDM" or have specific pattern
                if (fname.find("KDM") != std::string::npos ||
                    fname.find("kdm") != std::string::npos) {
                    kdm_found = true;
                    break;
                }
            }
            if (!kdm_found) {
                notes.push_back({Severity::info, Code::kdm_required,
                                "No KDM file found in DCP directory for encrypted content",
                                cpl_path});
            }
        }

        xmlFreeDoc(doc);
    }

    return notes;
}

// --- Reel Continuity ---
std::vector<Note> check_reel_continuity(const std::filesystem::path& cpl_path) {
    std::vector<Note> notes;

    auto doc = xmlReadFile(cpl_path.c_str(), nullptr,
                          XML_PARSE_NOERROR | XML_PARSE_NOWARNING | XML_PARSE_NONET);
    if (!doc) return notes;

    auto root = xmlDocGetRootElement(doc);
    if (!root) { xmlFreeDoc(doc); return notes; }

    // Find all Reel elements
    std::vector<xmlNodePtr> reels;
    find_elements(root, "Reel", reels);

    if (reels.size() < 2) {
        xmlFreeDoc(doc);
        return notes;  // Single reel, nothing to check
    }

    // For each reel, track EntryPoint + Duration to verify continuity
    uint64_t expected_entry = 0;
    int reel_num = 0;

    for (auto reel_node : reels) {
        ++reel_num;

        // Find MainPicture/MainImage in the reel
        std::vector<xmlNodePtr> pictures;
        find_elements(reel_node, "MainPicture", pictures);
        if (pictures.empty())
            find_elements(reel_node, "MainImage", pictures);

        if (pictures.empty()) continue;

        auto pic = pictures[0];
        std::string entry_str = get_child_text(pic, "EntryPoint");
        std::string duration_str = get_child_text(pic, "Duration");

        uint64_t entry_point = 0;
        uint64_t duration = 0;

        if (!entry_str.empty()) entry_point = std::stoull(entry_str);
        if (!duration_str.empty()) duration = std::stoull(duration_str);

        // Check entry point matches expected
        if (reel_num > 1 && entry_point != 0 && entry_point != expected_entry) {
            notes.push_back({Severity::warning, Code::reel_discontinuity,
                            "Reel " + std::to_string(reel_num) +
                            " EntryPoint " + std::to_string(entry_point) +
                            " does not follow previous reel (expected " +
                            std::to_string(expected_entry) + ")",
                            cpl_path});
        }

        expected_entry = entry_point + duration;
    }

    xmlFreeDoc(doc);
    return notes;
}

// --- 3D Stereoscopic ---
std::vector<Note> check_stereo(const std::filesystem::path& cpl_path) {
    std::vector<Note> notes;

    auto doc = xmlReadFile(cpl_path.c_str(), nullptr,
                          XML_PARSE_NOERROR | XML_PARSE_NOWARNING | XML_PARSE_NONET);
    if (!doc) return notes;

    auto root = xmlDocGetRootElement(doc);
    if (!root) { xmlFreeDoc(doc); return notes; }

    // Check for MainStereoscopicPicture elements
    std::vector<xmlNodePtr> stereo;
    find_elements(root, "MainStereoscopicPicture", stereo);

    if (stereo.empty()) {
        xmlFreeDoc(doc);
        return notes;  // Not a 3D DCP
    }

    // For each stereoscopic reel, verify left/right eye presence
    for (size_t i = 0; i < stereo.size(); ++i) {
        auto node = stereo[i];

        // Check for LeftEye and RightEye sub-elements (Interop)
        std::vector<xmlNodePtr> left_eyes, right_eyes;
        find_elements(node, "LeftEye", left_eyes);
        find_elements(node, "RightEye", right_eyes);

        if (!left_eyes.empty() || !right_eyes.empty()) {
            // Interop 3D - check both eyes present
            if (left_eyes.empty()) {
                notes.push_back({Severity::error, Code::stereo_mismatch,
                                "Stereoscopic reel " + std::to_string(i + 1) +
                                " missing LeftEye", cpl_path});
            }
            if (right_eyes.empty()) {
                notes.push_back({Severity::error, Code::stereo_mismatch,
                                "Stereoscopic reel " + std::to_string(i + 1) +
                                " missing RightEye", cpl_path});
            }
        }

        // Check duration consistency
        std::string duration = get_child_text(node, "Duration");
        std::string intrinsic = get_child_text(node, "IntrinsicDuration");
        if (!duration.empty() && !intrinsic.empty()) {
            if (std::stoull(duration) > std::stoull(intrinsic)) {
                notes.push_back({Severity::error, Code::stereo_mismatch,
                                "Stereoscopic reel " + std::to_string(i + 1) +
                                " Duration exceeds IntrinsicDuration", cpl_path});
            }
        }
    }

    xmlFreeDoc(doc);
    return notes;
}

// --- Marker Validation ---
std::vector<Note> check_markers(const std::filesystem::path& cpl_path, bool strict) {
    std::vector<Note> notes;

    auto doc = xmlReadFile(cpl_path.c_str(), nullptr,
                          XML_PARSE_NOERROR | XML_PARSE_NOWARNING | XML_PARSE_NONET);
    if (!doc) return notes;

    auto root = xmlDocGetRootElement(doc);
    if (!root) { xmlFreeDoc(doc); return notes; }

    // Find all MarkerAsset/MainMarkers elements
    std::vector<xmlNodePtr> markers;
    find_elements(root, "MainMarkers", markers);
    if (markers.empty())
        find_elements(root, "MarkerAsset", markers);

    // Collect all marker labels across reels
    std::set<std::string> found_markers;
    for (auto marker_node : markers) {
        std::vector<xmlNodePtr> marker_list;
        find_elements(marker_node, "Marker", marker_list);
        for (auto m : marker_list) {
            // Marker has Label and Offset children
            std::string label = get_child_text(m, "Label");
            if (!label.empty()) {
                found_markers.insert(label);
            }

            // Check Offset is present and valid
            std::string offset = get_child_text(m, "Offset");
            if (offset.empty()) {
                notes.push_back({Severity::warning, Code::marker_invalid,
                                "Marker '" + label + "' missing Offset", cpl_path});
            }
        }
    }

    // Required markers for compliant DCP (SMPTE 429-7):
    // FFMC (First Frame of Moving Content)
    // LFMC (Last Frame of Moving Content)
    if (strict && !markers.empty()) {
        static const std::vector<std::pair<std::string, std::string>> required_markers = {
            {"FFMC", "First Frame of Moving Content"},
            {"LFMC", "Last Frame of Moving Content"},
        };

        for (const auto& [label, desc] : required_markers) {
            if (!found_markers.contains(label)) {
                notes.push_back({Severity::warning, Code::marker_missing,
                                "Required marker missing: " + label + " (" + desc + ")",
                                cpl_path});
            }
        }

        // Recommended markers
        static const std::vector<std::pair<std::string, std::string>> recommended_markers = {
            {"FFTC", "First Frame of Title Credits"},
            {"LFTC", "Last Frame of Title Credits"},
            {"FFOI", "First Frame of Intermission"},
            {"LFOI", "Last Frame of Intermission"},
            {"FFEC", "First Frame of End Credits"},
            {"LFEC", "Last Frame of End Credits"},
        };

        for (const auto& [label, desc] : recommended_markers) {
            if (!found_markers.contains(label)) {
                notes.push_back({Severity::info, Code::marker_missing,
                                "Recommended marker not present: " + label + " (" + desc + ")",
                                cpl_path});
            }
        }
    }

    xmlFreeDoc(doc);
    return notes;
}

// --- Cross-Reference Integrity ---
std::vector<Note> check_cross_references(const std::filesystem::path& dcp_dir,
                                          const std::vector<std::string>& known_asset_ids,
                                          const std::vector<std::filesystem::path>& cpl_paths) {
    std::vector<Note> notes;

    // Normalize known IDs: strip urn:uuid: prefix for matching
    std::set<std::string> known_ids;
    for (const auto& id : known_asset_ids) {
        if (id.starts_with("urn:uuid:"))
            known_ids.insert(id.substr(9));
        else
            known_ids.insert(id);
    }

    for (const auto& cpl_path : cpl_paths) {
        auto doc = xmlReadFile(cpl_path.c_str(), nullptr,
                              XML_PARSE_NOERROR | XML_PARSE_NOWARNING | XML_PARSE_NONET);
        if (!doc) continue;

        auto root = xmlDocGetRootElement(doc);
        if (!root) { xmlFreeDoc(doc); continue; }

        // Find all Id elements within asset references
        std::vector<xmlNodePtr> id_nodes;
        // Look for specific asset reference patterns
        const char* asset_id_elements[] = {
            "MainPicture", "MainSound", "MainSubtitle",
            "MainStereoscopicPicture", "MainMarkers",
            "ClosedCaption", "MainImage", "AuxData"
        };

        for (const char* elem_name : asset_id_elements) {
            std::vector<xmlNodePtr> elems;
            find_elements(root, elem_name, elems);
            for (auto elem : elems) {
                std::string id = get_child_text(elem, "Id");
                if (!id.empty()) {
                    // Strip urn:uuid: prefix if present
                    if (id.starts_with("urn:uuid:"))
                        id = id.substr(9);

                    if (!known_ids.contains(id)) {
                        notes.push_back({Severity::error, Code::cross_ref_broken,
                                        "CPL references asset " + id +
                                        " not found in ASSETMAP/PKL", cpl_path});
                    }
                }
            }
        }

        xmlFreeDoc(doc);
    }

    return notes;
}

// --- Supplemental DCP ---
std::vector<Note> check_supplemental(const std::filesystem::path& dcp_dir,
                                      const std::vector<std::filesystem::path>& cpl_paths) {
    std::vector<Note> notes;
    namespace fs = std::filesystem;

    for (const auto& cpl_path : cpl_paths) {
        auto doc = xmlReadFile(cpl_path.c_str(), nullptr,
                              XML_PARSE_NOERROR | XML_PARSE_NOWARNING | XML_PARSE_NONET);
        if (!doc) continue;

        auto root = xmlDocGetRootElement(doc);
        if (!root) { xmlFreeDoc(doc); continue; }

        // Check for IssueDate and ContentVersion to identify if this is supplemental
        std::vector<xmlNodePtr> opl_nodes;
        find_elements(root, "OPL", opl_nodes);
        // Also check for ReferencedCPL in ExtensionMetadata
        std::vector<xmlNodePtr> ref_cpls;
        find_elements(root, "OriginalPackagingList", ref_cpls);

        // Check if CPL references external assets via OriginalFileName
        std::vector<xmlNodePtr> ext_refs;
        find_elements(root, "OriginalFileName", ext_refs);

        if (!ref_cpls.empty() || !opl_nodes.empty()) {
            // This is a supplemental/version file DCP
            notes.push_back({Severity::info, Code::supplemental_opl_missing,
                            "CPL appears to be a supplemental/version file package",
                            cpl_path});
        }

        xmlFreeDoc(doc);
    }

    return notes;
}

// --- Audio Channel Labeling ---
std::vector<Note> check_audio_channels(const std::filesystem::path& cpl_path) {
    std::vector<Note> notes;

    auto doc = xmlReadFile(cpl_path.c_str(), nullptr,
                          XML_PARSE_NOERROR | XML_PARSE_NOWARNING | XML_PARSE_NONET);
    if (!doc) return notes;

    auto root = xmlDocGetRootElement(doc);
    if (!root) { xmlFreeDoc(doc); return notes; }

    // Find MainSound elements and check channel configuration
    std::vector<xmlNodePtr> sounds;
    find_elements(root, "MainSound", sounds);
    if (sounds.empty())
        find_elements(root, "MainAudio", sounds);

    for (size_t i = 0; i < sounds.size(); ++i) {
        // Check for MCASubDescriptor / AudioChannelLabelSubDescriptor
        std::vector<xmlNodePtr> mca;
        find_elements(sounds[i], "MCALabelDictionaryId", mca);

        // If no MCA labeling but has multiple channels, warn
        if (mca.empty()) {
            // Check for SoundFieldGroupLabelSubDescriptor
            std::vector<xmlNodePtr> sfg;
            find_elements(sounds[i], "SoundFieldGroupLinkId", sfg);
            if (sfg.empty()) {
                notes.push_back({Severity::info, Code::sound_invalid_channel_count,
                                "Reel " + std::to_string(i + 1) +
                                " sound has no MCA channel labeling metadata",
                                cpl_path});
            }
        }
    }

    xmlFreeDoc(doc);
    return notes;
}

// --- Color Space ---
std::vector<Note> check_color_space(const std::filesystem::path& cpl_path) {
    std::vector<Note> notes;

    auto doc = xmlReadFile(cpl_path.c_str(), nullptr,
                          XML_PARSE_NOERROR | XML_PARSE_NOWARNING | XML_PARSE_NONET);
    if (!doc) return notes;

    auto root = xmlDocGetRootElement(doc);
    if (!root) { xmlFreeDoc(doc); return notes; }

    // DCI requires XYZ color space (CIE 1931 XYZ with DCI white point)
    // Check for any color space indicators in CPL metadata

    // Look for TransferCharacteristic or CodingEquations
    std::vector<xmlNodePtr> tc_nodes;
    find_elements(root, "TransferCharacteristic", tc_nodes);

    std::vector<xmlNodePtr> ce_nodes;
    find_elements(root, "CodingEquations", ce_nodes);

    // Check MainPicture elements for color info
    std::vector<xmlNodePtr> pictures;
    find_elements(root, "MainPicture", pictures);
    if (pictures.empty())
        find_elements(root, "MainImage", pictures);

    // If color metadata is present, verify it indicates XYZ
    for (auto tc : tc_nodes) {
        auto content = xmlNodeGetContent(tc);
        if (content) {
            std::string val(reinterpret_cast<const char*>(content));
            xmlFree(content);
            // DCI XYZ uses gamma 2.6
            if (val.find("2.6") == std::string::npos &&
                val.find("XYZ") == std::string::npos) {
                notes.push_back({Severity::info, Code::picture_invalid_resolution,
                                "TransferCharacteristic indicates non-DCI color: " + val,
                                cpl_path});
            }
        }
    }

    xmlFreeDoc(doc);
    return notes;
}

} // namespace dcpdoctor
