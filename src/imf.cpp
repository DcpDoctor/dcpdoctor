#include "dcpdoctor/imf.h"
#include <libxml/parser.h>
#include <filesystem>
#include <set>

namespace dcpdoctor
{
namespace fs = std::filesystem;
namespace
{

  std::string get_text(xmlNodePtr parent, const char* name)
  {
    for(auto child = parent->children; child; child = child->next)
    {
      if(child->type == XML_ELEMENT_NODE && xmlStrcmp(child->name, BAD_CAST name) == 0)
      {
        auto content = xmlNodeGetContent(child);
        if(content)
        {
          std::string r(reinterpret_cast<const char*>(content));
          xmlFree(content);
          return r;
        }
      }
    }
    return {};
  }

  std::string find_recursive(xmlNodePtr node, const char* name)
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
      auto child_result = find_recursive(cur->children, name);
      if(!child_result.empty())
        return child_result;
    }
    return {};
  }

} // namespace

ImfPackageInfo validate_imf_package(const fs::path& imf_dir)
{
  ImfPackageInfo info;

  // IMF packages use ASSETMAP.xml (or ASSETMAP without extension)
  auto assetmap_path = imf_dir / "ASSETMAP.xml";
  if(!fs::exists(assetmap_path))
  {
    assetmap_path = imf_dir / "ASSETMAP";
    if(!fs::exists(assetmap_path))
    {
      info.error = "No ASSETMAP found - not a valid IMF package";
      return info;
    }
  }

  info.has_assetmap = true;

  // Parse ASSETMAP
  auto doc = xmlReadFile(assetmap_path.string().c_str(), nullptr,
                         XML_PARSE_NOERROR | XML_PARSE_NOWARNING | XML_PARSE_NONET);
  if(!doc)
  {
    info.error = "Failed to parse ASSETMAP";
    return info;
  }

  auto root = xmlDocGetRootElement(doc);
  if(!root)
  {
    info.error = "Empty ASSETMAP";
    xmlFreeDoc(doc);
    return info;
  }

  // Count assets and collect them
  for(auto child = root->children; child; child = child->next)
  {
    if(child->type != XML_ELEMENT_NODE)
      continue;
    if(xmlStrcmp(child->name, BAD_CAST "AssetList") != 0)
      continue;

    for(auto asset = child->children; asset; asset = asset->next)
    {
      if(asset->type != XML_ELEMENT_NODE)
        continue;
      if(xmlStrcmp(asset->name, BAD_CAST "Asset") != 0)
        continue;

      ImfPackageInfo::AssetEntry entry;
      entry.id = get_text(asset, "Id");

      // Look for ChunkList -> Chunk -> Path
      for(auto chunk_list = asset->children; chunk_list; chunk_list = chunk_list->next)
      {
        if(chunk_list->type != XML_ELEMENT_NODE)
          continue;
        if(xmlStrcmp(chunk_list->name, BAD_CAST "ChunkList") != 0)
          continue;
        for(auto chunk = chunk_list->children; chunk; chunk = chunk->next)
        {
          if(chunk->type != XML_ELEMENT_NODE)
            continue;
          if(xmlStrcmp(chunk->name, BAD_CAST "Chunk") != 0)
            continue;
          entry.path = get_text(chunk, "Path");
          break;
        }
      }

      // Check PackingList flag
      auto packing_list = get_text(asset, "PackingList");
      if(packing_list == "true" || packing_list == "1")
      {
        info.packing_list_id = entry.id;
        entry.is_packing_list = true;
      }

      info.assets.push_back(std::move(entry));
    }
  }

  xmlFreeDoc(doc);

  // Check for packing list (PKL)
  if(!info.packing_list_id.empty())
  {
    info.has_packing_list = true;
  }

  // Find CPLs in the package
  std::error_code ec;
  for(auto& entry : fs::directory_iterator(imf_dir, ec))
  {
    if(!entry.is_regular_file())
      continue;
    if(entry.path().extension() != ".xml")
      continue;
    if(entry.path().filename() == "ASSETMAP.xml" || entry.path().filename() == "ASSETMAP")
      continue;

    auto cpl_doc = xmlReadFile(entry.path().string().c_str(), nullptr,
                               XML_PARSE_NOERROR | XML_PARSE_NOWARNING | XML_PARSE_NONET);
    if(!cpl_doc)
      continue;

    auto cpl_root = xmlDocGetRootElement(cpl_doc);
    if(!cpl_root)
    {
      xmlFreeDoc(cpl_doc);
      continue;
    }

    std::string rn(reinterpret_cast<const char*>(cpl_root->name));
    if(rn == "CompositionPlaylist")
    {
      ImfCplInfo cpl;
      cpl.id = get_text(cpl_root, "Id");
      cpl.content_title = get_text(cpl_root, "ContentTitleText");
      cpl.edit_rate = find_recursive(cpl_root->children, "EditRate");
      cpl.annotation = get_text(cpl_root, "AnnotationText");

      // Check for IMF-specific elements
      auto app_id = find_recursive(cpl_root->children, "ApplicationIdentification");
      if(!app_id.empty())
      {
        cpl.application_id = app_id;
        info.is_imf = true; // Has IMF-specific metadata
      }

      // Count segments (IMF uses Segments, DCP uses Reels)
      for(auto child = cpl_root->children; child; child = child->next)
      {
        if(child->type != XML_ELEMENT_NODE)
          continue;
        if(xmlStrcmp(child->name, BAD_CAST "SegmentList") == 0)
        {
          info.is_imf = true; // SegmentList is IMF-specific
          for(auto seg = child->children; seg; seg = seg->next)
          {
            if(seg->type == XML_ELEMENT_NODE && xmlStrcmp(seg->name, BAD_CAST "Segment") == 0)
              cpl.segment_count++;
          }
        }
        if(xmlStrcmp(child->name, BAD_CAST "ReelList") == 0)
        {
          for(auto reel = child->children; reel; reel = reel->next)
          {
            if(reel->type == XML_ELEMENT_NODE && xmlStrcmp(reel->name, BAD_CAST "Reel") == 0)
              cpl.reel_count++;
          }
        }
      }

      info.cpls.push_back(std::move(cpl));
    }
    else if(rn == "PackingList")
    {
      info.has_packing_list = true;
    }

    xmlFreeDoc(cpl_doc);
  }

  info.valid = true;
  return info;
}

