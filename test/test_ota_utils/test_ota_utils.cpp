#include <unity.h>

#include <cstring>
#include <string>

#include "../../src/ota_utils.cpp"
#include "Arduino.h"
#include "ota_utils.h"

void setUp() {}

void tearDown() {}

// ---------------------------------------------------------------------------
// compareVersions
// ---------------------------------------------------------------------------

void test_compare_equal_versions() {
    TEST_ASSERT_EQUAL(0, cryptoapp::compareVersions("1.2.3", "1.2.3"));
    TEST_ASSERT_EQUAL(0, cryptoapp::compareVersions("0.0.0", "0.0.0"));
    TEST_ASSERT_EQUAL(0, cryptoapp::compareVersions("1.0.0", "1.0.0"));
}

void test_compare_major_dominates() {
    TEST_ASSERT(cryptoapp::compareVersions("2.0.0", "1.99.99") > 0);
    TEST_ASSERT(cryptoapp::compareVersions("1.99.99", "2.0.0") < 0);
}

void test_compare_minor_dominates_over_patch() {
    TEST_ASSERT(cryptoapp::compareVersions("1.10.0", "1.9.99") > 0);
    TEST_ASSERT(cryptoapp::compareVersions("1.9.99", "1.10.0") < 0);
}

void test_compare_patch() {
    TEST_ASSERT(cryptoapp::compareVersions("1.2.10", "1.2.9") > 0);
    TEST_ASSERT(cryptoapp::compareVersions("1.2.9", "1.2.10") < 0);
    TEST_ASSERT(cryptoapp::compareVersions("1.2.3", "1.2.2") > 0);
}

void test_compare_numeric_not_lexicographic() {
    // Regression: "1.9.9" must sort before "1.10.0" numerically.
    TEST_ASSERT(cryptoapp::compareVersions("1.9.9", "1.10.0") < 0);
    TEST_ASSERT(cryptoapp::compareVersions("1.10.0", "1.9.9") > 0);
    TEST_ASSERT_EQUAL(0, cryptoapp::compareVersions("1.10.9", "1.10.9"));
}

void test_compare_missing_segments_treated_as_zero() {
    TEST_ASSERT_EQUAL(0, cryptoapp::compareVersions("1.2", "1.2.0"));
    TEST_ASSERT_EQUAL(0, cryptoapp::compareVersions("1.2.0", "1.2"));
    TEST_ASSERT(cryptoapp::compareVersions("1.2", "1.2.1") < 0);
    TEST_ASSERT(cryptoapp::compareVersions("1", "1.0.0") == 0);
}

void test_compare_zero_versions() {
    TEST_ASSERT_EQUAL(0, cryptoapp::compareVersions("0.0.0", "0.0.0"));
    TEST_ASSERT(cryptoapp::compareVersions("0.0.1", "0.0.0") > 0);
    TEST_ASSERT(cryptoapp::compareVersions("0.1.0", "0.0.9") > 0);
}

void test_compare_leading_zeros() {
    TEST_ASSERT_EQUAL(0, cryptoapp::compareVersions("01.2.3", "1.2.3"));
    TEST_ASSERT_EQUAL(0, cryptoapp::compareVersions("1.02.3", "1.2.3"));
    TEST_ASSERT_EQUAL(0, cryptoapp::compareVersions("1.2.03", "1.2.3"));
}

void test_compare_large_patch_values() {
    TEST_ASSERT(cryptoapp::compareVersions("1.2.500", "1.2.499") > 0);
    TEST_ASSERT_EQUAL(0, cryptoapp::compareVersions("1.2.999", "1.2.999"));
}

void test_compare_non_numeric_does_not_crash() {
    TEST_ASSERT_EQUAL(0, cryptoapp::compareVersions("a.b.c", "a.b.c"));
    TEST_ASSERT(cryptoapp::compareVersions("", "") == 0);
}

// ---------------------------------------------------------------------------
// sha256ToHex
// ---------------------------------------------------------------------------

void test_sha256_known_vectors() {
    // sha256("") == e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855
    const unsigned char empty[32] = {0xe3, 0xb0, 0xc4, 0x42, 0x98, 0xfc, 0x1c, 0x14,
                                     0x9a, 0xfb, 0xf4, 0xc8, 0x99, 0x6f, 0xb9, 0x24,
                                     0x27, 0xae, 0x41, 0xe4, 0x64, 0x9b, 0x93, 0x4c,
                                     0xa4, 0x95, 0x99, 0x1b, 0x78, 0x52, 0xb8, 0x55};
    char                hex[65];
    cryptoapp::sha256ToHex(empty, hex);
    TEST_ASSERT_EQUAL_STRING("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
                             hex);

    // sha256("abc") == ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad
    const unsigned char abc[32] = {0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea, 0x41, 0x41, 0x40,
                                   0xde, 0x5d, 0xae, 0x22, 0x23, 0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17,
                                   0x7a, 0x9c, 0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad};
    cryptoapp::sha256ToHex(abc, hex);
    TEST_ASSERT_EQUAL_STRING("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
                             hex);
}

void test_sha256_all_zero_and_all_ff() {
    unsigned char zeros[32] = {};
    char          hex[65];
    cryptoapp::sha256ToHex(zeros, hex);
    TEST_ASSERT_EQUAL_STRING("0000000000000000000000000000000000000000000000000000000000000000",
                             hex);

    unsigned char ff[32];
    memset(ff, 0xFF, sizeof(ff));
    cryptoapp::sha256ToHex(ff, hex);
    TEST_ASSERT_EQUAL_STRING("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff",
                             hex);
}

