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
const unsigned long DISPLAY_REFRESH_INTERVAL = 50;
const unsigned long CAN_ICON_DURATION = 1000;
const uint8_t WIFI_ICON_WIDTH = 8;
const uint8_t WIFI_ICON_HEIGHT = 6;
const uint8_t CAN_ICON_WIDTH = 8;
const uint8_t CAN_ICON_HEIGHT = 8;
/*
const uint8_t WIFI_ICON_WIDTH = 14;
const uint8_t WIFI_ICON_HEIGHT = 7;
const uint8_t PROGMEM wifiOnlineBitmap[] = {
  0x0F, 0xC0, 0x30, 0x30, 0x40, 0x08, 0x03, 0x00,
  0x0C, 0x30, 0x03, 0x00, 0x02, 0x00
};


const uint8_t PROGMEM wifiOfflineBitmap[] = {
  0x0F, 0xC2, 0x30, 0x30, 0x40, 0x88, 0x03, 0x00,
  0x1C, 0x30, 0x23, 0x00, 0x82, 0x00
};

*/
const uint8_t PROGMEM wifiOnlineBitmap[] = {
  0x3C, // Row 0: Top wave (columns 1-6)
  0x42, // Row 1
  0x18, // Row 2
  0x24, // Row 3
  0x00, // Row 4
  0x18  // Row 4
};

const uint8_t PROGMEM wifiOfflineBitmap[] = {
  0x42,
  0x24,
  0x18,
  0x18,
  0x24,
  0x42
};

const uint8_t PROGMEM canActivityBitmap[] = {
  0x7E, 0x42, 0x5A, 0x5A, 0x5A, 0x42, 0x7E, 0x00
};

struct TinyGlyph {
  char character;
  uint8_t rows[5];
};

const TinyGlyph tinyFont[] PROGMEM = {
  {' ', {0, 0, 0, 0, 0}},
  {'0', {7, 5, 5, 5, 7}}, {'1', {2, 6, 2, 2, 7}},
  {'2', {6, 1, 2, 4, 7}}, {'3', {6, 1, 2, 1, 6}},
  {'4', {5, 5, 7, 1, 1}}, {'5', {7, 4, 6, 1, 6}},
  {'6', {3, 4, 6, 5, 2}}, {'7', {7, 1, 2, 2, 2}},
  {'8', {7, 5, 7, 5, 7}}, {'9', {2, 5, 3, 1, 6}},
  {'A', {2, 5, 7, 5, 5}}, {'B', {6, 5, 6, 5, 6}},
  {'C', {3, 4, 4, 4, 3}}, {'D', {6, 5, 5, 5, 6}},
  {'E', {7, 4, 6, 4, 7}}, {'F', {7, 4, 6, 4, 4}},
  {'G', {3, 4, 5, 5, 3}}, {'H', {5, 5, 7, 5, 5}},
  {'I', {7, 2, 2, 2, 7}}, {'J', {1, 1, 1, 5, 2}},
  {'K', {5, 5, 6, 5, 5}}, {'L', {4, 4, 4, 4, 7}},
  {'M', {5, 7, 7, 5, 5}}, {'N', {5, 7, 7, 7, 5}},
  {'O', {2, 5, 5, 5, 2}}, {'P', {6, 5, 6, 4, 4}},
  {'Q', {2, 5, 5, 7, 3}}, {'R', {6, 5, 6, 5, 5}},
  {'S', {3, 4, 2, 1, 6}}, {'T', {7, 2, 2, 2, 2}},
  {'U', {5, 5, 5, 5, 7}}, {'V', {5, 5, 5, 5, 2}},
  {'W', {5, 5, 7, 7, 5}}, {'X', {5, 5, 2, 5, 5}},
  {'Y', {5, 5, 2, 2, 2}}, {'Z', {7, 1, 2, 4, 7}},
  {'a', {0, 3, 5, 7, 5}}, {'b', {4, 6, 5, 5, 6}},
  {'c', {0, 3, 4, 4, 3}}, {'d', {1, 3, 5, 5, 3}},
  {'e', {0, 2, 5, 6, 3}}, {'f', {1, 2, 7, 2, 2}},
  {'g', {0, 3, 5, 3, 6}}, {'h', {4, 6, 5, 5, 5}},
  {'i', {2, 0, 2, 2, 2}}, {'j', {1, 0, 1, 1, 6}},
  {'k', {4, 5, 6, 5, 5}}, {'l', {2, 2, 2, 2, 1}},
  {'m', {0, 7, 7, 5, 5}}, {'n', {0, 6, 5, 5, 5}},
  {'o', {0, 2, 5, 5, 2}}, {'p', {0, 6, 5, 6, 4}},
  {'q', {0, 3, 5, 3, 1}}, {'r', {0, 5, 6, 4, 4}},
  {'s', {0, 3, 4, 2, 6}}, {'t', {2, 7, 2, 2, 1}},
  {'u', {0, 5, 5, 5, 3}}, {'v', {0, 5, 5, 5, 2}},
  {'w', {0, 5, 5, 7, 7}}, {'x', {0, 5, 2, 2, 5}},
  {'y', {0, 5, 3, 1, 6}}, {'z', {0, 7, 1, 2, 7}},
  {'-', {0, 0, 7, 0, 0}}, {'_', {0, 0, 0, 0, 7}},
  {'.', {0, 0, 0, 0, 2}}, {':', {0, 2, 0, 2, 0}},
  {'%', {5, 1, 2, 4, 5}}, {'/', {1, 1, 2, 4, 4}}
};

