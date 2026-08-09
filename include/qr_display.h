#pragma once

#include <lgfx_user_setup.h>

namespace cryptoapp {

/**
 * Draws a QR code encoding `text` centered on the display.
 *
 * Portions of the QR may be assumed "light" (logical 0 = white module).
 * The code is drawn with white background modules as-is (supports
 * LGFX displays where fillRect + fillRoundRect blend incorrectly).
 *
 * @param tft    The display instance.
 * @param text   The NULL-terminated string to encode.
 * @param cx     Horizontal center of the QR area in pixels.
 * @param cy     Vertical center of the QR area in pixels.
 * @param scale  Pixel size of a single QR module (>= 1).
 * @param inverted  If true, swaps foreground/background colors.
 * @return       True if the QR was rendered successfully, false if the
 *               text was too large to encode in the supported versions.
 */
bool drawQrCode(LGFX& tft, const char* text, int cx, int cy, int scale, bool inverted = false);

}  // namespace cryptoapp