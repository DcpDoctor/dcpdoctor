#include "dcpdoctor/theater.h"
#include <algorithm>
#include <cctype>

namespace dcpdoctor {
namespace fs = std::filesystem;

std::vector<TheaterProfile> get_theater_profiles() {
    return {
        {
            "Dolby IMS3000",
            "Dolby",
            true,   // requires_bv21
            true,   // supports_interop
            true,   // supports_hfr
            true,   // supports_4k
            true,   // supports_atmos
            64,     // max_channels (Atmos)
            500,    // max_bitrate_mbps (4K)
            {"Requires BV2.1 for Atmos content",
             "May reject DCPs with non-standard MCA labels",
             "Requires ASSETMAP.xml naming"}
        },
        {
            "Dolby DSS/IMS2000",
            "Dolby",
            false,
            true,
            false,  // no HFR
            false,  // no 4K
            true,   // supports Atmos (limited)
            16,
            250,
            {"Limited Atmos support (bed channels only on older firmware)",
             "May have issues with 48fps content"}
        },
        {
            "Barco SP4K",
            "Barco",
            true,
            true,
            true,
            true,
            false,  // no native Atmos
            16,
            500,
            {"Requires SMPTE standard for 4K",
             "No native Dolby Atmos (requires external processor)",
             "Some firmware versions have subtitle timing issues"}
        },
        {
            "Barco SP2K",
            "Barco",
            false,
            true,
            false,
            false,
            false,
            8,
            250,
            {"Limited to 2K projection",
             "May not support all SMPTE subtitle features"}
        },
        {
            "Christie CP4440-RGB",
            "Christie",
            true,
            true,
            true,
            true,
            false,
            16,
            500,
            {"No native Atmos support",
             "Requires external IMB for content decryption",
             "4K RGB laser - ensure correct color profile"}
        },
        {
            "Christie CP2230",
            "Christie",
            false,
            true,
            false,
            false,
            false,
            8,
            250,
            {"Legacy 2K DLP projection",
             "Limited subtitle font support",
             "No HFR capability"}
        },
        {
            "GDC SX-4000",
            "GDC",
            true,
            true,
            true,
            true,
            true,
            64,
            500,
            {"Full Atmos support with integrated processor",
             "Strict BV2.1 enforcement on latest firmware",
             "Known issue: rejects PKLs without .xml extension"}
        },
        {
            "GDC SR-1000",
            "GDC",
            false,
            true,
            false,
            false,
            false,
            8,
            250,
            {"Legacy server - limited feature support",
             "May have issues with large CPLs (>50 reels)",
             "No subtitle font embedding support"}
        },
        {
            "Dolby Cinema (Premium)",
            "Dolby",
            true,
            false,  // No interop
            true,
            true,
            true,
            128,    // Atmos objects
            500,
            {"SMPTE-only (rejects Interop)",
             "Requires BV2.1 compliance",
             "Dolby Vision HDR metadata required for premium experience",
             "Strict audio channel labeling (MCA) required"}
        },
        {
            "IMAX Digital",
            "IMAX",
            true,
            false,  // No interop
            true,
            true,
            false,
            12,     // IMAX 12-channel
            500,
            {"SMPTE-only",
             "Requires IMAX-specific audio channel layout",
             "12-channel audio configuration mandatory",
             "Higher frame rate requirements for IMAX Enhanced"}
        }
    };
}

const TheaterProfile* find_profile(const std::string& query) {
    static auto profiles = get_theater_profiles();

    std::string lower_query;
    lower_query.reserve(query.size());
    for (char c : query) lower_query += std::tolower(c);

    for (const auto& profile : profiles) {
        std::string lower_name;
        lower_name.reserve(profile.name.size());
        for (char c : profile.name) lower_name += std::tolower(c);

        if (lower_name.find(lower_query) != std::string::npos)
            return &profile;

        std::string lower_vendor;
        for (char c : profile.vendor) lower_vendor += std::tolower(c);
        if (lower_vendor == lower_query)
            return &profile;
    }
    return nullptr;
}

std::vector<Note> check_theater_compatibility(const fs::path& dcp_dir,
                                               const VerifyResult& result,
                                               const TheaterProfile& profile) {
    std::vector<Note> notes;

    // Check standard compatibility
    if (!profile.supports_interop && result.standard == Standard::interop) {
        notes.push_back(Note{Severity::error, Code::smpte_namespace_wrong,
                        profile.name + " does not support Interop standard",
                        dcp_dir});
    }

    if (profile.requires_bv21 && result.standard == Standard::smpte) {
        // Check BV2.1 requirements
        if (!fs::exists(dcp_dir / "ASSETMAP.xml")) {
            notes.push_back(Note{Severity::warning, Code::smpte_naming_violation,
                            profile.name + " requires BV2.1 (ASSETMAP.xml naming)",
                            dcp_dir});
        }
    }

    // Check for known issues
    if (!profile.known_issues.empty()) {
        for (const auto& issue : profile.known_issues) {
            notes.push_back(Note{Severity::info, Code::smpte_namespace_wrong,
                            profile.name + " note: " + issue, dcp_dir});
        }
    }

    return notes;
}

} // namespace dcpdoctor
