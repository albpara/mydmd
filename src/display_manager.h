#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <time.h>
#include "config.h"

// Forward declarations
extern MatrixPanel_I2S_DMA *dma_display;
extern String scrollText;
extern bool wifiConnected;
extern bool ntpSynchronized;
extern bool isBootupPhase;
extern float colorHue;
extern int textScrollX;
extern String serviceText;
extern bool serviceTextActive;
extern uint32_t serviceTextStartTime;
extern uint16_t serviceTextDuration;

// Convert HSV color to RGB 565
uint16_t hsvToRGB(float hue) {
  float h = (hue >= 360.0) ? fmod(hue, 360.0) : hue;
  float c = 1.0;  // v * s where v=1.0, s=1.0
  float x = c * (1.0 - fabs(fmod(h / 60.0, 2.0) - 1.0));

  float r = 0, g = 0, b = 0;
  if (h < 60) { r = c; g = x; }
  else if (h < 120) { r = x; g = c; }
  else if (h < 180) { g = c; b = x; }
  else if (h < 240) { b = c; r = x; }
  else if (h < 300) { r = x; b = c; }
  else { r = c; b = x; }

  return dma_display->color565((int)(r * 255), (int)(g * 255), (int)(b * 255));
}

// Calculate text width based on character count and text size
int calculateTextWidth(const String& text) {
  return text.length() * (6 * TEXT_SIZE);
}

// Display boot/loading animation
void displayBootAnimation() {
  Serial.println("[BOOT] Mostrando animación de carga...");
  for (int frame = 0; frame < 3; frame++) {
    dma_display->fillScreen(0);

    // Simple animated pattern: shift colors
    uint16_t color = dma_display->color565(
      (frame * 100) % 255,
      (frame * 80) % 255,
      (frame * 60) % 255
    );

    // Draw "LOADING" text
    dma_display->setTextSize(1);
    dma_display->setTextColor(color);

    int centerX = max(0, (PANEL_WIDTH * 2 - (7) * 6) / 2);

    dma_display->setCursor(centerX, 12);
    dma_display->print("LOADING");

    delay(300);
  }
  dma_display->fillScreen(0);
  Serial.println("[BOOT] Animación completada");
}

// Display scrolling text with rainbow colors
void displayScrollText() {
  dma_display->setTextSize(TEXT_SIZE);
  dma_display->setTextWrap(false);
  uint16_t currentColor = hsvToRGB(colorHue);
  dma_display->setTextColor(currentColor);

  int textWidth = calculateTextWidth(scrollText);
  dma_display->setCursor(textScrollX, DISPLAY_Y_OFFSET);
  dma_display->print(scrollText.c_str());

  textScrollX -= SCROLL_SPEED;
  if (textScrollX < -textWidth) textScrollX = DISPLAY_WIDTH;

  colorHue += 1.0;
  if (colorHue >= 360.0) colorHue = 0.0;
}

// Display clock only if NTP time is synchronized
void displayClock() {
  if (!wifiConnected || !ntpSynchronized) return;

  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);
  char timeStr[9];
  strftime(timeStr, sizeof(timeStr), "%H:%M:%S", timeinfo);

  dma_display->setTextSize(TEXT_SIZE);
  dma_display->setTextColor(dma_display->color565(255, 100, 255));

  int centerX = max(0, (DISPLAY_WIDTH - (10 + 2) * 8) / 2);
  dma_display->setCursor(centerX, DISPLAY_Y_OFFSET);

  // Colon blinks synchronized to the exact moment seconds change
  bool showColon = (timeinfo->tm_sec % 2) == 0;

  if (showColon) {
    dma_display->print(timeStr);
  } else {
    // Replace colons with spaces - avoid String allocation
    for (int i = 0; timeStr[i]; i++) {
      dma_display->print(timeStr[i] == ':' ? ' ' : timeStr[i]);
    }
  }
}

// Display service text (called from MQTT command)
void displayServiceText() {
  if (!serviceTextActive || serviceText.length() == 0) {
    return;
  }

  // Check if duration has expired
  uint32_t elapsedMs = millis() - serviceTextStartTime;
  if (elapsedMs > (serviceTextDuration * 1000UL)) {
    serviceTextActive = false;
    return;
  }

  dma_display->setTextSize(2);  // Font size 2 as requested
  dma_display->setTextWrap(false);

  // Cyan color for service text
  dma_display->setTextColor(dma_display->color565(0, 255, 255));

  int textWidth = serviceText.length() * 12;  // 6 * 2 = 12 pixels per char at size 2

  if (textWidth > DISPLAY_WIDTH) {
    // Text is larger than display, scroll it
    static int serviceTextScrollX = DISPLAY_WIDTH;
    dma_display->setCursor(serviceTextScrollX, DISPLAY_Y_OFFSET);
    dma_display->print(serviceText.c_str());

    serviceTextScrollX -= SCROLL_SPEED;
    if (serviceTextScrollX < -textWidth) {
      serviceTextScrollX = DISPLAY_WIDTH;
    }
  } else {
    // Text fits on display, center it
    int centerX = max(0, (DISPLAY_WIDTH - textWidth) / 2);
    dma_display->setCursor(centerX, DISPLAY_Y_OFFSET);
    dma_display->print(serviceText.c_str());
  }
}

#endif // DISPLAY_MANAGER_H
