#include "ota_mgr.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
// Arduino core 2.x wraps the IDF cert bundle as arduino_esp_crt_bundle_attach
// (WiFiClientSecure/src/esp_crt_bundle.h). This is the same trust store the
// rest of the app uses for HTTPS, so TLS behaviour is unchanged.
#include <esp_crt_bundle.h>
#include <esp_http_client.h>
#include <esp_https_ota.h>
#include <esp_ota_ops.h>
#include <esp_task_wdt.h>
#include <mbedtls/sha256.h>

#include "app_config.h"
#include "ota_utils.h"

namespace cryptoapp {

namespace {

constexpr int HTTP_CONNECT_TIMEOUT_MS = 5000;
constexpr int HTTP_READ_TIMEOUT_MS    = 8000;

// State carried through the HTTPS OTA HTTP client events. The event
// handler hashes each received firmware chunk while esp_https_ota_perform()
// streams it to the OTA partition, so the SHA-256 from the manifest is
// verified in a single download pass.
struct OtaDownloadState {
    mbedtls_sha256_context shaCtx;
    size_t                 bytesReceived;
};

// HTTP client event handler: feeds every received payload chunk into the
// running SHA-256 context while esp_https_ota streams the image to flash.
esp_err_t otaHttpEventHandler(esp_http_client_event_t* evt) {
    OtaDownloadState* state = static_cast<OtaDownloadState*>(evt->user_data);
    if (state == nullptr) {
        return ESP_OK;
    }

    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            if (evt->data_len > 0) {
                mbedtls_sha256_update(
                    &state->shaCtx, static_cast<const unsigned char*>(evt->data), evt->data_len);
                state->bytesReceived += evt->data_len;
                Serial.printf("[OTA] Downloaded %u bytes\n", (unsigned)state->bytesReceived);
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            if (state->bytesReceived > 0) {
                Serial.printf("[OTA] HTTP download finished (%u bytes)\n",
                              (unsigned)state->bytesReceived);
            }
            break;
        default:
            break;
    }
    return ESP_OK;
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

    const char* remoteVersion = otaManifestVersion(doc);
    if (strlen(remoteVersion) == 0) {
        Serial.println("[OTA] Manifest missing version field");
        return CheckResult::ERROR;
    }

    Serial.printf("[OTA] Installed: %s, Remote: %s\n", FW_VERSION, remoteVersion);

    if (compareVersions(FW_VERSION, remoteVersion) >= 0) {
        Serial.println("[OTA] Firmware is up to date");
        return CheckResult::UP_TO_DATE;
    }

    OtaManifest manifest;
    if (!parseOtaManifest(doc, manifest)) {
        Serial.println("[OTA] Manifest missing firmware_url or sha256");
        return CheckResult::ERROR;
    }

    // Store the firmware URL and SHA-256 for the caller to use with
    // performUpdate().
    _firmwareUrl = manifest.firmwareUrl;
    _sha256      = manifest.sha256;

    Serial.println("[OTA] Update available");
    return CheckResult::UPDATE_AVAILABLE;
}

bool OtaManager::performUpdate(const String& firmwareUrl, const String& sha256) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[OTA] Not connected to WiFi");
        return false;
    }

    Serial.printf("[OTA] Starting update from %s\n", firmwareUrl.c_str());

    // Download, verify (SHA-256), and flash in a single streaming pass
    // using the ESP-IDF HTTPS OTA API. The event handler continuously
    // hashes the received stream while esp_https_ota_perform() writes it
    // to the next OTA partition. Aborting on any failure leaves the
    // previously working partition untouched.
    OtaDownloadState state = {};
    mbedtls_sha256_init(&state.shaCtx);
    mbedtls_sha256_starts(&state.shaCtx, 0);  // 0 = SHA-256 (not 224)

    esp_http_client_config_t httpConfig = {};
    httpConfig.url                      = firmwareUrl.c_str();
    httpConfig.event_handler            = otaHttpEventHandler;
    httpConfig.user_data                = &state;
    // Bound every network read so a slow or dead connection can never
    // block the main loop indefinitely.
    httpConfig.timeout_ms = HTTP_READ_TIMEOUT_MS;
    httpConfig.user_agent = "ESP32";
    // Verify the server certificate against the system cert bundle —
    // the same trust store the rest of the app uses for HTTPS.
    httpConfig.crt_bundle_attach = arduino_esp_crt_bundle_attach;
    // GitHub release assets reply with a 302 redirect to the asset CDN.
    // disable_auto_redirect defaults to false, so esp_http_client follows
    // the redirect automatically (no manual follow needed).
    httpConfig.disable_auto_redirect = false;

    esp_https_ota_config_t otaConfig = {};
    otaConfig.http_config            = &httpConfig;

    esp_https_ota_handle_t otaHandle = nullptr;
    esp_err_t              err       = esp_https_ota_begin(&otaConfig, &otaHandle);
    if (err != ESP_OK) {
        Serial.printf("[OTA] esp_https_ota_begin failed: %s\n", esp_err_to_name(err));
        mbedtls_sha256_free(&state.shaCtx);
        return false;
    }

    // Log the remote image's embedded version for diagnostics.
    esp_app_desc_t appDesc = {};
    if (esp_https_ota_get_img_desc(otaHandle, &appDesc) == ESP_OK && appDesc.version[0] != '\0') {
        Serial.printf("[OTA] Remote image version: %s\n", appDesc.version);
    }

    // Stream the firmware: esp_https_ota_perform() must be called in a
    // loop until it stops returning ESP_ERR_HTTPS_OTA_IN_PROGRESS.
    while (true) {
        err = esp_https_ota_perform(otaHandle);
        if (err != ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
            break;
        }
    }

    // Finalize the hash now so every failure path below has already
    // released the mbedtls context.
    uint8_t computedHash[32];
    mbedtls_sha256_finish(&state.shaCtx, computedHash);
    mbedtls_sha256_free(&state.shaCtx);

    if (err != ESP_OK) {
        Serial.printf("[OTA] esp_https_ota_perform failed: %s\n", esp_err_to_name(err));
        esp_https_ota_abort(otaHandle);
        return false;
    }

    if (!esp_https_ota_is_complete_data_received(otaHandle)) {
        Serial.println("[OTA] Incomplete image received - aborting update");
        esp_https_ota_abort(otaHandle);
        return false;
    }

    Serial.printf("[OTA] Downloaded %u bytes\n", (unsigned)state.bytesReceived);

    char computedHex[65];
    sha256ToHex(computedHash, computedHex);
    Serial.printf("[OTA] Computed SHA-256: %s\n", computedHex);
    Serial.printf("[OTA] Expected SHA-256: %s\n", sha256.c_str());

    if (strcmp(computedHex, sha256.c_str()) != 0) {
        Serial.println("[OTA] SHA-256 mismatch - aborting update");
        esp_https_ota_abort(otaHandle);
        return false;
    }

    err = esp_https_ota_finish(otaHandle);
    if (err != ESP_OK) {
        Serial.printf("[OTA] esp_https_ota_finish failed: %s\n", esp_err_to_name(err));
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
