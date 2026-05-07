#include "dcpdoctor/assetmap.h"
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <cstring>

namespace dcpdoctor
{

static std::string get_content(xmlNodePtr node)
{
  xmlChar* content = xmlNodeGetContent(node);
  if(!content)
    return {};
  std::string result(reinterpret_cast<const char*>(content));
  xmlFree(content);
  return result;
}

static xmlNodePtr find_child(xmlNodePtr parent, const char* name)
{
  for(auto child = parent->children; child; child = child->next)
  {
    if(child->type == XML_ELEMENT_NODE &&
       std::strcmp(reinterpret_cast<const char*>(child->name), name) == 0)
      return child;
  }
  return nullptr;
}

std::optional<AssetMap> AssetMap::parse(const std::filesystem::path& file)
{
  xmlDocPtr doc =
      xmlReadFile(file.string().c_str(), nullptr,
                  XML_PARSE_NONET | XML_PARSE_NOBLANKS | XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
  if(!doc)
    return std::nullopt;

  xmlNodePtr root = xmlDocGetRootElement(doc);
  if(!root)
  {
    xmlFreeDoc(doc);
    return std::nullopt;
  }

  // Check if this is actually an AssetMap
  std::string root_name(reinterpret_cast<const char*>(root->name));
  if(root_name != "AssetMap")
  {
    xmlFreeDoc(doc);
    return std::nullopt;
  }

  AssetMap am;

  // Parse Id
  if(auto node = find_child(root, "Id"))
    am.id = get_content(node);

  // Parse Creator
  if(auto node = find_child(root, "Creator"))
    am.creator = get_content(node);

  // Parse IssueDate
  if(auto node = find_child(root, "IssueDate"))
    am.issue_date = get_content(node);

  // Parse AssetList
  auto asset_list = find_child(root, "AssetList");
  if(!asset_list)
  {
    xmlFreeDoc(doc);
    return am;
  }

  for(auto asset_node = asset_list->children; asset_node; asset_node = asset_node->next)
  {
    if(asset_node->type != XML_ELEMENT_NODE)
      continue;
    if(std::strcmp(reinterpret_cast<const char*>(asset_node->name), "Asset") != 0)
      continue;

    Asset asset;
    if(auto node = find_child(asset_node, "Id"))
      asset.id = get_content(node);

    // ChunkList -> Chunk -> Path
    if(auto chunk_list = find_child(asset_node, "ChunkList"))
    {
      for(auto chunk = chunk_list->children; chunk; chunk = chunk->next)
      {
        if(chunk->type != XML_ELEMENT_NODE)
          continue;
        if(auto path_node = find_child(chunk, "Path"))
          asset.path = get_content(path_node);
      }
    }

    if(!asset.id.empty())
      am.assets.push_back(std::move(asset));
  }

  xmlFreeDoc(doc);
  return am;
}

} // namespace dcpdoctor