void test_sha256_mixed_bytes_and_null_termination() {
    unsigned char mixed[32] = {0x00, 0x01, 0x0a, 0x0f, 0x10, 0xff, 0xfe, 0x80, 0x7f, 0x00, 0xab,
                               0xcd, 0xef, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0, 0x11,
                               0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb};
    char          hex[65];
    cryptoapp::sha256ToHex(mixed, hex);
    TEST_ASSERT_EQUAL_STRING("00010a0f10fffe807f00abcdef123456789abcdef0112233445566778899aabb",
                             hex);
    TEST_ASSERT_EQUAL(64u, strlen(hex));
    TEST_ASSERT_EQUAL('\0', hex[64]);
}

// ---------------------------------------------------------------------------
// otaManifestVersion / parseOtaManifest
// ---------------------------------------------------------------------------

void test_parse_valid_manifest() {
    JsonDocument doc;
    deserializeJson(doc, R"({"version":"1.2.3","firmware_url":"http://x/f.bin","sha256":"abc"})");
    TEST_ASSERT_EQUAL_STRING("1.2.3", cryptoapp::otaManifestVersion(doc));

    cryptoapp::OtaManifest m;
    TEST_ASSERT_TRUE(cryptoapp::parseOtaManifest(doc, m));
    TEST_ASSERT_EQUAL_STRING("1.2.3", m.version.c_str());
    TEST_ASSERT_EQUAL_STRING("http://x/f.bin", m.firmwareUrl.c_str());
    TEST_ASSERT_EQUAL_STRING("abc", m.sha256.c_str());
}

void test_parse_missing_version() {
    JsonDocument doc;
    deserializeJson(doc, R"({"firmware_url":"http://x/f.bin","sha256":"abc"})");
    TEST_ASSERT_EQUAL_STRING("", cryptoapp::otaManifestVersion(doc));

    cryptoapp::OtaManifest m;
    TEST_ASSERT_FALSE(cryptoapp::parseOtaManifest(doc, m));
}

void test_parse_missing_firmware_url() {
    JsonDocument doc;
    deserializeJson(doc, R"({"version":"1.2.3","sha256":"abc"})");
    cryptoapp::OtaManifest m;
    TEST_ASSERT_FALSE(cryptoapp::parseOtaManifest(doc, m));
}

void test_parse_missing_sha256() {
    JsonDocument doc;
    deserializeJson(doc, R"({"version":"1.2.3","firmware_url":"http://x/f.bin"})");
    cryptoapp::OtaManifest m;
    TEST_ASSERT_FALSE(cryptoapp::parseOtaManifest(doc, m));
}

void test_parse_empty_values() {
    JsonDocument doc;
    deserializeJson(doc, R"({"version":"","firmware_url":"","sha256":""})");
    cryptoapp::OtaManifest m;
    TEST_ASSERT_FALSE(cryptoapp::parseOtaManifest(doc, m));
}

void test_parse_wrong_types() {
    JsonDocument doc;
    deserializeJson(doc, R"({"version":123,"firmware_url":true,"sha256":["x"]})");
    TEST_ASSERT_EQUAL_STRING("", cryptoapp::otaManifestVersion(doc));
    cryptoapp::OtaManifest m;
    TEST_ASSERT_FALSE(cryptoapp::parseOtaManifest(doc, m));
}

void test_parse_malformed_json() {
    JsonDocument         doc;
    DeserializationError err = deserializeJson(doc, "{ not json ]]");
    TEST_ASSERT_TRUE(bool(err));
    cryptoapp::OtaManifest m;
    TEST_ASSERT_FALSE(cryptoapp::parseOtaManifest(doc, m));
}

void test_parse_empty_document() {
    JsonDocument           doc;
    cryptoapp::OtaManifest m;
    TEST_ASSERT_FALSE(cryptoapp::parseOtaManifest(doc, m));
}

void test_parse_extra_fields_ignored() {
    JsonDocument doc;
    deserializeJson(doc, R"({"version":"9.9.9","firmware_url":"u","sha256":"s","note":"hi"})");
    cryptoapp::OtaManifest m;
    TEST_ASSERT_TRUE(cryptoapp::parseOtaManifest(doc, m));
    TEST_ASSERT_EQUAL_STRING("9.9.9", m.version.c_str());
}

int main(int, char**) {
    UNITY_BEGIN();

    RUN_TEST(test_compare_equal_versions);
    RUN_TEST(test_compare_major_dominates);
    RUN_TEST(test_compare_minor_dominates_over_patch);
    RUN_TEST(test_compare_patch);
    RUN_TEST(test_compare_numeric_not_lexicographic);
    RUN_TEST(test_compare_missing_segments_treated_as_zero);
    RUN_TEST(test_compare_zero_versions);
    RUN_TEST(test_compare_leading_zeros);
    RUN_TEST(test_compare_large_patch_values);
    RUN_TEST(test_compare_non_numeric_does_not_crash);

    RUN_TEST(test_sha256_known_vectors);
    RUN_TEST(test_sha256_all_zero_and_all_ff);
    RUN_TEST(test_sha256_mixed_bytes_and_null_termination);

    RUN_TEST(test_parse_valid_manifest);
    RUN_TEST(test_parse_missing_version);
    RUN_TEST(test_parse_missing_firmware_url);
    RUN_TEST(test_parse_missing_sha256);
    RUN_TEST(test_parse_empty_values);
    RUN_TEST(test_parse_wrong_types);
    RUN_TEST(test_parse_malformed_json);
    RUN_TEST(test_parse_empty_document);
    RUN_TEST(test_parse_extra_fields_ignored);

    return UNITY_END();
}