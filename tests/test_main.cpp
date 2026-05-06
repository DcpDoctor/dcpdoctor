#include "dcpdoctor/dcpdoctor.h"
#include "dcpdoctor/j2k.h"
#include "dcpdoctor/isdcf.h"
#include "dcpdoctor/subtitle.h"
#include "dcpdoctor/validators.h"
#include "dcpdoctor/mxf.h"
#include <cassert>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) \
    static void test_##name(); \
    struct test_reg_##name { test_reg_##name() { test_##name(); } } test_inst_##name; \
    static void test_##name()

#define ASSERT(cond) do { \
    ++tests_run; \
    if (!(cond)) { \
        std::cerr << "FAIL: " << #cond << " at " << __FILE__ << ":" << __LINE__ << "\n"; \
    } else { \
        ++tests_passed; \
    } \
} while(0)

TEST(nonexistent_directory) {
    auto result = dcpdoctor::verify("/tmp/nonexistent_dcp_path_12345");
    ASSERT(!result.ok());
    ASSERT(result.error_count > 0);
    ASSERT(result.notes[0].code == dcpdoctor::Code::missing_assetmap);
}

TEST(note_severity_str) {
    dcpdoctor::Note n{dcpdoctor::Severity::error, dcpdoctor::Code::missing_assetmap, "test"};
    ASSERT(n.severity_str() == "ERROR");
    n.severity = dcpdoctor::Severity::warning;
    ASSERT(n.severity_str() == "WARNING");
    n.severity = dcpdoctor::Severity::info;
    ASSERT(n.severity_str() == "INFO");
}

TEST(verify_result_add) {
    dcpdoctor::VerifyResult result;
    ASSERT(result.ok());
    ASSERT(result.error_count == 0);
    ASSERT(result.warning_count == 0);

    result.add({dcpdoctor::Severity::warning, dcpdoctor::Code::pkl_hash_mismatch, "test"});
    ASSERT(result.ok());
    ASSERT(result.warning_count == 1);

    result.add({dcpdoctor::Severity::error, dcpdoctor::Code::missing_pkl, "test"});
    ASSERT(!result.ok());
    ASSERT(result.error_count == 1);
}

// --- J2K codestream parser tests ---

TEST(j2k_parse_valid_header) {
    // Minimal valid J2K codestream: SOC + SIZ marker
    // SOC(FF4F) + SIZ(FF51) + len(0029=41) + rsiz(0003=Cinema2K) +
    // Xsiz(00000800=2048) + Ysiz(00000438=1080) +
    // XOsiz(0) + YOsiz(0) + XTsiz(00000800) + YTsiz(00000438) +
    // XTOsiz(0) + YTOsiz(0) + Csiz(0003=3 components) +
    // Ssiz(0B=12bit) + XRsiz(01) + YRsiz(01) x3
    uint8_t data[] = {
        0xFF, 0x4F,  // SOC
        0xFF, 0x51,  // SIZ
        0x00, 0x2F,  // Length = 47
        0x00, 0x03,  // Rsiz = Cinema2K
        0x00, 0x00, 0x08, 0x00,  // Xsiz = 2048
        0x00, 0x00, 0x04, 0x38,  // Ysiz = 1080
        0x00, 0x00, 0x00, 0x00,  // XOsiz = 0
        0x00, 0x00, 0x00, 0x00,  // YOsiz = 0
        0x00, 0x00, 0x08, 0x00,  // XTsiz = 2048
        0x00, 0x00, 0x04, 0x38,  // YTsiz = 1080
        0x00, 0x00, 0x00, 0x00,  // XTOsiz = 0
        0x00, 0x00, 0x00, 0x00,  // YTOsiz = 0
        0x00, 0x03,              // Csiz = 3
        0x0B, 0x01, 0x01,       // Component 0: 12bit, XR=1, YR=1
        0x0B, 0x01, 0x01,       // Component 1: 12bit, XR=1, YR=1
        0x0B, 0x01, 0x01,       // Component 2: 12bit, XR=1, YR=1
    };

    auto info = dcpdoctor::parse_j2k_header(data, sizeof(data));
    ASSERT(info.valid);
    ASSERT(info.width == 2048);
    ASSERT(info.height == 1080);
    ASSERT(info.num_components == 3);
    ASSERT(info.bit_depth == 12);
    ASSERT(info.rsiz == dcpdoctor::RSIZ_CINEMA_2K);
}

