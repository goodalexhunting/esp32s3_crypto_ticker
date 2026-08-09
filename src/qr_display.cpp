#include <Arduino.h>
#include <lgfx_user_setup.h>
#include <qr_display.h>
#include <qrcode.h>
#include <string.h>

namespace cryptoapp {

namespace {
constexpr int kMinVersion = 1;
constexpr int kMaxVersion = 10;
}  // namespace

bool drawQrCode(LGFX& tft, const char* text, int cx, int cy, int scale, bool inverted) {
    if (text == nullptr || scale < 1) {
        return false;
    }

    // Find the smallest QR version that can hold the text.
    //
    // NOTE: the QRCode library never reports an "input too large" error (it
    // always returns 0 on success, -1 on failure), so we must pre-check the
    // byte-mode capacity ourselves. Otherwise an undersized version would
    // silently produce a corrupt, un-scannable QR.
    static constexpr uint16_t kEccLowByteCapacity[10] = {
        19, 34, 55, 80, 108, 136, 156, 194, 232, 274};

    const size_t textLen = strlen(text);

    QRCode   qr;
    uint8_t* buffer = nullptr;
    for (int v = kMinVersion; v <= kMaxVersion; ++v) {
        // Byte mode overhead: 4-bit mode indicator + 8-bit char count header.
        if ((textLen * 8 + 12) > (kEccLowByteCapacity[v - 1] * 8)) {
            continue;
        }

        uint8_t* candidate = static_cast<uint8_t*>(malloc(qrcode_getBufferSize(v)));
        if (candidate == nullptr) {
            continue;
        }
        if (qrcode_initText(&qr, candidate, v, ECC_LOW, text) == 0) {
            buffer = candidate;
            break;
        }
        free(candidate);
    }

    if (buffer == nullptr) {
        Serial.println("[QR] Failed to allocate/encode QR buffer");
        return false;
    }

    Serial.printf("[QR] Encoded version %d, size %d, scale %d\n", qr.version, qr.size, scale);

    const int size = qr.size;
    const int dim  = size * scale;
    const int x0   = cx - dim / 2;
    const int y0   = cy - dim / 2;

    const uint16_t fg = inverted ? TFT_WHITE : TFT_BLACK;
    const uint16_t bg = inverted ? TFT_BLACK : TFT_WHITE;

    // Background rectangle.
    tft.fillRect(x0, y0, dim, dim, bg);

    // Draw dark modules as filled squares; paint runs of light modules as a
    // single rect to keep the number of draw calls low.
    for (int y = 0; y < size; ++y) {
        int x = 0;
        while (x < size) {
            if (qrcode_getModule(&qr, x, y)) {
                tft.fillRect(x0 + x * scale, y0 + y * scale, scale, scale, fg);
                ++x;
            } else {
                int run = 0;
                while (x + run < size && !qrcode_getModule(&qr, x + run, y)) {
                    ++run;
                }
                tft.fillRect(x0 + x * scale, y0 + y * scale, run * scale, scale, bg);
                x += run;
            }
        }
    }

    free(buffer);
    return true;
}

}  // namespace cryptoapp