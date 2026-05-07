#pragma once
// Platform compatibility helpers for Windows/POSIX differences

#ifdef _WIN32
#define DCPDOCTOR_POPEN _popen
#define DCPDOCTOR_PCLOSE _pclose
#define DCPDOCTOR_TIMEGM _mkgmtime
#else
#define DCPDOCTOR_POPEN popen
#define DCPDOCTOR_PCLOSE pclose
#define DCPDOCTOR_TIMEGM timegm
#endif

// Portable gmtime_r (MSVC uses gmtime_s with reversed arguments)
#ifdef _WIN32
#define DCPDOCTOR_GMTIME_R(timer, buf) (gmtime_s((buf), (timer)) == 0 ? (buf) : nullptr)
#else
#define DCPDOCTOR_GMTIME_R(timer, buf) gmtime_r((timer), (buf))
#endif
