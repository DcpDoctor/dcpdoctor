#include "dcpdoctor/info.h"
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <stdexcept>

namespace fs = std::filesystem;

namespace dcpdoctor
{

static std::string get_text(xmlNodePtr node)
{
  if(!node || !node->children)
    return "";
  xmlChar* content = xmlNodeGetContent(node);
  std::string result(reinterpret_cast<const char*>(content));
  xmlFree(content);
  return result;
}

static xmlNodePtr find_child(xmlNodePtr parent, const char* name)
{
  for(xmlNodePtr cur = parent->children; cur; cur = cur->next)
  {
    if(cur->type == XML_ELEMENT_NODE && xmlStrcmp(cur->name, BAD_CAST name) == 0)
    {
      return cur;
    }
  }
  return nullptr;
}

static fs::path find_assetmap(const fs::path& dir)
{
  for(const auto& name : {"ASSETMAP.xml", "ASSETMAP"})
  {
    fs::path p = dir / name;
    if(fs::exists(p))
      return p;
  }
  return {};
}

static fs::path find_pkl(const fs::path& dir)
{
  for(const auto& entry : fs::directory_iterator(dir))
  {
    if(entry.path().filename().string().starts_with("PKL_") && entry.path().extension() == ".xml")
    {
      return entry.path();
    }
  }
  return {};
}

static fs::path find_cpl(const fs::path& dir)
{
  for(const auto& entry : fs::directory_iterator(dir))
  {
    if(entry.path().filename().string().starts_with("CPL_") && entry.path().extension() == ".xml")
    {
      return entry.path();
    }
  }
  return {};
}

ImpInfo read_imp_info(const fs::path& imp_dir)
{
  ImpInfo info;
  info.path = imp_dir;

  try
  {
    // Find and parse AssetMap
    fs::path am_path = find_assetmap(imp_dir);
    if(am_path.empty())
    {
      info.error = "ASSETMAP.xml not found";
      return info;
    }

    // Parse PKL
    fs::path pkl_path = find_pkl(imp_dir);
    if(!pkl_path.empty())
    {
      xmlDocPtr pkl_doc = xmlParseFile(pkl_path.string().c_str());
      if(pkl_doc)
      {
        xmlNodePtr root = xmlDocGetRootElement(pkl_doc);
        xmlNodePtr id_node = find_child(root, "Id");
        if(id_node)
          info.pkl_uuid = get_text(id_node);
        xmlNodePtr issuer_node = find_child(root, "Issuer");
        if(issuer_node)
          info.issuer = get_text(issuer_node);
        xmlNodePtr date_node = find_child(root, "IssueDate");
        if(date_node)
          info.issue_date = get_text(date_node);

        // Read assets from PKL
        xmlNodePtr asset_list = find_child(root, "AssetList");
        if(asset_list)
        {
          for(xmlNodePtr asset = asset_list->children; asset; asset = asset->next)
          {
            if(asset->type != XML_ELEMENT_NODE)
              continue;
            ImpTrackInfo track;
            xmlNodePtr uuid_node = find_child(asset, "Id");
            if(uuid_node)
              track.uuid = get_text(uuid_node);
            xmlNodePtr fn_node = find_child(asset, "OriginalFileName");
            if(fn_node)
              track.filename = get_text(fn_node);
            xmlNodePtr type_node = find_child(asset, "Type");
            if(type_node)
            {
              std::string mime = get_text(type_node);
              if(mime.find("mxf") != std::string::npos)
                track.type = "essence";
              else
                track.type = "metadata";
            }
            xmlNodePtr size_node = find_child(asset, "Size");
            if(size_node)
              track.size = std::stoull(get_text(size_node));
            info.tracks.push_back(track);
          }
        }
        xmlFreeDoc(pkl_doc);
      }
    }

    // Parse CPL
    fs::path cpl_path = find_cpl(imp_dir);
    if(!cpl_path.empty())
    {
      xmlDocPtr cpl_doc = xmlParseFile(cpl_path.string().c_str());
      if(cpl_doc)
      {
        xmlNodePtr root = xmlDocGetRootElement(cpl_doc);
        xmlNodePtr id_node = find_child(root, "Id");
        if(id_node)
          info.cpl_uuid = get_text(id_node);
        xmlNodePtr title_node = find_child(root, "ContentTitle");
        if(title_node)
          info.cpl_title = get_text(title_node);
        xmlFreeDoc(cpl_doc);
      }
    }

    info.valid = true;
  }
  catch(const std::exception& e)
  {
    info.error = e.what();
  }

  return info;
}

} // namespace dcpdoctor
