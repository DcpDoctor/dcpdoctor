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
