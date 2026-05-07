#include "dcpdoctor/signature.h"
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/c14n.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509_vfy.h>
#include <cstring>
#include <ctime>

namespace dcpdoctor
{
namespace
{

  xmlNodePtr find_child(xmlNodePtr parent, const char* name)
  {
    for(auto child = parent->children; child; child = child->next)
    {
      if(child->type == XML_ELEMENT_NODE &&
         std::strcmp(reinterpret_cast<const char*>(child->name), name) == 0)
        return child;
    }
    return nullptr;
  }

  xmlNodePtr find_child_recursive(xmlNodePtr parent, const char* name)
  {
    for(auto child = parent->children; child; child = child->next)
    {
      if(child->type == XML_ELEMENT_NODE &&
         std::strcmp(reinterpret_cast<const char*>(child->name), name) == 0)
        return child;
      auto found = find_child_recursive(child, name);
      if(found)
        return found;
    }
    return nullptr;
  }

  std::string get_text(xmlNodePtr node)
  {
    if(!node)
      return {};
    xmlChar* content = xmlNodeGetContent(node);
    if(!content)
      return {};
    std::string s(reinterpret_cast<const char*>(content));
    xmlFree(content);
    return s;
  }

  std::string get_attr(xmlNodePtr node, const char* name)
  {
    xmlChar* val = xmlGetProp(node, BAD_CAST name);
    if(!val)
      return {};
    std::string s(reinterpret_cast<const char*>(val));
    xmlFree(val);
    return s;
  }

  std::vector<uint8_t> base64_decode(const std::string& input)
  {
    std::string clean;
    clean.reserve(input.size());
    for(char c : input)
    {
      if(c != ' ' && c != '\n' && c != '\r' && c != '\t')
        clean += c;
    }
    BIO* b64 = BIO_new(BIO_f_base64());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    BIO* mem = BIO_new_mem_buf(clean.data(), static_cast<int>(clean.size()));
    BIO_push(b64, mem);
    std::vector<uint8_t> result(clean.size());
    int len = BIO_read(b64, result.data(), static_cast<int>(result.size()));
    BIO_free_all(b64);
    if(len > 0)
      result.resize(len);
    else
      result.clear();
    return result;
  }

  std::vector<uint8_t> canonicalize_subtree(xmlDocPtr doc, xmlNodePtr node, int mode)
  {
    // Create a temporary doc with just this subtree
    xmlDocPtr subdoc = xmlNewDoc(BAD_CAST "1.0");
    xmlNodePtr copy = xmlDocCopyNode(node, subdoc, 1);
    xmlDocSetRootElement(subdoc, copy);

    // Inherit namespace declarations from ancestor chain
    for(auto ancestor = node->parent; ancestor; ancestor = ancestor->parent)
    {
      if(ancestor->type != XML_ELEMENT_NODE)
        continue;
      for(auto ns = ancestor->nsDef; ns; ns = ns->next)
      {
        if(!xmlSearchNs(subdoc, copy, ns->prefix))
          xmlNewNs(copy, ns->href, ns->prefix);
      }
      if(ancestor->ns && ancestor->ns->href)
      {
        if(!xmlSearchNs(subdoc, copy, ancestor->ns->prefix))
          xmlNewNs(copy, ancestor->ns->href, ancestor->ns->prefix);
      }
    }

    xmlChar* buf = nullptr;
    int len = xmlC14NDocDumpMemory(subdoc, nullptr, mode, nullptr, 0, &buf);

    std::vector<uint8_t> result;
    if(buf && len > 0)
    {
      result.assign(buf, buf + len);
      xmlFree(buf);
    }
    xmlFreeDoc(subdoc);
    return result;
  }

  X509* parse_cert(const std::string& b64_cert)
  {
    std::string pem = "-----BEGIN CERTIFICATE-----\n" + b64_cert + "\n-----END CERTIFICATE-----\n";
    BIO* bio = BIO_new_mem_buf(pem.data(), static_cast<int>(pem.size()));
    X509* cert = PEM_read_bio_X509(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);
    return cert;
  }

