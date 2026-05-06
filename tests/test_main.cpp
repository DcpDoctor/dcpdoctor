#include "dcpdoctor/dcpdoctor.h"
#include <cassert>
#include <filesystem>
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

int main() {
    std::cout << tests_passed << "/" << tests_run << " tests passed\n";
    return (tests_passed == tests_run) ? 0 : 1;
}
