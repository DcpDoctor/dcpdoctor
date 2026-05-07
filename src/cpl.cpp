#include "dcpdoctor/cpl.h"
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

static int64_t parse_int(const std::string& s)
{
  if(s.empty())
    return 0;
  try
  {
    return std::stoll(s);
  }
  catch(...)
  {
    return 0;
  }
}

std::optional<Cpl> Cpl::parse(const std::filesystem::path& file)
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

  std::string root_name(reinterpret_cast<const char*>(root->name));
  if(root_name != "CompositionPlaylist")
  {
    xmlFreeDoc(doc);
    return std::nullopt;
  }

  Cpl cpl;

  if(auto node = find_child(root, "Id"))
    cpl.id = get_content(node);
  if(auto node = find_child(root, "ContentTitleText"))
    cpl.content_title = get_content(node);
  if(auto node = find_child(root, "ContentKind"))
    cpl.content_kind = get_content(node);
  if(auto node = find_child(root, "IssueDate"))
    cpl.issue_date = get_content(node);

  auto reel_list = find_child(root, "ReelList");
  if(!reel_list)
  {
    xmlFreeDoc(doc);
    return cpl;
  }

  for(auto reel_node = reel_list->children; reel_node; reel_node = reel_node->next)
  {
    if(reel_node->type != XML_ELEMENT_NODE)
      continue;
    if(std::strcmp(reinterpret_cast<const char*>(reel_node->name), "Reel") != 0)
      continue;

    Reel reel;
    if(auto node = find_child(reel_node, "Id"))
      reel.id = get_content(node);

    auto asset_list = find_child(reel_node, "AssetList");
    if(!asset_list)
    {
      cpl.reels.push_back(std::move(reel));
      continue;
    }

    // MainPicture
    if(auto pic = find_child(asset_list, "MainPicture"))
    {
      if(auto node = find_child(pic, "Id"))
        reel.picture.id = get_content(node);
      if(auto node = find_child(pic, "EditRate"))
        reel.picture.edit_rate = get_content(node);
      if(auto node = find_child(pic, "Duration"))
        reel.picture.duration = parse_int(get_content(node));
      if(auto node = find_child(pic, "EntryPoint"))
        reel.picture.entry_point = parse_int(get_content(node));
    }

    // MainSound
    if(auto snd = find_child(asset_list, "MainSound"))
    {
      if(auto node = find_child(snd, "Id"))
        reel.sound.id = get_content(node);
      if(auto node = find_child(snd, "EditRate"))
        reel.sound.edit_rate = get_content(node);
      if(auto node = find_child(snd, "Duration"))
        reel.sound.duration = parse_int(get_content(node));
      if(auto node = find_child(snd, "EntryPoint"))
        reel.sound.entry_point = parse_int(get_content(node));
    }

    cpl.reels.push_back(std::move(reel));
  }

  xmlFreeDoc(doc);
  return cpl;
}

} // namespace dcpdoctor
