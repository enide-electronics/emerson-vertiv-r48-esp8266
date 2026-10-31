#include "nokia_display.h"

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>
#include <string.h>

namespace {
const uint8_t DISPLAY_DC_PIN = D1;
const uint8_t DISPLAY_CS_PIN = D2;
const uint8_t DISPLAY_RESET_PIN = D3;
const uint8_t BACKLIGHT_PIN = D4;
const unsigned long DISPLAY_REFRESH_INTERVAL = 1000;

Adafruit_PCD8544 display(DISPLAY_DC_PIN, DISPLAY_CS_PIN, DISPLAY_RESET_PIN);
unsigned long lastDisplayRefresh = 0;

void drawWifiIcon() {
  display.drawLine(1, 1, 11, 1, BLACK);
  display.drawLine(3, 3, 9, 3, BLACK);
  display.drawLine(5, 5, 7, 5, BLACK);
  display.fillCircle(6, 8, 1, BLACK);
}

void drawNetworkName(const char *networkName) {
  char label[12];
  if (networkName == nullptr || networkName[0] == '\0') {
    snprintf(label, sizeof(label), "No WiFi");
  } else {
    snprintf(label, sizeof(label), "%.11s", networkName);
  }

  drawWifiIcon();
  display.setCursor(15, 0);
  display.print(label);
}

void drawIpAddress(const IPAddress &ipAddress) {
  char address[16];
  snprintf(address, sizeof(address), "%u.%u.%u.%u",
           ipAddress[0], ipAddress[1], ipAddress[2], ipAddress[3]);

  char firstLine[15];
  char secondLine[8];
  const size_t addressLength = strlen(address);
  size_t split = addressLength < 11 ? addressLength : 11;
  while (split > 0 && split < addressLength && address[split] != '.') {
    --split;
  }
  snprintf(firstLine, sizeof(firstLine), "IP:%.*s", (int)split, address);
  snprintf(secondLine, sizeof(secondLine), "%s", address + split);

  display.setCursor(0, 32);
  display.print(firstLine);
  display.setCursor(0, 40);
  display.print(secondLine);
}
}

void nokiaDisplayBegin() {
  digitalWrite(BACKLIGHT_PIN, HIGH);
  pinMode(BACKLIGHT_PIN, OUTPUT);
  analogWriteRange(255);
  analogWriteFreq(1000);
  analogWrite(BACKLIGHT_PIN, 220);

  display.begin();
  display.setContrast(0x43);
  display.setBias(0x02);
  display.setTextSize(1);
  display.setTextColor(BLACK);
  display.clearDisplay();
  display.display();
}

void nokiaDisplayShowStatus(const char *line1, const char *line2) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(BLACK);
  display.setCursor(0, 8);
  if (line1 != nullptr) {
    display.print(line1);
  }
  display.setCursor(0, 24);
  if (line2 != nullptr) {
    display.print(line2);
  }
  display.display();
}

void nokiaDisplayClear() {
  display.clearDisplay();
  display.display();
  lastDisplayRefresh = millis() - DISPLAY_REFRESH_INTERVAL;
}

void nokiaDisplayUpdate(float outputVoltage,
                        float outputCurrent,
                        float outputCurrentLimit,
                        const char *networkName,
                        const IPAddress &ipAddress) {
  if (millis() - lastDisplayRefresh < DISPLAY_REFRESH_INTERVAL) {
    return;
  }
  lastDisplayRefresh = millis();

  display.clearDisplay();
  drawNetworkName(networkName);

  char line[16];
  display.setCursor(0, 8);
  snprintf(line, sizeof(line), "V:%0.2fV", outputVoltage);
  display.print(line);

  display.setCursor(0, 16);
  snprintf(line, sizeof(line), "I:%0.2fA", outputCurrent);
  display.print(line);

  display.setCursor(0, 24);
  snprintf(line, sizeof(line), "Lim:%0.1f%%", outputCurrentLimit * 100.0f);
  display.print(line);

  drawIpAddress(ipAddress);
  display.display();
}