// SWIG interface file for DCP Doctor Python bindings
%module dcpdoctor

%{
#include "dcpdoctor/validate.h"
#include "dcpdoctor/info.h"
#include "dcpdoctor/hash.h"
#include "dcpdoctor/cpl.h"
#include "dcpdoctor/pkl.h"
#include "dcpdoctor/assetmap.h"
#include "dcpdoctor/mxf.h"
#include "dcpdoctor/j2k.h"
#include "dcpdoctor/subtitle.h"
#include "dcpdoctor/audio.h"
#include "dcpdoctor/bitrate.h"
#include "dcpdoctor/kdm.h"
#include "dcpdoctor/kdm_advanced.h"
#include "dcpdoctor/report.h"
#include "dcpdoctor/qc_report.h"
#include "dcpdoctor/qc.h"
#include "dcpdoctor/compliance.h"
#include "dcpdoctor/diff.h"
#include "dcpdoctor/fixes.h"
#include "dcpdoctor/isdcf.h"
#include "dcpdoctor/timeline.h"
#include "dcpdoctor/signature.h"
#include "dcpdoctor/schema_validator.h"
#include "dcpdoctor/schema_validate.h"
#include "dcpdoctor/checksum_verify.h"
#include "dcpdoctor/loudness.h"
#include "dcpdoctor/hdr_validate.h"
#include "dcpdoctor/hfr_stereo.h"
#include "dcpdoctor/av_sync.h"
#include "dcpdoctor/frame_compare.h"
#include "dcpdoctor/mxf_extract.h"
#include "dcpdoctor/mxf_advanced.h"
#include "dcpdoctor/advanced.h"
#include "dcpdoctor/cache.h"
#include "dcpdoctor/theater.h"
#include "dcpdoctor/server.h"
#include "dcpdoctor/auto_qc.h"
#include "dcpdoctor/imf.h"
#include "dcpdoctor/imf_compliance.h"
%}

// STL support
%include "std_string.i"
%include "std_vector.i"
%include "stdint.i"

// Map std::filesystem::path to/from Python str
%typemap(in) std::filesystem::path {
  if (!PyUnicode_Check($input)) {
    PyErr_SetString(PyExc_TypeError, "Expected a string");
    SWIG_fail;
  }
  $1 = std::filesystem::path(PyUnicode_AsUTF8($input));
}
%typemap(in) const std::filesystem::path& (std::filesystem::path temp) {
  if (!PyUnicode_Check($input)) {
    PyErr_SetString(PyExc_TypeError, "Expected a string");
    SWIG_fail;
  }
  temp = std::filesystem::path(PyUnicode_AsUTF8($input));
  $1 = &temp;
}
%typemap(out) std::filesystem::path {
  $result = PyUnicode_FromString($1.string().c_str());
}
%typemap(typecheck, precedence=SWIG_TYPECHECK_STRING) std::filesystem::path, const std::filesystem::path& {
  $1 = PyUnicode_Check($input) ? 1 : 0;
}

// Template instantiations
%template(StringVector) std::vector<std::string>;

// Parse the headers
%include "dcpdoctor/validate.h"
%include "dcpdoctor/info.h"
%include "dcpdoctor/hash.h"
%include "dcpdoctor/cpl.h"
%include "dcpdoctor/pkl.h"
%include "dcpdoctor/assetmap.h"
%include "dcpdoctor/mxf.h"
%include "dcpdoctor/j2k.h"
%include "dcpdoctor/subtitle.h"
%include "dcpdoctor/audio.h"
%include "dcpdoctor/bitrate.h"
%include "dcpdoctor/kdm.h"
%include "dcpdoctor/kdm_advanced.h"
%include "dcpdoctor/report.h"
%include "dcpdoctor/qc_report.h"
%include "dcpdoctor/qc.h"
%include "dcpdoctor/compliance.h"
%include "dcpdoctor/diff.h"
%include "dcpdoctor/fixes.h"
%include "dcpdoctor/isdcf.h"
%include "dcpdoctor/timeline.h"
%include "dcpdoctor/signature.h"
%include "dcpdoctor/schema_validator.h"
%include "dcpdoctor/schema_validate.h"
%include "dcpdoctor/checksum_verify.h"
%include "dcpdoctor/loudness.h"
%include "dcpdoctor/hdr_validate.h"
%include "dcpdoctor/hfr_stereo.h"
%include "dcpdoctor/av_sync.h"
%include "dcpdoctor/frame_compare.h"
%include "dcpdoctor/mxf_extract.h"
%include "dcpdoctor/mxf_advanced.h"
%include "dcpdoctor/advanced.h"
%include "dcpdoctor/cache.h"
%include "dcpdoctor/theater.h"
%include "dcpdoctor/server.h"
%include "dcpdoctor/auto_qc.h"
%include "dcpdoctor/imf.h"
%include "dcpdoctor/imf_compliance.h"
