#pragma once

#include <Arduino.h>

#include "app_config.h"

namespace cryptoapp {

/**
 * Secure OTA firmware update manager.
 *
 * Checks a remote version manifest, compares versions, downloads the
 * firmware via HTTPS, verifies its SHA-256 checksum, and installs it
 * using the ESP32 OTA partition mechanism.
 *
 * A failed OTA leaves the existing firmware operational.
 */
class OtaManager {
   public:
    enum class CheckResult {
        UP_TO_DATE,
        UPDATE_AVAILABLE,
        ERROR,
    };

    OtaManager() = default;

    /**
     * Check the update manifest at manifestUrl.
     * Returns UP_TO_DATE if no update is needed, UPDATE_AVAILABLE if a
     * newer version exists, or ERROR if the check failed.
     * On UPDATE_AVAILABLE, the firmware URL and SHA-256 from the
     * manifest are stored and retrievable via firmwareUrl()/sha256().
     */
    CheckResult checkForUpdate(const String& manifestUrl);

    /**
     * Download and install the firmware at firmwareUrl, verifying the
     * SHA-256 checksum before flashing.
     * Returns true on success (the device should reboot immediately).
     */
    bool performUpdate(const String& firmwareUrl, const String& sha256);

    /**
     * Confirm that the currently booting firmware started successfully.
     *
     * Must be called shortly after boot (within the boot self-test window,
     * see BOOT_SELF_TEST_TIMEOUT_MS) whenever the device runs from an
     * OTA-updated partition. This cancels the pending bootloader rollback
     * (esp_ota_mark_app_valid_cancel_rollback) and disarms the boot-time
     * task watchdog, so the updated partition stays active.
     *
     * If this function is never reached before the watchdog fires, the
     * chip reboots and the bootloader falls back to the previously working
     * partition — this is what prevents bricking after a failed update.
     */
    void selfTestVerification();

    /** Firmware URL from the most recent successful manifest check. */
    const String& firmwareUrl() const {
        return _firmwareUrl;
    }

    /** SHA-256 from the most recent successful manifest check. */
    const String& sha256() const {
        return _sha256;
    }

    /** Current installed firmware version. */
    static const char* installedVersion() {
        return FW_VERSION;
    }

   private:
    String _firmwareUrl;
    String _sha256;
};

}  // namespace cryptoapp