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

# Core library
add_library(dcpdoctor_core STATIC
    src/dcp.cpp
    src/assetmap.cpp
    src/pkl.cpp
    src/cpl.cpp
    src/hash.cpp
    src/mxf.cpp
    src/j2k.cpp
    src/subtitle.cpp
    src/isdcf.cpp
    src/validators.cpp
    src/compliance.cpp
    src/schema_validator.cpp
    src/signature.cpp
    src/report.cpp
    src/note.cpp)

target_include_directories(dcpdoctor_core PUBLIC
    ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(dcpdoctor_core PUBLIC
    PkgConfig::LIBXML2
    PkgConfig::OPENSSL
    PkgConfig::XERCES)

# CLI tool
add_executable(dcpdoctor src/main.cpp)
target_link_libraries(dcpdoctor PRIVATE dcpdoctor_core)

# Tests
option(BUILD_TESTING "Build tests" ON)
if(BUILD_TESTING)
    enable_testing()
    add_executable(dcpdoctor_test tests/test_main.cpp)
    target_link_libraries(dcpdoctor_test PRIVATE dcpdoctor_core)
    add_test(NAME dcpdoctor_test COMMAND dcpdoctor_test)
endif()