TEST(j2k_parse_too_small) {
    uint8_t data[] = {0xFF};
    auto info = dcpdoctor::parse_j2k_header(data, 1);
    ASSERT(!info.valid);
    ASSERT(!info.error.empty());
}

TEST(j2k_parse_bad_soc) {
    uint8_t data[] = {0x00, 0x00, 0xFF, 0x51};
    auto info = dcpdoctor::parse_j2k_header(data, 4);
    ASSERT(!info.valid);
    ASSERT(info.error == "Missing SOC marker");
}

TEST(j2k_bitrate_under_limit) {
    // Create a temp file that's small enough to be under 250 Mbps
    auto tmp = fs::temp_directory_path() / "dcpdoctor_test_bitrate.mxf";
    {
        std::ofstream f(tmp, std::ios::binary);
        std::vector<char> data(1024 * 1024, 0);  // 1MB file
        f.write(data.data(), data.size());
    }
    // 1MB, 24 frames, 24fps = 1 second = ~8 Mbps << 250 Mbps
    auto notes = dcpdoctor::check_j2k_bitrate(tmp, 24, 24, 1, 2048, 1080);
    ASSERT(notes.empty());
    fs::remove(tmp);
}

// --- ISDCF naming tests ---

TEST(isdcf_valid_name) {
    auto notes = dcpdoctor::check_isdcf_naming(
        "MyMovie_FTR_F_EN_US_51_2K_ST_20230101_FAC_SMPTE_OV", "cpl.xml");
    // Should have no errors/warnings (may have info)
    bool has_error = false;
    for (const auto& n : notes)
        if (n.severity == dcpdoctor::Severity::error) has_error = true;
    ASSERT(!has_error);
}

TEST(isdcf_no_underscores) {
    auto notes = dcpdoctor::check_isdcf_naming("Just A Movie Title", "cpl.xml");
    ASSERT(!notes.empty());
    ASSERT(notes[0].code == dcpdoctor::Code::isdcf_naming_violation);
}

TEST(isdcf_bad_content_type) {
    auto notes = dcpdoctor::check_isdcf_naming("Movie_XXX_F_EN", "cpl.xml");
    bool found_type_warning = false;
    for (const auto& n : notes)
        if (n.message.find("content type") != std::string::npos)
            found_type_warning = true;
    ASSERT(found_type_warning);
}

TEST(isdcf_empty_title) {
    auto notes = dcpdoctor::check_isdcf_naming("", "cpl.xml");
    ASSERT(!notes.empty());
}

// --- Subtitle validation tests ---

TEST(subtitle_valid_smpte) {
    auto tmp = fs::temp_directory_path() / "dcpdoctor_test_sub.xml";
    {
        std::ofstream f(tmp);
        f << R"(<?xml version="1.0" encoding="UTF-8"?>
<SubtitleReel xmlns="http://www.smpte-ra.org/schemas/428-7/2010/DCST">
  <Id>urn:uuid:12345678-1234-1234-1234-123456789012</Id>
  <EditRate>24 1</EditRate>
  <TimeCodeRate>24</TimeCodeRate>
  <LoadFont Id="arial" URI="arial.ttf"/>
  <SubtitleList>
    <Font Id="arial" Size="42">
      <Subtitle TimeIn="00:00:01:00" TimeOut="00:00:03:00">
        <Text>Hello World</Text>
      </Subtitle>
    </Font>
  </SubtitleList>
</SubtitleReel>)";
    }
    auto notes = dcpdoctor::validate_subtitle(tmp, dcpdoctor::Standard::smpte);
    bool has_error = false;
    for (const auto& n : notes)
        if (n.severity == dcpdoctor::Severity::error) has_error = true;
    ASSERT(!has_error);
    fs::remove(tmp);
}

