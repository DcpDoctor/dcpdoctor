cmake_minimum_required(VERSION 3.25)
project(dcpdoctor
    VERSION 0.1.0
    DESCRIPTION "DCP validator and verifier"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Dependencies
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBXML2 REQUIRED IMPORTED_TARGET libxml-2.0)
pkg_check_modules(OPENSSL REQUIRED IMPORTED_TARGET openssl)
pkg_check_modules(XERCES REQUIRED IMPORTED_TARGET xerces-c)
pkg_check_modules(SQLITE3 REQUIRED IMPORTED_TARGET sqlite3)

# CLI11 (header-only)
add_subdirectory(extern/CLI11 EXCLUDE_FROM_ALL)

# spdlog
add_subdirectory(extern/spdlog EXCLUDE_FROM_ALL)

# asdcplib
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
add_subdirectory(extern/asdcplib EXCLUDE_FROM_ALL)

# Core library
add_library(dcpdoctor_core STATIC
    src/dcp.cpp
    src/assetmap.cpp
    src/pkl.cpp
    src/cpl.cpp
    src/hash.cpp
    src/mxf.cpp
    src/j2k.cpp
    src/bitrate.cpp
    src/audio.cpp
    src/subtitle.cpp
    src/isdcf.cpp
    src/validators.cpp
    src/compliance.cpp
    src/schema_validator.cpp
    src/signature.cpp
    src/report.cpp
    src/note.cpp
    src/timeline.cpp
    src/advanced.cpp
    src/server.cpp
    src/kdm.cpp
    src/diff.cpp
    src/cache.cpp
    src/theater.cpp
    src/fixes.cpp
    src/mxf_advanced.cpp
    src/kdm_advanced.cpp
    src/hfr_stereo.cpp
    src/imf.cpp
    src/quality.cpp
    src/photon.cpp
    src/premium.cpp
    src/studio.cpp)

target_include_directories(dcpdoctor_core PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/extern/asdcplib/src)
target_link_libraries(dcpdoctor_core PUBLIC
    PkgConfig::LIBXML2
    PkgConfig::OPENSSL
    PkgConfig::XERCES
    PkgConfig::SQLITE3
    spdlog::spdlog
    libasdcp)

# CLI tool
add_executable(dcpdoctor src/main.cpp)
target_link_libraries(dcpdoctor PRIVATE dcpdoctor_core CLI11::CLI11)

# Tests
option(BUILD_TESTING "Build tests" ON)
if(BUILD_TESTING)
    enable_testing()
    add_executable(dcpdoctor_test tests/test_main.cpp)
    target_link_libraries(dcpdoctor_test PRIVATE dcpdoctor_core)
    add_test(NAME dcpdoctor_test COMMAND dcpdoctor_test)
endif()
