#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

#include <cstddef>

namespace cryptoapp {

/**
 * Compare two semantic version strings "major.minor.patch".
 * Returns:
 *   > 0 if a > b
 *   0 if a == b
 *   < 0 if a < b
 *
 * Missing trailing segments are treated as 0 (e.g. "1.2" == "1.2.0").
 * Non-numeric segments are parsed by atoi-like behaviour (0) and never
 * crash the comparison.
 */
int compareVersions(const char* a, const char* b);

/**
 * Convert a 32-byte SHA-256 digest to a lowercase hex string.
 * hex must be at least 65 bytes; hex[64] is set to '\0'.
 */
void sha256ToHex(const unsigned char hash[32], char hex[65]);

/**
 * Fields extracted from an OTA version manifest.
 */
struct OtaManifest {
    String version;      // Remote firmware version, e.g. "1.2.3"
    String firmwareUrl;  // Direct URL to the firmware binary
    String sha256;       // Expected SHA-256 of the firmware binary
};

/**
 * Extract just the version field from a parsed OTA manifest. Returns an
 * empty string if the field is missing or empty. Used to compare versions
 * before requiring the download fields.
 */
const char* otaManifestVersion(const JsonDocument& doc);

/**
 * Extract the version, firmware URL and SHA-256 from a parsed OTA manifest
 * JSON document. Returns false (and leaves out unchanged) if any required
 * field is missing or empty.
 */
bool parseOtaManifest(const JsonDocument& doc, OtaManifest& out);

}  // namespace cryptoapp