std::vector<Note> check_imf_compliance(const ImfPackageInfo& info, const fs::path& imf_dir)
{
  std::vector<Note> notes;

  if(!info.valid)
  {
    notes.push_back(Note{Severity::error, Code::missing_assetmap,
                         "Invalid IMF package: " + info.error, imf_dir});
    return notes;
  }

  if(!info.has_assetmap)
  {
    notes.push_back(
        Note{Severity::error, Code::missing_assetmap, "IMF package missing ASSETMAP", imf_dir});
  }

  if(!info.has_packing_list)
  {
    notes.push_back(Note{Severity::error, Code::missing_pkl,
                         "IMF package missing Packing List (PKL)", imf_dir});
  }

  if(info.cpls.empty())
  {
    notes.push_back(Note{Severity::error, Code::missing_cpl,
                         "IMF package has no Composition Playlist", imf_dir});
  }

  // Check asset file existence
  for(const auto& asset : info.assets)
  {
    if(asset.path.empty())
      continue;
    auto full_path = imf_dir / asset.path;
    if(!fs::exists(full_path))
    {
      notes.push_back(Note{Severity::error, Code::asset_not_found,
                           "Asset referenced in ASSETMAP not found: " + asset.path, imf_dir});
    }
  }

  // IMF-specific checks
  if(info.is_imf)
  {
    notes.push_back(Note{Severity::info, Code::missing_assetmap,
                         "Package identified as IMF (Interoperable Master Format)", imf_dir});

    for(const auto& cpl : info.cpls)
    {
      if(cpl.application_id.empty())
      {
        notes.push_back(
            Note{Severity::warning, Code::missing_cpl,
                 "IMF CPL '" + cpl.content_title + "' missing ApplicationIdentification", imf_dir});
      }
      if(cpl.segment_count == 0 && cpl.reel_count == 0)
      {
        notes.push_back(Note{Severity::error, Code::missing_cpl,
                             "IMF CPL '" + cpl.content_title + "' has no segments or reels",
                             imf_dir});
      }
    }
  }

  return notes;
}

} // namespace dcpdoctor