Adafruit_PCD8544 display(DISPLAY_DC_PIN, DISPLAY_CS_PIN, DISPLAY_RESET_PIN);
unsigned long lastDisplayRefresh = 0;
unsigned long canActivityUntil = 0;

bool canIconVisible(unsigned long now) {
  return static_cast<long>(canActivityUntil - now) > 0;
}

void drawTinyCharacter(char character, int16_t x, int16_t y) {
  for (size_t glyphIndex = 0; glyphIndex < sizeof(tinyFont) / sizeof(tinyFont[0]); ++glyphIndex) {
    if (pgm_read_byte(&tinyFont[glyphIndex].character) == character) {
      for (uint8_t row = 0; row < 5; ++row) {
        const uint8_t pixels = pgm_read_byte(&tinyFont[glyphIndex].rows[row]);
        for (uint8_t column = 0; column < 3; ++column) {
          if (pixels & (1 << (2 - column))) {
            display.drawPixel(x + column, y + row, BLACK);
          }
        }
      }
      return;
    }
  }
}

uint8_t drawTinyText(const char *text, int16_t x, int16_t y, uint8_t maxCharacters = 255) {
  if (text == nullptr) {
    return 0;
  }

  uint8_t count = 0;
  while (text[count] != '\0' && count < maxCharacters) {
    drawTinyCharacter(text[count], x + count * 4, y);
    ++count;
  }
  return count;
}

void drawTinyTextRight(const char *text, int16_t right, int16_t y, int16_t leftLimit) {
  if (text == nullptr) {
    return;
  }

  const size_t textLength = strlen(text);
  const uint8_t maxCharacters = static_cast<uint8_t>((right - leftLimit + 1) / 4);
  char clipped[24];
  const char *visibleText = text;
  if (textLength > maxCharacters) {
    const uint8_t prefixLength = maxCharacters > 3 ? maxCharacters - 3 : maxCharacters;
    memcpy(clipped, text, prefixLength);
    if (maxCharacters > 3) {
      memcpy(clipped + prefixLength, "...", 3);
    }
    clipped[prefixLength + (maxCharacters > 3 ? 3 : 0)] = '\0';
    visibleText = clipped;
  }

  const uint8_t visibleLength = strlen(visibleText);
  const int16_t x = right - visibleLength * 4 + 1;
  drawTinyText(visibleText, x < leftLimit ? leftLimit : x, y, maxCharacters);
}

