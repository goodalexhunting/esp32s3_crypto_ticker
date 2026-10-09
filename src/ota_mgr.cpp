#include "ota_mgr.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_http_client.h>
#include <esp_ota_ops.h>
#include <esp_task_wdt.h>
#include <mbedtls/sha256.h>

#include <strings.h>

#include "app_config.h"
#include "ota_certs.h"
#include "ota_utils.h"

namespace cryptoapp {

namespace {

constexpr int HTTP_CONNECT_TIMEOUT_MS = 5000;
constexpr int HTTP_READ_TIMEOUT_MS    = 8000;

// State carried through the HTTPS OTA HTTP client events. The event
// handler logs the request/response dialogue (including redirect hops)
// and captures the Location header so performUpdate() can follow
// redirects manually with full visibility into every hop.
struct OtaDownloadState {
    String location;      // Location header from the latest response
    size_t bytesReceived;
};

// HTTP client event handler: logs the request/response dialogue so a
// failing update (e.g. a CDN block page) can be diagnosed from the
// serial log alone.
esp_err_t otaHttpEventHandler(esp_http_client_event_t* evt) {
    OtaDownloadState* state = static_cast<OtaDownloadState*>(evt->user_data);

    switch (evt->event_id) {
        case HTTP_EVENT_ERROR:
            Serial.println("[OTA] HTTP client error event");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            Serial.println("[OTA] HTTP connected");
            break;
        case HTTP_EVENT_HEADERS_SENT:
            Serial.println("[OTA] HTTP request headers sent");
            break;
        case HTTP_EVENT_ON_HEADER:
            if (state != nullptr && evt->header_key != nullptr &&
                strcasecmp(evt->header_key, "Location") == 0 && evt->header_value != nullptr) {
                state->location = evt->header_value;
            }
            Serial.printf("[OTA] HDR: %s: %s\n", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_DISCONNECTED:
            Serial.println("[OTA] HTTP disconnected");
            break;
        case HTTP_EVENT_ON_DATA:
        case HTTP_EVENT_ON_FINISH:
            break;  // download progress is logged by the read loop
        default:
            Serial.printf("[OTA] HTTP event %d\n", (int)evt->event_id);
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

    // The manifest carries the firmware URL and its SHA-256, so it is the
    // trust anchor for the whole update: verify it against the pinned CA
    // roots in ota_certs.h. (Plain HTTPClient::begin(url) would skip TLS
    // verification entirely in this Arduino core, so an explicit secure
    // client is required.)
    WiFiClientSecure secureClient;
    secureClient.setCACert(OTA_CA_CERT_PEM);

    HTTPClient http;
    http.begin(secureClient, manifestUrl);
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
    // using esp_http_client + the esp_ota write API directly. This gives
    // full visibility into every redirect hop (GitHub release assets
    // redirect to a signed CDN URL) and lets a blocked download be
    // diagnosed from the logged response body. Aborting on any failure
    // leaves the previously working partition untouched.
    const esp_partition_t* updatePartition = esp_ota_get_next_update_partition(nullptr);
    if (updatePartition == nullptr) {
        Serial.println("[OTA] No inactive OTA partition available");
        return false;
    }
    Serial.printf("[OTA] Target partition: %s\n", updatePartition->label);

    esp_ota_handle_t otaHandle = 0;
    esp_err_t       err       = esp_ota_begin(updatePartition, OTA_WITH_SEQUENTIAL_WRITES, &otaHandle);
    if (err != ESP_OK) {
        Serial.printf("[OTA] esp_ota_begin failed: %s\n", esp_err_to_name(err));
        return false;
    }

    OtaDownloadState        state = {};
    mbedtls_sha256_context  shaCtx;
    mbedtls_sha256_init(&shaCtx);
    mbedtls_sha256_starts(&shaCtx, 0);  // 0 = SHA-256 (not 224)

    esp_http_client_config_t httpConfig = {};
    httpConfig.url                      = firmwareUrl.c_str();
    httpConfig.event_handler            = otaHttpEventHandler;
    httpConfig.user_data                = &state;
    // Bound every network read so a slow or dead connection can never
    // block the main loop indefinitely.
    httpConfig.timeout_ms = HTTP_READ_TIMEOUT_MS;
    httpConfig.user_agent = "ESP32";
    // Verify the server certificate against the pinned CA roots in
    // ota_certs.h. (Arduino's arduino_esp_crt_bundle_attach is a no-op
    // unless the app pre-populates the bundle with setCACertBundle, so
    // crt_bundle_attach would silently leave TLS unverified.)
    httpConfig.cert_pem = OTA_CA_CERT_PEM;
    // Redirects are followed manually below so every hop's status and
    // headers are visible in the serial log.
    httpConfig.disable_auto_redirect = true;

    esp_http_client_handle_t client = esp_http_client_init(&httpConfig);
    if (client == nullptr) {
        Serial.println("[OTA] esp_http_client_init failed");
        mbedtls_sha256_free(&shaCtx);
        esp_ota_abort(otaHandle);
        return false;
    }
    esp_http_client_set_header(client, "Accept", "application/octet-stream");
    esp_http_client_set_header(client, "Accept-Encoding", "identity");

    bool success = false;
    int  hops    = 0;
    while (hops <= 5) {
        state.location = "";
        err = esp_http_client_open(client, 0);
        if (err != ESP_OK) {
            Serial.printf("[OTA] esp_http_client_open failed: %s\n", esp_err_to_name(err));
            break;
        }
        // open() only sends the request; the response status line and
        // headers arrive with fetch_headers().
        int headerRet = esp_http_client_fetch_headers(client);
        if (headerRet < 0) {
            Serial.printf("[OTA] esp_http_client_fetch_headers failed: %d\n", headerRet);
            break;
        }

        int status = esp_http_client_get_status_code(client);
        Serial.printf("[OTA] HTTP status: %d\n", status);

        if (status == 301 || status == 302 || status == 303 || status == 307 || status == 308) {
            if (state.location.length() == 0) {
                Serial.println("[OTA] Redirect without Location header - aborting");
                break;
            }
            Serial.printf("[OTA] Following redirect to %s\n", state.location.c_str());
            esp_http_client_set_url(client, state.location.c_str());
            hops++;
            continue;
        }

        if (status != 200) {
            // Log the server's error page so a blocked update is
            // diagnosable from the serial log alone.
            char body[512];
            int  total = 0;
            int  n;
            while (total < (int)sizeof(body) - 1 &&
                   (n = esp_http_client_read(client, body + total, sizeof(body) - 1 - total)) > 0) {
                total += n;
            }
            body[total] = '\0';
            Serial.printf("[OTA] Error body: %s\n", total > 0 ? body : "(empty)");
            break;
        }

        // Stream the image: hash every chunk while writing it to the OTA
        // partition, so the SHA-256 from the manifest is verified in a
        // single download pass.
        char buf[1024];
        int  n;
        while ((n = esp_http_client_read(client, buf, sizeof(buf))) > 0) {
            mbedtls_sha256_update(&shaCtx, reinterpret_cast<const unsigned char*>(buf), n);
            state.bytesReceived += n;
            err = esp_ota_write(otaHandle, buf, n);
            if (err != ESP_OK) {
                Serial.printf("[OTA] esp_ota_write failed: %s\n", esp_err_to_name(err));
                break;
            }
            if (state.bytesReceived % (64U * 1024U) == 0) {
                Serial.printf("[OTA] Downloaded %u bytes\n", (unsigned)state.bytesReceived);
            }
        }
        if (err != ESP_OK) {
            break;
        }
        Serial.printf("[OTA] Downloaded %u bytes\n", (unsigned)state.bytesReceived);
        success = true;
        break;
    }

    esp_http_client_cleanup(client);

    // Finalize the hash now so every failure path below has already
    // released the mbedtls context.
    uint8_t computedHash[32];
    mbedtls_sha256_finish(&shaCtx, computedHash);
    mbedtls_sha256_free(&shaCtx);

    if (!success) {
        esp_ota_abort(otaHandle);
        return false;
    }

    char computedHex[65];
    sha256ToHex(computedHash, computedHex);
    Serial.printf("[OTA] Computed SHA-256: %s\n", computedHex);
    Serial.printf("[OTA] Expected SHA-256: %s\n", sha256.c_str());

    if (strcmp(computedHex, sha256.c_str()) != 0) {
        Serial.println("[OTA] SHA-256 mismatch - aborting update");
        esp_ota_abort(otaHandle);
        return false;
    }

    err = esp_ota_end(otaHandle);
    if (err != ESP_OK) {
        Serial.printf("[OTA] esp_ota_end failed: %s\n", esp_err_to_name(err));
        return false;
    }

    err = esp_ota_set_boot_partition(updatePartition);
    if (err != ESP_OK) {
        Serial.printf("[OTA] esp_ota_set_boot_partition failed: %s\n", esp_err_to_name(err));
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
