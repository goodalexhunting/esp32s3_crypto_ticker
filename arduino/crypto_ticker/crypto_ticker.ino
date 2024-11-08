/**
 * Crypto Ticker
 *
 *  Created on: 24.05.2015 - https://github.com/osbock/AssetTicker
 *  Updated on: 08.11.2024 - https://github.com/goodalexhunting
 *
 */
#include <WiFi.h>
#include <HTTPClient.h>
#include "config.h"
#include "TFT_eSPI.h"

#define UPDATE_INTERVAL 1000 * 60 * 1

extern void update_crypto(String coins);
unsigned long last_update = 0L;

TFT_eSPI tft = TFT_eSPI();

void setup() {
  Serial.begin(115200);
  pinMode(PIN_POWER_ON, OUTPUT);
  digitalWrite(PIN_POWER_ON, HIGH);
  pinMode(14, INPUT_PULLUP);
  WiFi.begin("SKYSXQIX", "8RfGzsJxcrWd");

  tft.begin();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setTextWrap(true);
  tft.setCursor(0, 170);
  tft.setTextSize(3);
  update_crypto("bitcoin,ethereum,solana");
}

void loop() {
  unsigned long currenttime = millis();
  if ((currenttime - last_update) > UPDATE_INTERVAL) {
    last_update = currenttime;
    update_crypto("bitcoin,ethereum,solana");
  }
}