void drawHeader(const char *networkName, bool wifiOnline, bool showCanIcon) {
  if (showCanIcon) {
    display.drawBitmap(0, 0, canActivityBitmap, CAN_ICON_WIDTH, CAN_ICON_HEIGHT, BLACK);
  }

  const uint8_t iconX = display.width() - WIFI_ICON_WIDTH;
  const uint8_t textLeft = showCanIcon ? CAN_ICON_WIDTH + 2 : 0;
  drawTinyTextRight(networkName, iconX - 2, 1, textLeft);

  const uint8_t *wifiBitmap = wifiOnline ? wifiOnlineBitmap : wifiOfflineBitmap;
  display.drawBitmap(iconX, 0, wifiBitmap, WIFI_ICON_WIDTH, WIFI_ICON_HEIGHT, BLACK);
  display.drawLine(0, 7, display.width() - 1, 7, BLACK);
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
  display.clearDisplay();
  display.display();
}

void nokiaDisplayAddAsLog(const char *line1, bool clearScreen) {
  // current screen position
  static uint16_t x=0, y=0;

  // line breaks are only honoured at the end of string
  bool lineBreak = false;

  if(clearScreen) {
    display.clearDisplay();
    x=0;
    y=0;
  }

  // find if we need to rollover to the next line
  size_t len = strlen(line1);
  if (line1[len - 1] == '\n') {
    len -= 1;
    lineBreak = true;
  }

  // find last char index of current line
  uint lineCharsLeft = 84 - x;
  uint splitAtCharIndex = lineCharsLeft / 4;

  // either rollover when length of line1 is over the available line space
  // or just place chars on the current line
  if (len > splitAtCharIndex) {
    drawTinyText(line1, x, y, splitAtCharIndex);  
    x=0;
    y+=6;
    drawTinyText(line1 + splitAtCharIndex, x, y, 21);  
    x += (len - splitAtCharIndex) * 4;
  } else {
    drawTinyText(line1, x, y, 21);
    x += len * 4;
  }

  // line break as end char will move "virtual caret" to the next line
  if (lineBreak) {
    y += 6;
    x = 0;
  }
  
  display.display();
}

void nokiaDisplayClear() {
  display.clearDisplay();
  display.display();
  lastDisplayRefresh = millis() - DISPLAY_REFRESH_INTERVAL;
}

void nokiaDisplayNotifyCanMessage() {
  canActivityUntil = millis() + CAN_ICON_DURATION;
  lastDisplayRefresh = millis() - DISPLAY_REFRESH_INTERVAL;
}

void nokiaDisplayUpdate(float outputVoltage,
                        float outputCurrent,
                        float outputCurrentLimit,
                        const char *networkName,
                        const IPAddress &ipAddress,
                        bool wifiOnline) {
  if (millis() - lastDisplayRefresh < DISPLAY_REFRESH_INTERVAL) {
    return;
  }
  lastDisplayRefresh = millis();

  display.clearDisplay();
  const bool showCanIcon = canIconVisible(lastDisplayRefresh);
  drawHeader(networkName, wifiOnline, showCanIcon);

  char line[20];
  snprintf(line, sizeof(line), "Vdc:%0.1fV", outputVoltage);
  drawTinyText(line, 0, 10);

  snprintf(line, sizeof(line), "Idc:%0.1fA", outputCurrent);
  drawTinyText(line, 0, 17);

  snprintf(line, sizeof(line), "Ilim:%0.1f%%", outputCurrentLimit * 100.0f);
  drawTinyText(line, 43, 17); // 0,28

  snprintf(line, sizeof(line), "Pdc:%0.1fW", outputVoltage * outputCurrent);
  drawTinyText(line, 0, 24);

  snprintf(line, sizeof(line), "IP:%u.%u.%u.%u",
           ipAddress[0], ipAddress[1], ipAddress[2], ipAddress[3]);
  drawTinyText(line, 0, 41);
  display.display();
}