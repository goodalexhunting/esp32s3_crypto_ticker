#include "ota_mgr.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Update.h>
#include <WiFi.h>
#include <esp_ota_ops.h>
#include <esp_task_wdt.h>
#include <mbedtls/sha256.h>

#include "app_config.h"

namespace cryptoapp {

namespace {

// Manifest JSON fields
constexpr char MANIFEST_VERSION[]      = "version";
constexpr char MANIFEST_FIRMWARE_URL[] = "firmware_url";
constexpr char MANIFEST_SHA256[]       = "sha256";
constexpr int  HTTP_CONNECT_TIMEOUT_MS = 5000;
constexpr int  HTTP_READ_TIMEOUT_MS    = 8000;

// Compare two semantic version strings "major.minor.patch".
// Returns:
//   > 0 if a > b
//   0 if a == b
//   < 0 if a < b
int compareVersions(const char* a, const char* b) {
    int ma = atoi(a);
    int mb = atoi(b);
    if (ma != mb) return ma - mb;

    // Skip to minor
    const char* pa = strchr(a, '.');
    const char* pb = strchr(b, '.');
    if (!pa || !pb) return 0;
    pa++;
    pb++;

    int va = atoi(pa);
    int vb = atoi(pb);
    if (va != vb) return va - vb;

    // Skip to patch
    pa = strchr(pa, '.');
    pb = strchr(pb, '.');
    if (!pa || !pb) return 0;
    pa++;
    pb++;

    return atoi(pa) - atoi(pb);
}

}  // namespace

OtaManager::CheckResult OtaManager::checkForUpdate(const String& manifestUrl) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[OTA] Not connected to WiFi");
        return CheckResult::ERROR;
    }

    HTTPClient http;
    http.begin(manifestUrl);
    // Bound the request so a slow or unreachable update server can never
    // block the main loop.
    http.setConnectTimeout(HTTP_CONNECT_TIMEOUT_MS);
    http.setTimeout(HTTP_READ_TIMEOUT_MS);
    // GitHub release assets respond with a 302 redirect to the asset CDN.
    // HTTPClient does not follow redirects by default (Arduino core 3.x),
    // so redirects must be enabled explicitly for the manifest check.
    http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
    http.addHeader("User-Agent", "ESP32");
    int httpCode = http.GET();

    if (httpCode != HTTP_CODE_OK) {
        Serial.printf("[OTA] Manifest fetch failed: HTTP %d\n", httpCode);
        http.end();
        return CheckResult::ERROR;
    }

    String payload = http.getString();
    http.end();

    JsonDocument         doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (error) {
        Serial.printf("[OTA] Manifest JSON parse failed: %s\n", error.c_str());
        return CheckResult::ERROR;
    }

    const char* remoteVersion = doc[MANIFEST_VERSION] | "";
    if (strlen(remoteVersion) == 0) {
        Serial.println("[OTA] Manifest missing version field");
        return CheckResult::ERROR;
    }

    Serial.printf("[OTA] Installed: %s, Remote: %s\n", FW_VERSION, remoteVersion);

    if (compareVersions(FW_VERSION, remoteVersion) >= 0) {
        Serial.println("[OTA] Firmware is up to date");
        return CheckResult::UP_TO_DATE;
    }

    // Store the firmware URL and SHA-256 for the caller to use with
    // performUpdate().
    _firmwareUrl = doc[MANIFEST_FIRMWARE_URL] | "";
    _sha256      = doc[MANIFEST_SHA256] | "";
    if (_firmwareUrl.length() == 0 || _sha256.length() == 0) {
        Serial.println("[OTA] Manifest missing firmware_url or sha256");
        return CheckResult::ERROR;
    }

    Serial.println("[OTA] Update available");
    return CheckResult::UPDATE_AVAILABLE;
}

