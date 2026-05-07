#include "dcpdoctor/advanced.h"
#include <libxml/parser.h>
#include <fstream>
#include <set>
#include <sstream>
#include <iomanip>

namespace dcpdoctor {
namespace fs = std::filesystem;
namespace {

std::string find_element_text(xmlNodePtr root, const char* name) {
    // Recursive search for element text
    for (auto node = root->children; node; node = node->next) {
        if (node->type == XML_ELEMENT_NODE &&
            xmlStrcmp(node->name, BAD_CAST name) == 0) {
            auto content = xmlNodeGetContent(node);
            if (content) {
                std::string r(reinterpret_cast<const char*>(content));
                xmlFree(content);
                return r;
            }
        }
        auto result = find_element_text(node, name);
        if (!result.empty()) return result;
    }
    return {};
}

void find_all_elements(xmlNodePtr node, const char* name, std::vector<xmlNodePtr>& out) {
    for (auto cur = node; cur; cur = cur->next) {
        if (cur->type == XML_ELEMENT_NODE &&
            xmlStrcmp(cur->name, BAD_CAST name) == 0)
            out.push_back(cur);
        find_all_elements(cur->children, name, out);
    }
}

} // namespace

std::vector<Note> check_bv21_compliance(const fs::path& dcp_dir, Standard standard) {
    std::vector<Note> notes;

    if (standard != Standard::smpte) {
        notes.push_back({Severity::warning, Code::smpte_namespace_wrong,
                        "BV2.1 requires SMPTE standard; this DCP uses Interop", dcp_dir});
        return notes;
    }

    // BV2.1 specific checks:
    // 1. ASSETMAP must be named ASSETMAP.xml (not ASSETMAP)
    if (!fs::exists(dcp_dir / "ASSETMAP.xml")) {
        notes.push_back({Severity::error, Code::smpte_naming_violation,
                        "BV2.1 requires ASSETMAP.xml filename", dcp_dir});
    }

    // 2. PKL must have .xml extension
    std::error_code ec;
    for (auto& entry : fs::directory_iterator(dcp_dir, ec)) {
        auto fname = entry.path().filename().string();
        if (fname.find("PKL") != std::string::npos || fname.find("pkl") != std::string::npos) {
            if (entry.path().extension() != ".xml") {
                notes.push_back({Severity::warning, Code::smpte_naming_violation,
                                "BV2.1: PKL file should have .xml extension: " + fname,
                                entry.path()});
            }
        }
    }

    // 3. CPL checks
    for (auto& entry : fs::directory_iterator(dcp_dir, ec)) {
        if (!entry.is_regular_file()) continue;
        auto ext = entry.path().extension().string();
        if (ext != ".xml") continue;

        auto doc = xmlReadFile(entry.path().string().c_str(), nullptr,
                              XML_PARSE_NOERROR | XML_PARSE_NOWARNING | XML_PARSE_NONET);
        if (!doc) continue;

        auto root = xmlDocGetRootElement(doc);
        if (!root) { xmlFreeDoc(doc); continue; }

        std::string root_name(reinterpret_cast<const char*>(root->name));
        if (root_name != "CompositionPlaylist") {
            xmlFreeDoc(doc);
            continue;
        }

        // BV2.1 requires ContentVersion element
        auto cv = find_element_text(root, "ContentVersion");
        if (cv.empty()) {
            // Check for ContentVersion element (may have sub-elements)
            std::vector<xmlNodePtr> cv_nodes;
            find_all_elements(root, "ContentVersion", cv_nodes);
            if (cv_nodes.empty()) {
                notes.push_back({Severity::warning, Code::missing_required_element,
                                "BV2.1 requires ContentVersion in CPL", entry.path()});
            }
        }

        // BV2.1 requires ExtensionMetadata with application scope
        std::vector<xmlNodePtr> ext_meta;
        find_all_elements(root, "ExtensionMetadata", ext_meta);
        if (ext_meta.empty()) {
            notes.push_back({Severity::info, Code::missing_required_element,
                            "BV2.1 recommends ExtensionMetadata in CPL", entry.path()});
        }

        // BV2.1 requires MainMarkers in first reel
        std::vector<xmlNodePtr> reels;
        find_all_elements(root, "Reel", reels);
        if (!reels.empty()) {
            std::vector<xmlNodePtr> markers;
            find_all_elements(reels[0], "MainMarkers", markers);
            if (markers.empty()) {
                notes.push_back({Severity::warning, Code::marker_missing,
                                "BV2.1 requires MainMarkers in first reel", entry.path()});
            }
        }

        // BV2.1: EditRate must be one of the approved rates
        auto edit_rate_str = find_element_text(root, "EditRate");
        if (!edit_rate_str.empty()) {
            // Parse "num den" format
            std::istringstream iss(edit_rate_str);
            int num = 0, den = 1;
            iss >> num >> den;
            if (den > 0) {
                double fps = double(num) / double(den);
                bool valid_bv21_rate = (fps == 24.0 || fps == 25.0 || fps == 30.0 ||
                                       fps == 48.0 || fps == 60.0);
                if (!valid_bv21_rate) {
                    notes.push_back({Severity::warning, Code::cpl_invalid_edit_rate,
                                    "BV2.1: EditRate " + edit_rate_str +
                                    " is not an approved rate", entry.path()});
                }
            }
        }

        xmlFreeDoc(doc);
    }

    return notes;
}

std::vector<Note> compare_manifest(const fs::path& dcp_dir,
                                    const fs::path& manifest_path) {
    std::vector<Note> notes;

    // Read manifest JSON (simple parser for {"assets": [{"filename": "...", "size": N, "hash": "..."}]})
    std::ifstream f(manifest_path);
    if (!f) {
        notes.push_back({Severity::error, Code::asset_not_found,
                        "Cannot open manifest file: " + manifest_path.string(),
                        manifest_path});
        return notes;
    }

    std::string content((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());

    // Simple JSON array parsing for filenames
    // Look for "filename" : "value" patterns
    size_t pos = 0;
    while ((pos = content.find("\"filename\"", pos)) != std::string::npos) {
        auto colon = content.find(':', pos);
        if (colon == std::string::npos) break;
        auto quote1 = content.find('"', colon + 1);
        auto quote2 = content.find('"', quote1 + 1);
        if (quote1 == std::string::npos || quote2 == std::string::npos) break;

        std::string filename = content.substr(quote1 + 1, quote2 - quote1 - 1);

        // Check file exists in DCP
        auto full_path = dcp_dir / filename;
        if (!fs::exists(full_path)) {
            notes.push_back({Severity::error, Code::asset_not_found,
                            "Manifest asset not found in DCP: " + filename,
                            dcp_dir});
        }

        // Check for "size" field
        auto size_pos = content.find("\"size\"", quote2);
        if (size_pos != std::string::npos && size_pos < content.find('}', quote2)) {
            auto scolon = content.find(':', size_pos);
            if (scolon != std::string::npos) {
                auto sval_start = content.find_first_of("0123456789", scolon);
                if (sval_start != std::string::npos) {
                    uint64_t expected_size = std::stoull(content.substr(sval_start));
                    if (fs::exists(full_path)) {
                        auto actual_size = fs::file_size(full_path);
                        if (actual_size != expected_size) {
                            notes.push_back({Severity::error, Code::pkl_hash_mismatch,
                                            "Size mismatch for " + filename +
                                            ": expected " + std::to_string(expected_size) +
                                            ", got " + std::to_string(actual_size),
                                            full_path});
                        }
                    }
                }
            }
        }

        pos = quote2 + 1;
    }

    // Check for extra files in DCP not in manifest
    std::set<std::string> manifest_files;
    pos = 0;
    while ((pos = content.find("\"filename\"", pos)) != std::string::npos) {
        auto colon = content.find(':', pos);
        auto quote1 = content.find('"', colon + 1);
        auto quote2 = content.find('"', quote1 + 1);
        if (quote1 != std::string::npos && quote2 != std::string::npos)
            manifest_files.insert(content.substr(quote1 + 1, quote2 - quote1 - 1));
        pos = (quote2 != std::string::npos) ? quote2 + 1 : content.size();
    }

    std::error_code ec;
    for (auto& entry : fs::directory_iterator(dcp_dir, ec)) {
        if (!entry.is_regular_file()) continue;
        auto fname = entry.path().filename().string();
        if (!manifest_files.contains(fname)) {
            notes.push_back(Note{Severity::info, Code::asset_not_found,
                            "File in DCP not listed in manifest: " + fname,
                            entry.path()});
        }
    }

    return notes;
}

void write_batch_summary(std::ostream& out, const std::vector<BatchResult>& results) {
    out << "DcpDoctor Batch Summary\n";
    out << "=======================\n\n";

    int total = results.size();
    int passed = 0;
    for (const auto& r : results)
        if (r.passed) ++passed;

    out << "Total: " << total << "  Passed: " << passed
        << "  Failed: " << (total - passed) << "\n\n";

    // Table header
    out << std::left << std::setw(50) << "DCP Path"
        << std::setw(10) << "Status"
        << std::setw(10) << "Errors"
        << std::setw(10) << "Warnings"
        << "Standard\n";
    out << std::string(90, '-') << "\n";

    for (const auto& r : results) {
        std::string path_str = r.dcp_path.filename().string();
        if (path_str.size() > 48) path_str = path_str.substr(0, 45) + "...";

        out << std::left << std::setw(50) << path_str
            << std::setw(10) << (r.passed ? "PASS" : "FAIL")
            << std::setw(10) << r.errors
            << std::setw(10) << r.warnings
            << (r.standard == Standard::smpte ? "SMPTE" :
                r.standard == Standard::interop ? "Interop" : "Unknown")
            << "\n";
    }
}

} // namespace dcpdoctor
