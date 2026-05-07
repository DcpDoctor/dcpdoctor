#pragma once

#include "dcpdoctor/dcpdoctor.h"
#include <filesystem>
#include <string>
#include <vector>

namespace dcpdoctor
{

/// J2K perceptual quality estimate (from codestream parameters)
struct J2kQualityScore
{
  bool valid = false;
  double estimated_psnr_db = 0.0; // Estimated PSNR from bitrate
  double quality_score = 0.0; // 0-100 quality rating
  std::string quality_grade; // "Excellent", "Good", "Fair", "Poor"
  double compression_ratio = 0.0;
  double bits_per_pixel = 0.0;
  std::string notes;
};

/// Estimate J2K quality from bitrate and encoding parameters
J2kQualityScore estimate_j2k_quality(double bitrate_mbps, uint32_t width, uint32_t height,
                                     double frame_rate, uint8_t bit_depth, uint8_t num_components);

/// Content certificate chain validation
struct CertChainInfo
{
  bool valid = false;
  int chain_length = 0;
  bool root_trusted = false;
  bool chain_complete = false;
  std::string leaf_cn; // Common Name of leaf cert
  std::string root_cn; // Common Name of root cert
  std::string issuer_org;
  std::vector<std::string> chain_subjects;
  std::string error;
};

/// Validate the full certificate chain in a DCP against SMPTE root certificates
CertChainInfo validate_cert_chain(const std::filesystem::path& dcp_dir);

/// Check certificate chain against known SMPTE root certificates
std::vector<Note> check_cert_chain_compliance(const CertChainInfo& info,
                                              const std::filesystem::path& dcp_dir);

/// SMPTE DC root certificate thumbprints (for trust validation)
bool is_smpte_trusted_root(const std::string& cert_thumbprint);

} // namespace dcpdoctor