bool OtaManager::performUpdate(const String& firmwareUrl, const String& sha256) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[OTA] Not connected to WiFi");
        return false;
    }

    Serial.printf("[OTA] Starting update from %s\n", firmwareUrl.c_str());

    // Set up the Update library to write to the OTA flash partition.
    if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
        Serial.printf("[OTA] Update.begin failed: %s\n", Update.errorString());
        return false;
    }

    Update.onProgress([](uint8_t progress, uint8_t total) {
        Serial.printf("[OTA] Flash progress: %u/%u\n", progress, total);
    });

    // Download, verify (SHA-256), and flash in a single streaming pass.
    mbedtls_sha256_context shaCtx;
    mbedtls_sha256_init(&shaCtx);
    mbedtls_sha256_starts(&shaCtx, 0);  // 0 = SHA-256 (not 224)

    HTTPClient http;
    http.begin(firmwareUrl);
    http.setConnectTimeout(HTTP_CONNECT_TIMEOUT_MS);
    http.setTimeout(HTTP_READ_TIMEOUT_MS);
    // The firmware binary is served behind the same 302 redirect from
    // the GitHub release asset CDN; follow it so the download succeeds.
    http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
    http.addHeader("User-Agent", "ESP32");
    int httpCode = http.GET();

    if (httpCode != HTTP_CODE_OK) {
        Serial.printf("[OTA] Download failed: HTTP %d\n", httpCode);
        http.end();
        mbedtls_sha256_free(&shaCtx);
        Update.end();
        return false;
    }

    WiFiClient* stream = http.getStreamPtr();
    uint8_t     buffer[512];
    size_t      totalRead = 0;
    bool        ok        = true;

    while (http.connected()) {
        // Wait for data to be available, with a timeout.
        if (!stream->available()) {
            // Yield to the WiFi stack and retry.
            delay(1);
            continue;
        }

        size_t bytesRead = stream->read(buffer, sizeof(buffer));
        if (bytesRead > 0) {
            mbedtls_sha256_update(&shaCtx, buffer, bytesRead);
            if (Update.write(buffer, bytesRead) != bytesRead) {
                Serial.printf("[OTA] Write failed: %s\n", Update.errorString());
                ok = false;
                break;
            }
            totalRead += bytesRead;
        }
    }

    http.end();

    // Finalize the hash.
    uint8_t computedHash[32];
    mbedtls_sha256_finish(&shaCtx, computedHash);
    mbedtls_sha256_free(&shaCtx);

    // Convert computed hash to lowercase hex string for comparison.
    char computedHex[65];
    for (int i = 0; i < 32; i++) {
        snprintf(computedHex + i * 2, 3, "%02x", computedHash[i]);
    }
    computedHex[64] = '\0';

    Serial.printf("[OTA] Downloaded %u bytes\n", (unsigned)totalRead);
    Serial.printf("[OTA] Computed SHA-256: %s\n", computedHex);
    Serial.printf("[OTA] Expected SHA-256: %s\n", sha256.c_str());

    if (strcmp(computedHex, sha256.c_str()) != 0) {
        Serial.println("[OTA] SHA-256 mismatch - aborting update");
        Update.end();
        return false;
    }

    if (!ok) {
        Update.end();
        return false;
    }

    if (!Update.end()) {
        Serial.printf("[OTA] Update.end failed: %s\n", Update.errorString());
        return false;
    }

    Serial.println("[OTA] Update installed successfully - rebooting");
    Serial.flush();
    ESP.restart();
    return true;
}

void OtaManager::selfTestVerification() {
    // Cancel the pending bootloader rollback: the app running from the
    // newly updated partition has booted far enough to reach this point,
    // so the updated partition is now the known-good one.
    esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
    if (err == ESP_OK) {
        Serial.println("[OTA] Self-test passed - OTA boot confirmed");
    } else {
        // ESP_ERR_OTA_ROLLBACK_INVALID_STATE means the app is running from a
        // factory partition, where rollback tracking does not apply.
        Serial.printf("[OTA] mark_app_valid result: %s\n", esp_err_to_name(err));
    }

    // Disarm the boot-time task watchdog that was armed in setup() so a
    // hung boot forces a reboot back to the previously working partition.
    esp_err_t wdtErr = esp_task_wdt_delete(nullptr);
    if (wdtErr != ESP_OK && wdtErr != ESP_ERR_NOT_FOUND) {
        Serial.printf("[OTA] Watchdog disarm warning: %s\n", esp_err_to_name(wdtErr));
    } else {
        Serial.println("[OTA] Boot watchdog disarmed");
    }
    esp_task_wdt_deinit();
}

}  // namespace cryptoapp
