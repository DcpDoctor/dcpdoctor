#include "dcpdoctor/subtitle.h"
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include <set>
#include <string>

namespace dcpdoctor {
namespace {

// SMPTE 428-7 namespace
constexpr const char* SMPTE_SUBTITLE_NS = "http://www.smpte-ra.org/schemas/428-7/2010/DCST";
// Interop CineCanvas namespace
constexpr const char* INTEROP_SUBTITLE_NS = "http://www.digicine.com/PROTO-ASDCP-AM-20040311#";

struct SubtitleContext {
    Standard standard;
    std::filesystem::path path;
    std::vector<Note> notes;
    std::set<std::string> referenced_fonts;
    std::set<std::string> declared_fonts;
};

void check_subtitle_timing(SubtitleContext& ctx, xmlNodePtr node) {
    // Check TimeIn/TimeOut attributes
    auto time_in = xmlGetProp(node, BAD_CAST "TimeIn");
    auto time_out = xmlGetProp(node, BAD_CAST "TimeOut");

    if (!time_in) {
        ctx.notes.push_back({Severity::error, Code::subtitle_invalid_timing,
                            "Subtitle element missing TimeIn attribute", ctx.path});
    }
    if (!time_out) {
        ctx.notes.push_back({Severity::error, Code::subtitle_invalid_timing,
                            "Subtitle element missing TimeOut attribute", ctx.path});
    }

    if (time_in && time_out) {
        // Basic format check: HH:MM:SS:FF or HH:MM:SS.mmm
        std::string tin(reinterpret_cast<const char*>(time_in));
        std::string tout(reinterpret_cast<const char*>(time_out));

        auto valid_timecode = [](const std::string& tc) {
            if (tc.size() < 11) return false;
            // HH:MM:SS:FF or HH:MM:SS.mmm
            return (tc[2] == ':' && tc[5] == ':' &&
                    (tc[8] == ':' || tc[8] == '.'));
        };

        if (!valid_timecode(tin)) {
            ctx.notes.push_back({Severity::error, Code::subtitle_invalid_timing,
                                "Invalid TimeIn format: " + tin, ctx.path});
        }
        if (!valid_timecode(tout)) {
            ctx.notes.push_back({Severity::error, Code::subtitle_invalid_timing,
                                "Invalid TimeOut format: " + tout, ctx.path});
        }

        // Check TimeOut > TimeIn (simple string comparison works for timecodes)
        if (valid_timecode(tin) && valid_timecode(tout) && tout <= tin) {
            ctx.notes.push_back({Severity::warning, Code::subtitle_invalid_timing,
                                "TimeOut (" + tout + ") not after TimeIn (" + tin + ")",
                                ctx.path});
        }
    }

    if (time_in) xmlFree(time_in);
    if (time_out) xmlFree(time_out);
}

void walk_subtitles(SubtitleContext& ctx, xmlNodePtr node) {
    for (auto cur = node; cur; cur = cur->next) {
        if (cur->type != XML_ELEMENT_NODE) continue;

        std::string name(reinterpret_cast<const char*>(cur->name));

        if (name == "Font") {
            // Check for font ID declaration (LoadFont) vs reference
            auto id = xmlGetProp(cur, BAD_CAST "Id");
            if (id) {
                ctx.referenced_fonts.insert(reinterpret_cast<const char*>(id));
                xmlFree(id);
            }
            // Also check Script attribute
            auto script = xmlGetProp(cur, BAD_CAST "Script");
            if (script) xmlFree(script);
        } else if (name == "LoadFont") {
            auto id = xmlGetProp(cur, BAD_CAST "Id");
            if (id) {
                ctx.declared_fonts.insert(reinterpret_cast<const char*>(id));
                xmlFree(id);
            } else {
                // SMPTE uses ID, Interop uses Id
                id = xmlGetProp(cur, BAD_CAST "ID");
                if (id) {
                    ctx.declared_fonts.insert(reinterpret_cast<const char*>(id));
                    xmlFree(id);
                }
            }
        } else if (name == "Subtitle") {
            check_subtitle_timing(ctx, cur);
        }

        // Recurse into children
        walk_subtitles(ctx, cur->children);
    }
}

} // namespace

std::vector<Note> validate_subtitle(const std::filesystem::path& xml_path,
                                     Standard standard) {
    SubtitleContext ctx;
    ctx.standard = standard;
    ctx.path = xml_path;

    auto doc = xmlReadFile(xml_path.string().c_str(), nullptr,
                          XML_PARSE_NOERROR | XML_PARSE_NOWARNING | XML_PARSE_NONET);
    if (!doc) {
        ctx.notes.push_back({Severity::error, Code::subtitle_parse_error,
                            "Failed to parse subtitle XML", xml_path});
        return ctx.notes;
    }

    auto root = xmlDocGetRootElement(doc);
    if (!root) {
        ctx.notes.push_back({Severity::error, Code::subtitle_parse_error,
                            "Empty subtitle document", xml_path});
        xmlFreeDoc(doc);
        return ctx.notes;
    }

    // Check root element name
    std::string root_name(reinterpret_cast<const char*>(root->name));
    if (root_name != "SubtitleReel" && root_name != "DCSubtitle") {
        ctx.notes.push_back({Severity::error, Code::subtitle_parse_error,
                            "Unexpected root element: " + root_name +
                            " (expected SubtitleReel or DCSubtitle)", xml_path});
        xmlFreeDoc(doc);
        return ctx.notes;
    }

    // Check namespace
    if (root->ns && root->ns->href) {
        std::string ns(reinterpret_cast<const char*>(root->ns->href));
        if (standard == Standard::smpte && ns != SMPTE_SUBTITLE_NS) {
            ctx.notes.push_back({Severity::warning, Code::smpte_namespace_wrong,
                                "Subtitle namespace should be SMPTE 428-7: " + ns, xml_path});
        } else if (standard == Standard::interop && ns != INTEROP_SUBTITLE_NS) {
            ctx.notes.push_back({Severity::warning, Code::interop_namespace_wrong,
                                "Subtitle namespace should be Interop: " + ns, xml_path});
        }
    }

    // Check required elements
    bool has_id = false, has_edit_rate = false, has_time_code_rate = false;
    for (auto child = root->children; child; child = child->next) {
        if (child->type != XML_ELEMENT_NODE) continue;
        std::string cname(reinterpret_cast<const char*>(child->name));
        if (cname == "Id" || cname == "SubtitleID") has_id = true;
        if (cname == "EditRate") has_edit_rate = true;
        if (cname == "TimeCodeRate") has_time_code_rate = true;
    }

    if (!has_id) {
        ctx.notes.push_back({Severity::error, Code::missing_required_element,
                            "Subtitle missing Id element", xml_path});
    }
    if (standard == Standard::smpte && !has_edit_rate) {
        ctx.notes.push_back({Severity::warning, Code::missing_required_element,
                            "SMPTE subtitle missing EditRate element", xml_path});
    }
    if (standard == Standard::smpte && !has_time_code_rate) {
        ctx.notes.push_back({Severity::warning, Code::missing_required_element,
                            "SMPTE subtitle missing TimeCodeRate element", xml_path});
    }

    // Walk tree checking timing and fonts
    walk_subtitles(ctx, root->children);

    // Check for referenced fonts not declared
    for (const auto& font_id : ctx.referenced_fonts) {
        if (!ctx.declared_fonts.contains(font_id)) {
            ctx.notes.push_back({Severity::warning, Code::subtitle_font_missing,
                                "Font '" + font_id + "' referenced but not declared via LoadFont",
                                xml_path});
        }
    }

    xmlFreeDoc(doc);
    return ctx.notes;
}

} // namespace dcpdoctor
