#include "dcpdoctor/pkl.h"
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <cstring>

namespace dcpdoctor {

static std::string get_content(xmlNodePtr node) {
    xmlChar* content = xmlNodeGetContent(node);
    if (!content) return {};
    std::string result(reinterpret_cast<const char*>(content));
    xmlFree(content);
    return result;
}

static xmlNodePtr find_child(xmlNodePtr parent, const char* name) {
    for (auto child = parent->children; child; child = child->next) {
        if (child->type == XML_ELEMENT_NODE && std::strcmp(reinterpret_cast<const char*>(child->name), name) == 0)
            return child;
    }
    return nullptr;
}

std::optional<Pkl> Pkl::parse(const std::filesystem::path& file) {
    xmlDocPtr doc = xmlReadFile(file.string().c_str(), nullptr, XML_PARSE_NONET | XML_PARSE_NOBLANKS | XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
    if (!doc) return std::nullopt;

    xmlNodePtr root = xmlDocGetRootElement(doc);
    if (!root) {
        xmlFreeDoc(doc);
        return std::nullopt;
    }

    std::string root_name(reinterpret_cast<const char*>(root->name));
    if (root_name != "PackingList") {
        xmlFreeDoc(doc);
        return std::nullopt;
    }

    Pkl pkl;

    if (auto node = find_child(root, "Id"))
        pkl.id = get_content(node);

    if (auto node = find_child(root, "Creator"))
        pkl.creator = get_content(node);

    if (auto node = find_child(root, "IssueDate"))
        pkl.issue_date = get_content(node);

    auto asset_list = find_child(root, "AssetList");
    if (!asset_list) {
        xmlFreeDoc(doc);
        return pkl;
    }

    for (auto asset_node = asset_list->children; asset_node; asset_node = asset_node->next) {
        if (asset_node->type != XML_ELEMENT_NODE)
            continue;
        if (std::strcmp(reinterpret_cast<const char*>(asset_node->name), "Asset") != 0)
            continue;

        PklAsset asset;
        if (auto node = find_child(asset_node, "Id"))
            asset.id = get_content(node);
        if (auto node = find_child(asset_node, "Type"))
            asset.type = get_content(node);
        if (auto node = find_child(asset_node, "OriginalFileName"))
            asset.original_filename = get_content(node);
        if (auto node = find_child(asset_node, "Hash"))
            asset.hash = get_content(node);
        if (auto node = find_child(asset_node, "HashAlgorithm"))
            asset.hash_algorithm = get_content(node);
        if (auto node = find_child(asset_node, "Size"))
            asset.size = std::stoll(get_content(node));

        if (!asset.id.empty())
            pkl.assets.push_back(std::move(asset));
    }

    xmlFreeDoc(doc);
    return pkl;
}

} // namespace dcpdoctor