TEST(subtitle_missing_id) {
    auto tmp = fs::temp_directory_path() / "dcpdoctor_test_sub2.xml";
    {
        std::ofstream f(tmp);
        f << R"(<?xml version="1.0" encoding="UTF-8"?>
<SubtitleReel xmlns="http://www.smpte-ra.org/schemas/428-7/2010/DCST">
  <EditRate>24 1</EditRate>
  <TimeCodeRate>24</TimeCodeRate>
  <SubtitleList>
    <Subtitle TimeIn="00:00:01:00" TimeOut="00:00:03:00">
      <Text>Hello</Text>
    </Subtitle>
  </SubtitleList>
</SubtitleReel>)";
    }
    auto notes = dcpdoctor::validate_subtitle(tmp, dcpdoctor::Standard::smpte);
    bool found_id_error = false;
    for (const auto& n : notes)
        if (n.code == dcpdoctor::Code::missing_required_element &&
            n.message.find("Id") != std::string::npos)
            found_id_error = true;
    ASSERT(found_id_error);
    fs::remove(tmp);
}

TEST(subtitle_bad_timing) {
    auto tmp = fs::temp_directory_path() / "dcpdoctor_test_sub3.xml";
    {
        std::ofstream f(tmp);
        f << R"(<?xml version="1.0" encoding="UTF-8"?>
<SubtitleReel xmlns="http://www.smpte-ra.org/schemas/428-7/2010/DCST">
  <Id>urn:uuid:12345678-1234-1234-1234-123456789012</Id>
  <EditRate>24 1</EditRate>
  <TimeCodeRate>24</TimeCodeRate>
  <SubtitleList>
    <Subtitle TimeIn="bad" TimeOut="also bad">
      <Text>Hello</Text>
    </Subtitle>
  </SubtitleList>
</SubtitleReel>)";
    }
    auto notes = dcpdoctor::validate_subtitle(tmp, dcpdoctor::Standard::smpte);
    bool found_timing = false;
    for (const auto& n : notes)
        if (n.code == dcpdoctor::Code::subtitle_invalid_timing)
            found_timing = true;
    ASSERT(found_timing);
    fs::remove(tmp);
}

// --- Reel continuity tests ---

TEST(reel_continuity_single_reel) {
    auto tmp = fs::temp_directory_path() / "dcpdoctor_test_reel.xml";
    {
        std::ofstream f(tmp);
        f << R"(<?xml version="1.0" encoding="UTF-8"?>
<CompositionPlaylist xmlns="http://www.smpte-ra.org/schemas/429-7/2006/CPL">
  <ReelList>
    <Reel>
      <AssetList>
        <MainPicture>
          <EntryPoint>0</EntryPoint>
          <Duration>240</Duration>
        </MainPicture>
      </AssetList>
    </Reel>
  </ReelList>
</CompositionPlaylist>)";
    }
    auto notes = dcpdoctor::check_reel_continuity(tmp);
    ASSERT(notes.empty());  // Single reel, nothing to check
    fs::remove(tmp);
}

// --- Marker tests ---

TEST(markers_strict_missing) {
    auto tmp = fs::temp_directory_path() / "dcpdoctor_test_markers.xml";
    {
        std::ofstream f(tmp);
        f << R"(<?xml version="1.0" encoding="UTF-8"?>
<CompositionPlaylist xmlns="http://www.smpte-ra.org/schemas/429-7/2006/CPL">
  <ReelList>
    <Reel>
      <AssetList>
        <MainMarkers>
          <Marker>
            <Label>FFMC</Label>
            <Offset>0</Offset>
          </Marker>
        </MainMarkers>
      </AssetList>
    </Reel>
  </ReelList>
</CompositionPlaylist>)";
    }
    auto notes = dcpdoctor::check_markers(tmp, true);
    // Should warn about missing LFMC
    bool found_lfmc = false;
    for (const auto& n : notes)
        if (n.message.find("LFMC") != std::string::npos)
            found_lfmc = true;
    ASSERT(found_lfmc);
    fs::remove(tmp);
}

// --- Encryption detection tests ---

