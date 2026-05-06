#include "dcpdoctor/dcpdoctor.h"

namespace dcpdoctor {

std::string_view Note::severity_str() const {
    switch (severity) {
    case Severity::error: return "ERROR";
    case Severity::warning: return "WARNING";
    case Severity::info: return "INFO";
    }
    return "UNKNOWN";
}

std::string_view Note::code_str() const {
    switch (code) {
    case Code::missing_assetmap: return "missing_assetmap";
    case Code::missing_pkl: return "missing_pkl";
    case Code::missing_cpl: return "missing_cpl";
    case Code::asset_not_found: return "asset_not_found";
    case Code::duplicate_asset_id: return "duplicate_asset_id";
    case Code::xml_parse_error: return "xml_parse_error";
    case Code::xml_schema_violation: return "xml_schema_violation";
    case Code::invalid_uuid: return "invalid_uuid";
    case Code::missing_required_element: return "missing_required_element";
    case Code::pkl_hash_mismatch: return "pkl_hash_mismatch";
    case Code::pkl_missing_asset_reference: return "pkl_missing_asset_reference";
    case Code::cpl_invalid_duration: return "cpl_invalid_duration";
    case Code::cpl_mismatched_durations: return "cpl_mismatched_durations";
    case Code::cpl_missing_reel: return "cpl_missing_reel";
    case Code::cpl_invalid_frame_rate: return "cpl_invalid_frame_rate";
    case Code::cpl_invalid_edit_rate: return "cpl_invalid_edit_rate";
    case Code::mxf_unreadable: return "mxf_unreadable";
    case Code::mxf_hash_mismatch: return "mxf_hash_mismatch";
    case Code::signature_invalid: return "signature_invalid";
    case Code::certificate_expired: return "certificate_expired";
    case Code::certificate_chain_broken: return "certificate_chain_broken";
    case Code::smpte_naming_violation: return "smpte_naming_violation";
    case Code::smpte_namespace_wrong: return "smpte_namespace_wrong";
    case Code::interop_namespace_wrong: return "interop_namespace_wrong";
    case Code::picture_invalid_resolution: return "picture_invalid_resolution";
    case Code::picture_invalid_frame_rate: return "picture_invalid_frame_rate";
    case Code::j2k_bitrate_exceeded: return "j2k_bitrate_exceeded";
    case Code::j2k_invalid_profile: return "j2k_invalid_profile";
    case Code::j2k_invalid_component_count: return "j2k_invalid_component_count";
    case Code::sound_invalid_sample_rate: return "sound_invalid_sample_rate";
    case Code::sound_invalid_channel_count: return "sound_invalid_channel_count";
    case Code::subtitle_parse_error: return "subtitle_parse_error";
    case Code::subtitle_invalid_timing: return "subtitle_invalid_timing";
    case Code::subtitle_font_missing: return "subtitle_font_missing";
    case Code::isdcf_naming_violation: return "isdcf_naming_violation";
    case Code::encryption_detected: return "encryption_detected";
    case Code::kdm_required: return "kdm_required";
    case Code::reel_discontinuity: return "reel_discontinuity";
    case Code::stereo_mismatch: return "stereo_mismatch";
    case Code::marker_missing: return "marker_missing";
    case Code::marker_invalid: return "marker_invalid";
    case Code::cross_ref_broken: return "cross_ref_broken";
    case Code::supplemental_opl_missing: return "supplemental_opl_missing";
    }
    return "unknown";
}

void VerifyResult::add(Note note) {
    switch (note.severity) {
    case Severity::error: ++error_count; break;
    case Severity::warning: ++warning_count; break;
    case Severity::info: break;
    }
    notes.push_back(std::move(note));
}

} // namespace dcpdoctor
