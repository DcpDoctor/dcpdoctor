#include "dcpdoctor/diff.h"
#include <libxml/parser.h>
#include <algorithm>
#include <fstream>
#include <map>
#include <set>
#include <sstream>

namespace dcpdoctor
{
namespace fs = std::filesystem;
namespace
{

  struct AssetEntry
  {
    std::string id;
    std::string filename;
    uint64_t size = 0;
    std::string hash;
  };

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

  std::map<std::string, AssetEntry> parse_assetmap(const fs::path& dcp_dir)
  {
    std::map<std::string, AssetEntry> assets;

    fs::path am_path;
    if(fs::exists(dcp_dir / "ASSETMAP.xml"))
      am_path = dcp_dir / "ASSETMAP.xml";
    else if(fs::exists(dcp_dir / "ASSETMAP"))
      am_path = dcp_dir / "ASSETMAP";
    else
      return assets;

    auto doc = xmlReadFile(am_path.string().c_str(), nullptr,
                           XML_PARSE_NOERROR | XML_PARSE_NOWARNING | XML_PARSE_NONET);
    if(!doc)
      return assets;

    auto root = xmlDocGetRootElement(doc);
    if(!root)
    {
      xmlFreeDoc(doc);
      return assets;
    }

    // Find AssetList
    for(auto al = root->children; al; al = al->next)
    {
      if(al->type != XML_ELEMENT_NODE)
        continue;
      if(xmlStrcmp(al->name, BAD_CAST "AssetList") != 0)
        continue;

      for(auto asset = al->children; asset; asset = asset->next)
      {
        if(asset->type != XML_ELEMENT_NODE)
          continue;
        if(xmlStrcmp(asset->name, BAD_CAST "Asset") != 0)
          continue;

        AssetEntry entry;
        entry.id = get_text(asset, "Id");
        if(entry.id.starts_with("urn:uuid:"))
          entry.id = entry.id.substr(9);

        // Find ChunkList/Chunk/Path
        for(auto cl = asset->children; cl; cl = cl->next)
        {
          if(cl->type != XML_ELEMENT_NODE)
            continue;
          if(xmlStrcmp(cl->name, BAD_CAST "ChunkList") != 0)
            continue;
          for(auto chunk = cl->children; chunk; chunk = chunk->next)
          {
            if(chunk->type != XML_ELEMENT_NODE)
              continue;
            if(xmlStrcmp(chunk->name, BAD_CAST "Chunk") != 0)
              continue;
            entry.filename = get_text(chunk, "Path");
            auto sz = get_text(chunk, "Length");
            if(!sz.empty())
              entry.size = std::stoull(sz);
          }
        }

        if(!entry.id.empty())
          assets[entry.id] = std::move(entry);
      }
    }

    xmlFreeDoc(doc);
    return assets;
  }

} // namespace

DcpDiff compare_dcps(const fs::path& dcp_a, const fs::path& dcp_b, bool check_hashes)
{
  DcpDiff diff;
  diff.dcp_a = dcp_a;
  diff.dcp_b = dcp_b;

  auto assets_a = parse_assetmap(dcp_a);
  auto assets_b = parse_assetmap(dcp_b);

  std::set<std::string> all_ids;
  for(auto& [id, _] : assets_a)
    all_ids.insert(id);
  for(auto& [id, _] : assets_b)
    all_ids.insert(id);

  diff.structure_identical = true;
  diff.content_identical = true;

  for(const auto& id : all_ids)
  {
    DcpDiff::AssetDiff ad;
    ad.id = id;

    auto it_a = assets_a.find(id);
    auto it_b = assets_b.find(id);

    if(it_a != assets_a.end() && it_b == assets_b.end())
    {
      ad.status = DcpDiff::AssetDiff::Status::removed;
      ad.filename = it_a->second.filename;
      ad.detail = "Only in " + dcp_a.filename().string();
      diff.structure_identical = false;
      diff.content_identical = false;
    }
    else if(it_a == assets_a.end() && it_b != assets_b.end())
    {
      ad.status = DcpDiff::AssetDiff::Status::added;
      ad.filename = it_b->second.filename;
      ad.detail = "Only in " + dcp_b.filename().string();
      diff.structure_identical = false;
      diff.content_identical = false;
    }
    else
    {
      ad.filename = it_a->second.filename;
      if(it_a->second.size != it_b->second.size)
      {
        ad.status = DcpDiff::AssetDiff::Status::modified;
        ad.detail = "Size: " + std::to_string(it_a->second.size) + " → " +
                    std::to_string(it_b->second.size);
        diff.content_identical = false;
      }
      else
      {
        ad.status = DcpDiff::AssetDiff::Status::same;
      }
    }

    diff.assets.push_back(std::move(ad));
  }

  return diff;
}

void write_diff_report(std::ostream& out, const DcpDiff& diff)
{
  out << "DCP Comparison Report\n";
  out << "=====================\n";
  out << "A: " << diff.dcp_a.string() << "\n";
  out << "B: " << diff.dcp_b.string() << "\n";
  out << "Structure: " << (diff.structure_identical ? "IDENTICAL" : "DIFFERENT") << "\n";
  out << "Content:   " << (diff.content_identical ? "IDENTICAL" : "DIFFERENT") << "\n\n";

  if(diff.content_identical)
  {
    out << "DCPs are identical.\n";
    return;
  }

  out << "Changes:\n";
  for(const auto& ad : diff.assets)
  {
    if(ad.status == DcpDiff::AssetDiff::Status::same)
      continue;

    const char* marker = "";
    switch(ad.status)
    {
      case DcpDiff::AssetDiff::Status::added:
        marker = "+";
        break;
      case DcpDiff::AssetDiff::Status::removed:
        marker = "-";
        break;
      case DcpDiff::AssetDiff::Status::modified:
        marker = "~";
        break;
      default:
        break;
    }
    out << "  " << marker << " " << ad.filename;
    if(!ad.detail.empty())
      out << " (" << ad.detail << ")";
    out << "\n";
  }
}

} // namespace dcpdoctor
