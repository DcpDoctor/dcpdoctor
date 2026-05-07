#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <ctime>
#include <iomanip>
#include <sstream>

#include "dcpdoctor/kdm.h"
#include "dcpdoctor/platform.h"

namespace dcpdoctor
{
namespace fs = std::filesystem;
namespace
{

  std::string get_element_text_recursive(xmlNodePtr node, const char* name)
  {
    for(auto cur = node; cur; cur = cur->next)
    {
      if(cur->type == XML_ELEMENT_NODE && xmlStrcmp(cur->name, BAD_CAST name) == 0)
      {
        auto content = xmlNodeGetContent(cur);
        if(content)
        {
          std::string r(reinterpret_cast<const char*>(content));
          xmlFree(content);
          return r;
        }
      }
      auto child_result = get_element_text_recursive(cur->children, name);
      if(!child_result.empty())
        return child_result;
    }
    return {};
  }

  std::chrono::system_clock::time_point parse_iso8601(const std::string& str)
  {
    std::tm tm{};
    std::istringstream ss(str);
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    if(ss.fail())
      return {};
    return std::chrono::system_clock::from_time_t(DCPDOCTOR_TIMEGM(&tm));
  }

} // namespace

KdmInfo parse_kdm(const fs::path& kdm_path)
{
  KdmInfo info;

  auto doc = xmlReadFile(kdm_path.string().c_str(), nullptr,
                         XML_PARSE_NOERROR | XML_PARSE_NOWARNING | XML_PARSE_NONET);
  if(!doc)
  {
    info.error = "Failed to parse KDM XML";
    return info;
  }

  auto root = xmlDocGetRootElement(doc);
  if(!root)
  {
    info.error = "Empty KDM document";
    xmlFreeDoc(doc);
    return info;
  }

  // Check root element name
  std::string root_name(reinterpret_cast<const char*>(root->name));
  if(root_name != "DCinemaSecurityMessage")
  {
    info.error = "Not a KDM (root element: " + root_name + ")";
    xmlFreeDoc(doc);
    return info;
  }

  // Extract key fields
  info.cpl_id = get_element_text_recursive(root->children, "CompositionPlaylistId");
  info.content_title = get_element_text_recursive(root->children, "ContentTitleText");

  // Validity period
  auto not_before = get_element_text_recursive(root->children, "ContentKeysNotValidBefore");
  auto not_after = get_element_text_recursive(root->children, "ContentKeysNotValidAfter");

  if(!not_before.empty())
    info.not_valid_before = parse_iso8601(not_before);
  if(!not_after.empty())
    info.not_valid_after = parse_iso8601(not_after);

  // Check validity against current time
  auto now = std::chrono::system_clock::now();
  if(info.not_valid_after != std::chrono::system_clock::time_point{} && now > info.not_valid_after)
    info.is_expired = true;
  if(info.not_valid_before != std::chrono::system_clock::time_point{} &&
     now < info.not_valid_before)
    info.is_not_yet_valid = true;

  info.valid = true;
  xmlFreeDoc(doc);
  return info;
}

std::vector<Note> validate_kdm(const fs::path& kdm_path, const fs::path& dcp_dir)
{
  std::vector<Note> notes;

  auto info = parse_kdm(kdm_path);
  if(!info.valid)
  {
    notes.push_back(
        Note{Severity::error, Code::encryption_detected, "Invalid KDM: " + info.error, kdm_path});
    return notes;
  }

  if(info.is_expired)
  {
    notes.push_back(Note{Severity::error, Code::encryption_detected, "KDM has expired", kdm_path});
  }

  if(info.is_not_yet_valid)
  {
    notes.push_back(Note{Severity::warning, Code::encryption_detected,
                         "KDM is not yet valid (future start date)", kdm_path});
  }

  // Check if CPL referenced by KDM exists in the DCP
  if(!info.cpl_id.empty() && fs::is_directory(dcp_dir))
  {
    bool found_cpl = false;
    std::error_code ec;
    for(auto& entry : fs::directory_iterator(dcp_dir, ec))
    {
      if(!entry.is_regular_file())
        continue;
      if(entry.path().extension() != ".xml")
        continue;

      auto doc = xmlReadFile(entry.path().string().c_str(), nullptr,
                             XML_PARSE_NOERROR | XML_PARSE_NOWARNING | XML_PARSE_NONET);
      if(!doc)
        continue;
      auto root = xmlDocGetRootElement(doc);
      if(root)
      {
        std::string rn(reinterpret_cast<const char*>(root->name));
        if(rn == "CompositionPlaylist")
        {
          auto id = get_element_text_recursive(root->children, "Id");
          // Normalize: strip urn:uuid: prefix
          if(id.starts_with("urn:uuid:"))
            id = id.substr(9);
          std::string cpl_ref = info.cpl_id;
          if(cpl_ref.starts_with("urn:uuid:"))
            cpl_ref = cpl_ref.substr(9);
          if(id == cpl_ref)
            found_cpl = true;
        }
      }
      xmlFreeDoc(doc);
      if(found_cpl)
        break;
    }

    if(!found_cpl)
    {
      notes.push_back(Note{Severity::warning, Code::cross_ref_broken,
                           "KDM references CPL " + info.cpl_id + " which was not found in the DCP",
                           kdm_path});
    }
  }

  return notes;
}

} // namespace dcpdoctor
