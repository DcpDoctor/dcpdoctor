#include <filesystem>
#include <map>

#include "dcpdoctor/fixes.h"

namespace dcpdoctor
{
namespace fs = std::filesystem;

std::vector<FixSuggestion> suggest_fixes(const std::vector<Note>& notes)
{
  std::vector<FixSuggestion> suggestions;

  for(const auto& note : notes)
  {
    switch(note.code)
    {
      case Code::smpte_naming_violation:
        if(note.message.find("ASSETMAP") != std::string::npos)
        {
          suggestions.push_back(FixSuggestion{
              Code::smpte_naming_violation, "Rename ASSETMAP to ASSETMAP.xml for BV2.1 compliance",
              "mv ASSETMAP ASSETMAP.xml", true});
        }
        else if(note.message.find("PKL") != std::string::npos &&
                note.message.find(".xml") != std::string::npos)
        {
          suggestions.push_back(FixSuggestion{
              Code::smpte_naming_violation, "Rename PKL file to have .xml extension", "",
              false // Need to know exact filename
          });
        }
        break;

      case Code::smpte_namespace_wrong:
        suggestions.push_back(FixSuggestion{
            Code::smpte_namespace_wrong,
            "Convert DCP from Interop to SMPTE standard (requires re-wrapping all MXF files)", "",
            false});
        break;

      case Code::pkl_hash_mismatch:
        suggestions.push_back(FixSuggestion{
            Code::pkl_hash_mismatch,
            "Regenerate PKL hashes (file may have been modified after packaging)", "", false});
        break;

      case Code::missing_required_element:
        if(note.message.find("ContentVersion") != std::string::npos)
        {
          suggestions.push_back(
              FixSuggestion{Code::missing_required_element,
                            "Add ContentVersion element to CPL (required for BV2.1)", "", false});
        }
        else if(note.message.find("MainMarkers") != std::string::npos)
        {
          suggestions.push_back(FixSuggestion{
              Code::missing_required_element,
              "Add MainMarkers to first reel in CPL (FFOC, LFOC at minimum for BV2.1)", "", false});
        }
        break;

      case Code::j2k_bitrate_exceeded:
        suggestions.push_back(FixSuggestion{
            Code::j2k_bitrate_exceeded,
            "Re-encode picture at lower bitrate (DCI limit: 250 Mbps for 2K, 500 Mbps for 4K)", "",
            false});
        break;

      case Code::isdcf_naming_violation:
        suggestions.push_back(
            FixSuggestion{Code::isdcf_naming_violation,
                          "Rename content title to follow ISDCF naming convention: "
                          "Title_ContentType_AspectRatio_Language_Territory_AudioType_Resolution_"
                          "Studio_Date_Facility_Standard",
                          "", false});
        break;

      case Code::sound_invalid_channel_count:
        if(note.message.find("MCA") != std::string::npos)
        {
          suggestions.push_back(FixSuggestion{
              Code::sound_invalid_channel_count,
              "Add MCA (Multi-Channel Audio) labeling metadata to sound MXF", "", false});
        }
        break;

      case Code::encryption_detected:
        suggestions.push_back(FixSuggestion{
            Code::encryption_detected,
            "Obtain a valid KDM from the content distributor for this theater's certificate", "",
            false});
        break;

      case Code::marker_missing:
        suggestions.push_back(FixSuggestion{Code::marker_missing,
                                            "Add required markers to CPL (FFOC=first frame, "
                                            "LFOC=last frame, FFMC, LFMC for features)",
                                            "", false});
        break;

      case Code::subtitle_invalid_timing:
        suggestions.push_back(FixSuggestion{
            Code::subtitle_invalid_timing,
            "Fix subtitle timing: ensure all TimeIn < TimeOut and within reel duration", "",
            false});
        break;

      default:
        break;
    }
  }

  return suggestions;
}

int apply_fixes(const fs::path& dcp_dir, const std::vector<FixSuggestion>& suggestions)
{
  int applied = 0;

  for(const auto& fix : suggestions)
  {
    if(!fix.auto_fixable)
      continue;

    if(fix.related_code == Code::smpte_naming_violation &&
       fix.command == "mv ASSETMAP ASSETMAP.xml")
    {
      auto src = dcp_dir / "ASSETMAP";
      auto dst = dcp_dir / "ASSETMAP.xml";
      if(fs::exists(src) && !fs::exists(dst))
      {
        std::error_code ec;
        fs::rename(src, dst, ec);
        if(!ec)
          ++applied;
      }
    }
  }

  return applied;
}

} // namespace dcpdoctor