  const EVP_MD* get_digest_from_uri(const std::string& uri)
  {
    if(uri.find("sha256") != std::string::npos || uri.find("SHA256") != std::string::npos)
      return EVP_sha256();
    if(uri.find("sha512") != std::string::npos || uri.find("SHA512") != std::string::npos)
      return EVP_sha512();
    if(uri.find("sha1") != std::string::npos || uri.find("SHA1") != std::string::npos)
      return EVP_sha1();
    return EVP_sha256();
  }

} // namespace

std::vector<Note> verify_signature(const std::filesystem::path& xml_file)
{
  std::vector<Note> notes;

  xmlDocPtr doc = xmlReadFile(xml_file.string().c_str(), nullptr,
                              XML_PARSE_NONET | XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
  if(!doc)
    return notes;

  xmlNodePtr root = xmlDocGetRootElement(doc);
  if(!root)
  {
    xmlFreeDoc(doc);
    return notes;
  }

  auto sig_node = find_child_recursive(root, "Signature");
  if(!sig_node)
  {
    xmlFreeDoc(doc);
    return notes;
  }

  auto signed_info = find_child(sig_node, "SignedInfo");
  if(!signed_info)
  {
    notes.push_back(
        {Severity::error, Code::signature_invalid, "Signature missing SignedInfo", xml_file});
    xmlFreeDoc(doc);
    return notes;
  }

  auto sig_value_node = find_child(sig_node, "SignatureValue");
  if(!sig_value_node)
  {
    notes.push_back(
        {Severity::error, Code::signature_invalid, "Signature missing SignatureValue", xml_file});
    xmlFreeDoc(doc);
    return notes;
  }
  auto sig_value = base64_decode(get_text(sig_value_node));
  if(sig_value.empty())
  {
    notes.push_back(
        {Severity::error, Code::signature_invalid, "Cannot decode SignatureValue", xml_file});
    xmlFreeDoc(doc);
    return notes;
  }

  // Extract signing certificate
  auto key_info = find_child(sig_node, "KeyInfo");
  auto x509_data = key_info ? find_child_recursive(key_info, "X509Data") : nullptr;
  auto cert_node = x509_data ? find_child(x509_data, "X509Certificate") : nullptr;

  if(!cert_node)
  {
    notes.push_back(
        {Severity::warning, Code::signature_invalid, "No X509Certificate in signature", xml_file});
    xmlFreeDoc(doc);
    return notes;
  }

  X509* cert = parse_cert(get_text(cert_node));
  if(!cert)
  {
    notes.push_back(
        {Severity::error, Code::signature_invalid, "Cannot parse X509 certificate", xml_file});
    xmlFreeDoc(doc);
    return notes;
  }

  // Check certificate validity
  time_t now = time(nullptr);
  if(X509_cmp_time(X509_get0_notAfter(cert), &now) < 0)
    notes.push_back({Severity::warning, Code::certificate_expired,
                     "Signing certificate has expired", xml_file});
  if(X509_cmp_time(X509_get0_notBefore(cert), &now) > 0)
    notes.push_back({Severity::warning, Code::certificate_expired,
                     "Signing certificate not yet valid", xml_file});

  // Determine algorithms
  auto sig_method = find_child(signed_info, "SignatureMethod");
  const EVP_MD* sig_md = get_digest_from_uri(sig_method ? get_attr(sig_method, "Algorithm") : "");

  auto c14n_method = find_child(signed_info, "CanonicalizationMethod");
  std::string c14n_uri = c14n_method ? get_attr(c14n_method, "Algorithm") : "";
  int c14n_mode =
      (c14n_uri.find("exc-c14n") != std::string::npos) ? XML_C14N_EXCLUSIVE_1_0 : XML_C14N_1_0;

  // Verify signature over canonicalized SignedInfo
  auto signed_info_c14n = canonicalize_subtree(doc, signed_info, c14n_mode);
  if(signed_info_c14n.empty())
  {
    notes.push_back(
        {Severity::error, Code::signature_invalid, "Failed to canonicalize SignedInfo", xml_file});
    X509_free(cert);
    xmlFreeDoc(doc);
    return notes;
  }

  EVP_PKEY* pkey = X509_get0_pubkey(cert);
  if(pkey)
  {
    EVP_MD_CTX* md_ctx = EVP_MD_CTX_new();
    bool sig_ok =
        (EVP_DigestVerifyInit(md_ctx, nullptr, sig_md, nullptr, pkey) == 1 &&
         EVP_DigestVerifyUpdate(md_ctx, signed_info_c14n.data(), signed_info_c14n.size()) == 1 &&
         EVP_DigestVerifyFinal(md_ctx, sig_value.data(), sig_value.size()) == 1);
    EVP_MD_CTX_free(md_ctx);

    if(!sig_ok)
    {
      notes.push_back(
          {Severity::error, Code::signature_invalid, "Signature verification failed", xml_file});
    }
  }

  // Verify Reference digests
  for(auto ref = signed_info->children; ref; ref = ref->next)
  {
    if(ref->type != XML_ELEMENT_NODE)
      continue;
    if(std::strcmp(reinterpret_cast<const char*>(ref->name), "Reference") != 0)
      continue;

    std::string uri = get_attr(ref, "URI");
    auto digest_method_node = find_child(ref, "DigestMethod");
    auto digest_value_node = find_child(ref, "DigestValue");
    if(!digest_value_node)
      continue;

    const EVP_MD* ref_md =
        get_digest_from_uri(digest_method_node ? get_attr(digest_method_node, "Algorithm") : "");
    auto expected_digest = base64_decode(get_text(digest_value_node));
    if(expected_digest.empty())
      continue;

    if(uri.empty() || uri[0] == '#')
    {
      xmlNodePtr target = nullptr;
      if(uri.empty())
      {
        target = root;
      }
      else
      {
        std::string id = uri.substr(1);
        xmlXPathContextPtr xctx = xmlXPathNewContext(doc);
        if(xctx)
        {
          std::string expr = "//*[@Id='" + id + "']";
          xmlXPathObjectPtr xobj = xmlXPathEvalExpression(BAD_CAST expr.c_str(), xctx);
          if(xobj && xobj->nodesetval && xobj->nodesetval->nodeNr > 0)
            target = xobj->nodesetval->nodeTab[0];
          if(xobj)
            xmlXPathFreeObject(xobj);
          xmlXPathFreeContext(xctx);
        }
      }

      if(target)
      {
        // Enveloped signature transform: remove Signature node temporarily
        xmlUnlinkNode(sig_node);

        int ref_c14n = c14n_mode;
        auto transforms = find_child(ref, "Transforms");
        if(transforms)
        {
          for(auto t = transforms->children; t; t = t->next)
          {
            if(t->type != XML_ELEMENT_NODE)
              continue;
            std::string algo = get_attr(t, "Algorithm");
            if(algo.find("exc-c14n") != std::string::npos)
              ref_c14n = XML_C14N_EXCLUSIVE_1_0;
          }
        }

        auto canon = canonicalize_subtree(doc, target, ref_c14n);

        // Re-attach signature
        xmlAddChild(root, sig_node);

        if(!canon.empty())
        {
          unsigned char digest[EVP_MAX_MD_SIZE];
          unsigned int digest_len = 0;
          EVP_Digest(canon.data(), canon.size(), digest, &digest_len, ref_md, nullptr);

          if(digest_len != expected_digest.size() ||
             std::memcmp(digest, expected_digest.data(), digest_len) != 0)
          {
            notes.push_back({Severity::error, Code::signature_invalid,
                             "Reference digest mismatch (URI=\"" + uri + "\")", xml_file});
          }
        }
      }
    }
  }

  X509_free(cert);
  xmlFreeDoc(doc);
  return notes;
}

std::vector<Note> verify_certificate_chain(const std::filesystem::path& xml_file)
{
  std::vector<Note> notes;

  xmlDocPtr doc = xmlReadFile(xml_file.string().c_str(), nullptr,
                              XML_PARSE_NONET | XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
  if(!doc)
    return notes;

  xmlNodePtr root = xmlDocGetRootElement(doc);
  if(!root)
  {
    xmlFreeDoc(doc);
    return notes;
  }

  auto sig_node = find_child_recursive(root, "Signature");
  if(!sig_node)
  {
    xmlFreeDoc(doc);
    return notes;
  }

  auto key_info = find_child(sig_node, "KeyInfo");
  if(!key_info)
  {
    xmlFreeDoc(doc);
    return notes;
  }

  // Collect all certificates in chain
  std::vector<X509*> certs;
  auto x509_data = find_child_recursive(key_info, "X509Data");
  if(x509_data)
  {
    for(auto child = x509_data->children; child; child = child->next)
    {
      if(child->type != XML_ELEMENT_NODE)
        continue;
      if(std::strcmp(reinterpret_cast<const char*>(child->name), "X509Certificate") != 0)
        continue;
      X509* c = parse_cert(get_text(child));
      if(c)
        certs.push_back(c);
    }
  }

  if(certs.size() >= 2)
  {
    // Verify chain: first cert is leaf, rest are intermediates/root
    X509_STORE* store = X509_STORE_new();
    for(size_t i = 1; i < certs.size(); ++i)
      X509_STORE_add_cert(store, certs[i]);

    X509_STORE_CTX* ctx = X509_STORE_CTX_new();
    X509_STORE_CTX_init(ctx, store, certs[0], nullptr);

    if(X509_verify_cert(ctx) != 1)
    {
      int err = X509_STORE_CTX_get_error(ctx);
      notes.push_back(
          {Severity::error, Code::certificate_chain_broken,
           std::string("Certificate chain invalid: ") + X509_verify_cert_error_string(err),
           xml_file});
    }

    X509_STORE_CTX_free(ctx);
    X509_STORE_free(store);
  }

  for(auto* c : certs)
    X509_free(c);
  xmlFreeDoc(doc);
  return notes;
}

} // namespace dcpdoctor
