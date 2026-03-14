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
extern bool scrollTextCycleCompleted;
extern String serviceText;
extern bool serviceTextActive;
extern uint32_t serviceTextStartTime;
extern uint16_t serviceTextDuration;
extern int serviceTextScrollX;
extern bool serviceTextScrollCompleted;

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

// Unified text display: centers if text fits, scrolls if wider.
// Returns true when a full scroll cycle just completed (always true for centered text).
bool displayText(const String& text, int &scrollX, uint16_t color) {
  dma_display->setTextSize(TEXT_SIZE);
  dma_display->setTextWrap(false);
  dma_display->setTextColor(color);

  int textWidth = calculateTextWidth(text);

  if (textWidth <= DISPLAY_WIDTH) {
    // Text fits on display — center it
    int centerX = max(0, (DISPLAY_WIDTH - textWidth) / 2);
    dma_display->setCursor(centerX, DISPLAY_Y_OFFSET);
    dma_display->print(text.c_str());
    return true;
  }

  // Text wider than display — scroll it
  dma_display->setCursor(scrollX, DISPLAY_Y_OFFSET);
  dma_display->print(text.c_str());

  scrollX -= SCROLL_SPEED;
  if (scrollX < -textWidth) {
    scrollX = DISPLAY_WIDTH;
    return true;  // One full cycle completed
  }
  return false;
}

// Display scrolling text with rainbow colors (mode 1)
void displayScrollText() {
  uint16_t currentColor = hsvToRGB(colorHue);
  bool cycleCompleted = displayText(scrollText, textScrollX, currentColor);
  if (cycleCompleted) scrollTextCycleCompleted = true;
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

  char dateStr[11];
  strftime(dateStr, sizeof(dateStr), "%d/%m/%Y", timeinfo);

  const int timeTextSize = TEXT_SIZE;
  const int dateTextSize = 1;
  const int timeCharWidth = 6 * timeTextSize;
  const int dateCharWidth = 6 * dateTextSize;
  const int timeChars = 8;  // HH:MM:SS
  const int dateChars = 10; // DD/MM/YYYY
  const int timeHeight = 8 * timeTextSize;
  const int dateHeight = 8 * dateTextSize;
  const int lineGap = 2;

  int blockHeight = timeHeight + lineGap + dateHeight;
  int startY = max(0, (PANEL_HEIGHT - blockHeight) / 2);
  int timeY = startY;
  int dateY = timeY + timeHeight + lineGap;

  int timeCenterX = max(0, (DISPLAY_WIDTH - timeChars * timeCharWidth) / 2);
  int dateCenterX = max(0, (DISPLAY_WIDTH - dateChars * dateCharWidth) / 2);

  dma_display->setTextSize(timeTextSize);
  dma_display->setTextColor(dma_display->color565(255, 100, 255));
  dma_display->setCursor(timeCenterX, timeY);

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

  dma_display->setTextSize(dateTextSize);
  dma_display->setTextColor(dma_display->color565(220, 220, 220));
  dma_display->setCursor(dateCenterX, dateY);
  dma_display->print(dateStr);
}

// Display service text (MQTT notification — temporary, does not override scrollText)
// Guarantees at least one full scroll cycle before dismissing.
void displayServiceText() {
  if (!serviceTextActive || serviceText.length() == 0) {
    return;
  }

  uint32_t elapsedMs = millis() - serviceTextStartTime;
  bool durationExpired = elapsedMs > (serviceTextDuration * 1000UL);

  // Cyan color for service/notification text
  uint16_t cyan = dma_display->color565(0, 255, 255);
  bool cycleJustCompleted = displayText(serviceText, serviceTextScrollX, cyan);

  if (cycleJustCompleted) serviceTextScrollCompleted = true;

  // Only deactivate when duration expired AND at least one full scroll cycle done
  if (durationExpired && serviceTextScrollCompleted) {
    serviceTextActive = false;
    serviceTextScrollX = DISPLAY_WIDTH;
    serviceTextScrollCompleted = false;
    return;
  }
}

#endif // DISPLAY_MANAGER_H
