#include "dcpdoctor/quality.h"
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/pem.h>
#include <openssl/bio.h>
#include <openssl/sha.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cmath>

namespace dcpdoctor {
namespace fs = std::filesystem;

// Known SMPTE DC root certificate thumbprints
static const char* smpte_root_thumbprints[] = {
    // SMPTE DC root CA (SHA-1 fingerprints)
    "a178bdc1bd18c9d5f1e1cf12ec4cee71fbe50237",
    "44c8a37b7f3f5e4d9c3ba7e91dfd7d64a83c5c14",
    nullptr
};

J2kQualityScore estimate_j2k_quality(double bitrate_mbps, uint32_t width,
                                      uint32_t height, double frame_rate,
                                      uint8_t bit_depth, uint8_t num_components) {
    J2kQualityScore score;
    score.valid = true;

    // Calculate bits per pixel
    double pixels_per_frame = double(width) * double(height);
    double bits_per_frame = (bitrate_mbps * 1e6) / frame_rate;
    score.bits_per_pixel = bits_per_frame / pixels_per_frame;

    // Compression ratio relative to uncompressed
    double uncompressed_bpp = double(bit_depth) * double(num_components);
    score.compression_ratio = uncompressed_bpp / score.bits_per_pixel;

    // Quality assessment based on bits per pixel
    // J2K visually lossless typically needs >= 1.5 bpp for cinema
    if (score.bits_per_pixel >= 1.5) {
        score.quality_grade = "Excellent";
        score.quality_score = 90.0 + std::min(10.0, (score.bits_per_pixel - 1.5) * 10.0);
        score.estimated_psnr_db = 50.0 + (score.bits_per_pixel - 1.5) * 5.0;
    } else if (score.bits_per_pixel >= 1.0) {
        score.quality_grade = "Good";
        score.quality_score = 70.0 + (score.bits_per_pixel - 1.0) * 40.0;
        score.estimated_psnr_db = 44.0 + (score.bits_per_pixel - 1.0) * 12.0;
    } else if (score.bits_per_pixel >= 0.5) {
        score.quality_grade = "Fair";
        score.quality_score = 50.0 + (score.bits_per_pixel - 0.5) * 40.0;
        score.estimated_psnr_db = 38.0 + (score.bits_per_pixel - 0.5) * 12.0;
    } else {
        score.quality_grade = "Poor";
        score.quality_score = std::max(10.0, score.bits_per_pixel * 100.0);
        score.estimated_psnr_db = 30.0 + score.bits_per_pixel * 16.0;
    }

    // Clamp
    if (score.estimated_psnr_db > 55.0) score.estimated_psnr_db = 55.0;
    if (score.estimated_psnr_db < 25.0) score.estimated_psnr_db = 25.0;
    if (score.quality_score > 100.0) score.quality_score = 100.0;

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << score.bits_per_pixel
        << " bpp, ~" << int(score.estimated_psnr_db) << " dB PSNR, "
        << std::setprecision(1) << score.compression_ratio << ":1 compression";
    score.notes = oss.str();

    return score;
}

CertChainInfo validate_cert_chain(const fs::path& dcp_dir) {
    CertChainInfo info;

    // Look for certificate files in the DCP directory
    std::error_code ec;
    fs::path cert_path;
    for (auto& entry : fs::directory_iterator(dcp_dir, ec)) {
        if (!entry.is_regular_file()) continue;
        auto ext = entry.path().extension().string();
        if (ext == ".pem" || ext == ".crt") {
            cert_path = entry.path();
            break;
        }
    }

    // Also check for certificates embedded in XML signatures
    if (cert_path.empty()) {
        // Look in CPL/PKL XML files for X509Certificate elements
        for (auto& entry : fs::directory_iterator(dcp_dir, ec)) {
            if (!entry.is_regular_file()) continue;
            if (entry.path().extension() != ".xml") continue;
            // For now, note that embedded certs would be extracted here
            cert_path = entry.path();
            break;
        }
    }

    if (cert_path.empty()) {
        info.error = "No certificate files found in DCP";
        return info;
    }

    // Try to read PEM certificates
    FILE* fp = fopen(cert_path.string().c_str(), "r");
    if (!fp) {
        info.error = "Cannot open certificate file";
        return info;
    }

    std::vector<X509*> certs;
    while (auto cert = PEM_read_X509(fp, nullptr, nullptr, nullptr)) {
        certs.push_back(cert);
    }
    fclose(fp);

    if (certs.empty()) {
        info.error = "No PEM certificates found";
        return info;
    }

    info.chain_length = certs.size();

    // Get leaf cert info
    if (!certs.empty()) {
        char* subject = X509_NAME_oneline(X509_get_subject_name(certs[0]), nullptr, 0);
        if (subject) { info.leaf_cn = subject; OPENSSL_free(subject); }
    }

    // Get root cert info
    if (certs.size() > 1) {
        auto root = certs.back();
        char* subject = X509_NAME_oneline(X509_get_subject_name(root), nullptr, 0);
        if (subject) { info.root_cn = subject; OPENSSL_free(subject); }

        // Check if root is self-signed
        bool self_signed = (X509_check_issued(root, root) == X509_V_OK);

        // Check root against SMPTE trusted roots
        if (self_signed) {
            unsigned char md[SHA_DIGEST_LENGTH];
            unsigned int n;
            X509_digest(root, EVP_sha1(), md, &n);

            std::ostringstream oss;
            for (unsigned int i = 0; i < n; ++i)
                oss << std::hex << std::setfill('0') << std::setw(2) << int(md[i]);

            info.root_trusted = is_smpte_trusted_root(oss.str());
        }
    }

    // Collect all subjects
    for (auto cert : certs) {
        char* subject = X509_NAME_oneline(X509_get_subject_name(cert), nullptr, 0);
        if (subject) {
            info.chain_subjects.push_back(subject);
            OPENSSL_free(subject);
        }
    }

    // Verify chain continuity
    info.chain_complete = true;
    for (size_t i = 0; i + 1 < certs.size(); ++i) {
        if (X509_check_issued(certs[i + 1], certs[i]) != X509_V_OK) {
            info.chain_complete = false;
            break;
        }
    }

    // Get issuer org from leaf
    if (!certs.empty()) {
        auto issuer_name = X509_get_issuer_name(certs[0]);
        int idx = X509_NAME_get_index_by_NID(issuer_name, NID_organizationName, -1);
        if (idx >= 0) {
            auto entry = X509_NAME_get_entry(issuer_name, idx);
            if (entry) {
                auto data = X509_NAME_ENTRY_get_data(entry);
                if (data) {
                    unsigned char* utf8 = nullptr;
                    int len = ASN1_STRING_to_UTF8(&utf8, data);
                    if (len > 0 && utf8) {
                        info.issuer_org = std::string(reinterpret_cast<char*>(utf8), len);
                        OPENSSL_free(utf8);
                    }
                }
            }
        }
    }

    for (auto cert : certs) X509_free(cert);

    info.valid = true;
    return info;
}

std::vector<Note> check_cert_chain_compliance(const CertChainInfo& info,
                                               const fs::path& dcp_dir) {
    std::vector<Note> notes;

    if (!info.valid) {
        notes.push_back(Note{Severity::error, Code::signature_invalid,
                        "Certificate chain error: " + info.error, dcp_dir});
        return notes;
    }

    notes.push_back(Note{Severity::info, Code::signature_invalid,
                    "Certificate chain: " + std::to_string(info.chain_length) +
                    " certificates", dcp_dir});

    if (!info.chain_complete) {
        notes.push_back(Note{Severity::error, Code::certificate_chain_broken,
                        "Certificate chain is broken (issuer mismatch)", dcp_dir});
    }

    if (!info.root_trusted) {
        notes.push_back(Note{Severity::warning, Code::certificate_chain_broken,
                        "Root certificate not in SMPTE trusted root list", dcp_dir});
    }

    if (!info.leaf_cn.empty()) {
        notes.push_back(Note{Severity::info, Code::signature_invalid,
                        "Leaf certificate: " + info.leaf_cn, dcp_dir});
    }

    return notes;
}

bool is_smpte_trusted_root(const std::string& cert_thumbprint) {
    for (int i = 0; smpte_root_thumbprints[i]; ++i) {
        if (cert_thumbprint == smpte_root_thumbprints[i])
            return true;
    }
    return false;
}

} // namespace dcpdoctor
