#include <libxml/xmlschemas.h>
#include <libxml/parser.h>

#include "dcpdoctor/schema_validator.h"

namespace dcpdoctor
{

std::vector<Note> validate_schema(const std::filesystem::path& xml_file,
                                  const std::filesystem::path& schema_file)
{
  std::vector<Note> notes;

  xmlDocPtr schema_doc = xmlReadFile(schema_file.string().c_str(), nullptr, XML_PARSE_NONET);
  if(!schema_doc)
  {
    notes.push_back({Severity::error, Code::xml_parse_error,
                     "Cannot read schema: " + schema_file.string(), schema_file});
    return notes;
  }

  xmlSchemaParserCtxtPtr parser_ctx = xmlSchemaNewDocParserCtxt(schema_doc);
  xmlSchemaPtr schema = xmlSchemaParse(parser_ctx);
  xmlSchemaFreeParserCtxt(parser_ctx);

  if(!schema)
  {
    xmlFreeDoc(schema_doc);
    notes.push_back({Severity::error, Code::xml_schema_violation,
                     "Invalid schema file: " + schema_file.string(), schema_file});
    return notes;
  }

  xmlSchemaValidCtxtPtr valid_ctx = xmlSchemaNewValidCtxt(schema);

  xmlDocPtr doc = xmlReadFile(xml_file.string().c_str(), nullptr, XML_PARSE_NONET);
  if(!doc)
  {
    notes.push_back({Severity::error, Code::xml_parse_error,
                     "Cannot parse XML: " + xml_file.string(), xml_file});
  }
  else
  {
    int ret = xmlSchemaValidateDoc(valid_ctx, doc);
    if(ret != 0)
    {
      notes.push_back({Severity::error, Code::xml_schema_violation,
                       "XML does not conform to schema", xml_file});
    }
    xmlFreeDoc(doc);
  }

  xmlSchemaFreeValidCtxt(valid_ctx);
  xmlSchemaFree(schema);
  xmlFreeDoc(schema_doc);

  return notes;
}

std::vector<Note> validate_namespace(const std::filesystem::path& xml_file, Standard expected)
{
  std::vector<Note> notes;

  xmlDocPtr doc = xmlReadFile(xml_file.string().c_str(), nullptr, XML_PARSE_NONET);
  if(!doc)
    return notes;

  xmlNodePtr root = xmlDocGetRootElement(doc);
  if(root && root->ns && root->ns->href)
  {
    std::string ns(reinterpret_cast<const char*>(root->ns->href));

    if(expected == Standard::smpte)
    {
      if(ns.find("smpte-ra.org") == std::string::npos && ns.find("smpte.org") == std::string::npos)
      {
        notes.push_back({Severity::warning, Code::smpte_namespace_wrong,
                         "Expected SMPTE namespace, found: " + ns, xml_file});
      }
    }
    else if(expected == Standard::interop)
    {
      if(ns.find("digicine.com") == std::string::npos)
      {
        notes.push_back({Severity::warning, Code::interop_namespace_wrong,
                         "Expected Interop namespace, found: " + ns, xml_file});
      }
    }
  }

  xmlFreeDoc(doc);
  return notes;
}

} // namespace dcpdoctor
