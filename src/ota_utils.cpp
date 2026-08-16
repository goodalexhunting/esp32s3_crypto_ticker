#include "ota_utils.h"

#include <ArduinoJson.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace cryptoapp {

namespace {
constexpr char MANIFEST_VERSION[]      = "version";
constexpr char MANIFEST_FIRMWARE_URL[] = "firmware_url";
constexpr char MANIFEST_SHA256[]       = "sha256";
}  // namespace

int compareVersions(const char* a, const char* b) {
    int ma = atoi(a);
    int mb = atoi(b);
    if (ma != mb) return ma - mb;

    // Minor segment. A missing segment is treated as 0.
    const char* pa = strchr(a, '.');
    const char* pb = strchr(b, '.');
    int         va = pa != nullptr ? atoi(pa + 1) : 0;
    int         vb = pb != nullptr ? atoi(pb + 1) : 0;
    if (va != vb) return va - vb;

    // Patch segment. A missing segment is treated as 0.
    pa     = pa != nullptr ? strchr(pa + 1, '.') : nullptr;
    pb     = pb != nullptr ? strchr(pb + 1, '.') : nullptr;
    int xa = pa != nullptr ? atoi(pa + 1) : 0;
    int xb = pb != nullptr ? atoi(pb + 1) : 0;
    return xa - xb;
}

void sha256ToHex(const unsigned char hash[32], char hex[65]) {
    for (int i = 0; i < 32; i++) {
        snprintf(hex + i * 2, 3, "%02x", hash[i]);
    }
    hex[64] = '\0';
}

const char* otaManifestVersion(const JsonDocument& doc) {
    return doc[MANIFEST_VERSION] | "";
}

bool parseOtaManifest(const JsonDocument& doc, OtaManifest& out) {
    const char* version = doc[MANIFEST_VERSION] | "";
    const char* url     = doc[MANIFEST_FIRMWARE_URL] | "";
    const char* sha     = doc[MANIFEST_SHA256] | "";

    if (strlen(version) == 0 || strlen(url) == 0 || strlen(sha) == 0) {
        return false;
    }

    out.version     = version;
    out.firmwareUrl = url;
    out.sha256      = sha;
    return true;
}

}  // namespace cryptoapp