TEST(encryption_detected) {
    auto tmp_dir = fs::temp_directory_path() / "dcpdoctor_test_enc";
    fs::create_directories(tmp_dir);
    auto cpl = tmp_dir / "cpl.xml";
    {
        std::ofstream f(cpl);
        f << R"(<?xml version="1.0" encoding="UTF-8"?>
<CompositionPlaylist xmlns="http://www.smpte-ra.org/schemas/429-7/2006/CPL">
  <ReelList>
    <Reel>
      <AssetList>
        <MainPicture>
          <KeyId>urn:uuid:aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee</KeyId>
        </MainPicture>
      </AssetList>
    </Reel>
  </ReelList>
</CompositionPlaylist>)";
    }
    std::vector<fs::path> cpls = {cpl};
    auto notes = dcpdoctor::check_encryption(tmp_dir, cpls);
    bool found_enc = false;
    for (const auto& n : notes)
        if (n.code == dcpdoctor::Code::encryption_detected)
            found_enc = true;
    ASSERT(found_enc);
    fs::remove_all(tmp_dir);
}

// --- Essence type detection ---

TEST(essence_type_str) {
    ASSERT(dcpdoctor::essence_type_str(dcpdoctor::EssenceType::jpeg2000) == "JPEG 2000");
    ASSERT(dcpdoctor::essence_type_str(dcpdoctor::EssenceType::pcm_audio) == "PCM Audio");
    ASSERT(dcpdoctor::essence_type_str(dcpdoctor::EssenceType::dolby_atmos) == "Dolby Atmos (IAB)");
    ASSERT(dcpdoctor::essence_type_str(dcpdoctor::EssenceType::timed_text) == "Timed Text");
}

// --- Integration tests ---

TEST(valid_smpte_fixture) {
    auto fixtures = fs::path(__FILE__).parent_path() / "fixtures" / "valid_smpte";
    if (!fs::exists(fixtures)) return;  // Skip if fixtures not available

    auto result = dcpdoctor::verify(fixtures);
    ASSERT(result.ok());
    ASSERT(result.standard == dcpdoctor::Standard::smpte);
}

TEST(bad_hash_fixture) {
    auto fixtures = fs::path(__FILE__).parent_path() / "fixtures" / "bad_hash";
    if (!fs::exists(fixtures)) return;  // Skip if fixtures not available

    auto result = dcpdoctor::verify(fixtures);
    ASSERT(!result.ok());
    bool found_hash = false;
    for (const auto& n : result.notes)
        if (n.code == dcpdoctor::Code::pkl_hash_mismatch)
            found_hash = true;
    ASSERT(found_hash);
}

// --- Code string coverage ---

TEST(code_str_coverage) {
    dcpdoctor::Note n{dcpdoctor::Severity::info, dcpdoctor::Code::j2k_bitrate_exceeded, "test"};
    ASSERT(n.code_str() == "j2k_bitrate_exceeded");
    n.code = dcpdoctor::Code::subtitle_parse_error;
    ASSERT(n.code_str() == "subtitle_parse_error");
    n.code = dcpdoctor::Code::isdcf_naming_violation;
    ASSERT(n.code_str() == "isdcf_naming_violation");
    n.code = dcpdoctor::Code::encryption_detected;
    ASSERT(n.code_str() == "encryption_detected");
    n.code = dcpdoctor::Code::reel_discontinuity;
    ASSERT(n.code_str() == "reel_discontinuity");
    n.code = dcpdoctor::Code::stereo_mismatch;
    ASSERT(n.code_str() == "stereo_mismatch");
    n.code = dcpdoctor::Code::marker_missing;
    ASSERT(n.code_str() == "marker_missing");
    n.code = dcpdoctor::Code::cross_ref_broken;
    ASSERT(n.code_str() == "cross_ref_broken");
    n.code = dcpdoctor::Code::supplemental_opl_missing;
    ASSERT(n.code_str() == "supplemental_opl_missing");
}

int main() {
    std::cout << tests_passed << "/" << tests_run << " tests passed\n";
    return (tests_passed == tests_run) ? 0 : 1;
}
