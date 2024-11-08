/**
 * Coincap API ticker
 *
 *  Created on: 24.05.2015 - https://github.com/osbock
 *  Updated on: 08.11.2024 - https://github.com/goodalexhunting
 *
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include "config.h"
#include "TFT_eSPI.h"/* Please use the TFT library provided in the library. */

TFT_eSPI tft = TFT_eSPI();

extern void update_crypto(String coins);
extern void update_stock(String symbol);

unsigned int iteration=0;
unsigned long last_update =0L;

void setup() {
  Serial.begin(115200);
  pinMode(PIN_POWER_ON, OUTPUT);
  digitalWrite(PIN_POWER_ON, HIGH);
  pinMode(14,INPUT_PULLUP);
  WiFi.begin("ssid","passwd");

  tft.begin();
  tft.setRotation(3);
  tft.fillScreen(CUSTOM_DARK);
  tft.setTextColor(TFT_WHITE);
  tft.setTextWrap(true);
  tft.setCursor(0, 170);
  tft.setTextSize(2);
  update_crypto("bitcoin,ethereum,solana");
}
#define UPDATE_INTERVAL 1000*60*1

void loop() {
  unsigned long currenttime = millis();
  if ((currenttime - last_update) > UPDATE_INTERVAL){
    last_update = currenttime;
    update_crypto("bitcoin,ethereum,solana");
  }

}

  
