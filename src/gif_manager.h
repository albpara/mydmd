#ifndef GIF_MANAGER_H
#define GIF_MANAGER_H

#include <AnimatedGIF.h>
#include <SD.h>
#include "config.h"

// Forward declarations
extern MatrixPanel_I2S_DMA *dma_display;
extern bool sdMounted;

// ==================== GIF State ====================

AnimatedGIF gif;
static File FSGifFile;  // Handle for the currently open GIF file

// GIF inventory (count only — no paths stored in RAM)
int totalGifFiles = 0;

// Playback state
bool gifPlaying = false;
int currentGifIndex = -1;
char currentGifPath[256];  // Single path buffer for the active GIF (supports nested subfolders)
unsigned long gifStartTime = 0;
unsigned long gifNextFrameTime = 0;

// Service GIF (MQTT-requested specific GIF)
String serviceGifPath = "";
bool serviceGifActive = false;
uint16_t serviceGifDuration = 0;
uint32_t serviceGifStartTime = 0;

// ==================== AnimatedGIF Callbacks ====================

static void *GIFOpenFile(const char *fname, int32_t *pSize) {
  FSGifFile = SD.open(fname);
  if (FSGifFile) {
    *pSize = FSGifFile.size();
    return (void *)&FSGifFile;
  }
  return NULL;
}

static void GIFCloseFile(void *pHandle) {
  File *f = static_cast<File *>(pHandle);
  if (f != NULL)
    f->close();
}

static int32_t GIFReadFile(GIFFILE *pFile, uint8_t *pBuf, int32_t iLen) {
  int32_t iBytesRead = iLen;
  File *f = static_cast<File *>(pFile->fHandle);
  if ((pFile->iSize - pFile->iPos) < iLen)
    iBytesRead = pFile->iSize - pFile->iPos - 1;
  if (iBytesRead <= 0)
    return 0;
  iBytesRead = (int32_t)f->read(pBuf, iBytesRead);
  pFile->iPos = f->position();
  return iBytesRead;
}

static int32_t GIFSeekFile(GIFFILE *pFile, int32_t iPosition) {
  File *f = static_cast<File *>(pFile->fHandle);
  f->seek(iPosition);
  pFile->iPos = (int32_t)f->position();
  return pFile->iPos;
}

// Draw a line of GIF image directly on the LED matrix
void GIFDraw(GIFDRAW *pDraw) {
  uint8_t *s;
  uint16_t *d, *usPalette, usTemp[320];
  int x, y, iWidth;

  int baseX = pDraw->iX;
  iWidth = pDraw->iWidth;
  if (iWidth + baseX > DISPLAY_WIDTH)
    iWidth = DISPLAY_WIDTH - baseX;
  usPalette = pDraw->pPalette;
  y = pDraw->iY + pDraw->y;

  s = pDraw->pPixels;
  if (pDraw->ucDisposalMethod == 2) {
    for (x = 0; x < iWidth; x++) {
      if (s[x] == pDraw->ucTransparent)
        s[x] = pDraw->ucBackground;
    }
    pDraw->ucHasTransparency = 0;
  }

  if (pDraw->ucHasTransparency) {
    uint8_t *pEnd, c, ucTransparent = pDraw->ucTransparent;
    int iCount;
    pEnd = s + iWidth;
    x = 0;
    iCount = 0;
    while (x < iWidth) {
      c = ucTransparent - 1;
      d = usTemp;
      while (c != ucTransparent && s < pEnd) {
        c = *s++;
        if (c == ucTransparent) {
          s--;
        } else {
          *d++ = usPalette[c];
          iCount++;
        }
      }
      if (iCount) {
        for (int xOffset = 0; xOffset < iCount; xOffset++) {
          dma_display->drawPixel(baseX + x + xOffset, y, usTemp[xOffset]);
        }
        x += iCount;
        iCount = 0;
      }
      iCount = 0;
      while (s < pEnd && *s == ucTransparent) {
        s++;
        iCount++;
      }
      if (iCount) {
        x += iCount;
        iCount = 0;
      }
    }
  } else {
    s = pDraw->pPixels;
    for (x = 0; x < iWidth; x++)
      dma_display->drawPixel(baseX + x, y, usPalette[*s++]);
  }
}

// ==================== GIF Inventory (lista.txt based) ====================

// Count lines in lista.txt to determine total GIF count
void countGifFiles() {
  if (!sdMounted) {
    LOG("[GIF] SD card not mounted, skipping");
    return;
  }

  File f = SD.open(GIF_LIST_FILE, FILE_READ);
  if (!f) {
    LOGF("[GIF] %s not found on SD card\n", GIF_LIST_FILE);
    return;
  }

  totalGifFiles = 0;
  unsigned long start = millis();

  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.length() > 0) {
      totalGifFiles++;
    }
  }
  f.close();

  unsigned long elapsed = millis() - start;
  LOGF("[GIF] lista.txt: %d entries loaded in %lu ms\n", totalGifFiles, elapsed);
}

// Get the path of the Nth GIF by reading the Nth non-empty line from lista.txt
bool getGifPathByIndex(int index, char *outPath, size_t outSize) {
  File f = SD.open(GIF_LIST_FILE, FILE_READ);
  if (!f) return false;

  int count = 0;
  bool found = false;

  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) continue;

    if (count == index) {
      strncpy(outPath, line.c_str(), outSize - 1);
      outPath[outSize - 1] = '\0';
      found = true;
      break;
    }
    count++;
  }
  f.close();
  return found;
}

// ==================== Playback Control ====================

// Pick a random GIF index different from current
int pickRandomGifIndex() {
  if (totalGifFiles == 0) return -1;
  if (totalGifFiles == 1) return 0;

  int newIndex;
  do {
    newIndex = random(0, totalGifFiles);
  } while (newIndex == currentGifIndex);
  return newIndex;
}

// Open a GIF file for playback. Returns true if successful.
bool openGif(const char *path, bool silent = false) {
  if (gifPlaying) {
    gif.close();
    gifPlaying = false;
  }

  if (!gif.open(path, GIFOpenFile, GIFCloseFile, GIFReadFile, GIFSeekFile, GIFDraw)) {
    LOGF("[GIF] Failed to open: %s\n", path);
    return false;
  }

  if (!silent) {
    LOGF("[GIF] Playing: %s (%dx%d)\n", path, gif.getCanvasWidth(), gif.getCanvasHeight());
  }
  gifPlaying = true;
  gifStartTime = millis();
  gifNextFrameTime = 0;
  return true;
}

// Start playing a random GIF from the library
bool startRandomGif() {
  if (totalGifFiles == 0) return false;

  int idx = pickRandomGifIndex();
  if (idx < 0) return false;

  currentGifIndex = idx;

  // Look up the file path by index (directory seek)
  if (!getGifPathByIndex(idx, currentGifPath, sizeof(currentGifPath))) {
    LOGF("[GIF] Could not find GIF at index %d\n", idx);
    return false;
  }

  return openGif(currentGifPath);
}

// Start playing a specific GIF by path (for MQTT service)
bool startSpecificGif(const char *path) {
  if (!sdMounted) {
    LOG("[GIF] SD not mounted");
    return false;
  }

  // Verify file exists
  if (!SD.exists(path)) {
    LOGF("[GIF] File not found: %s\n", path);
    return false;
  }

  return openGif(path);
}

// Play one frame of the current GIF (non-blocking).
// Returns true if GIF is still playing, false if finished.
bool playGifFrame() {
  if (!gifPlaying) return false;

  // Check max duration
  if (millis() - gifStartTime > GIF_MAX_DURATION) {
    gif.close();
    gifPlaying = false;
    return false;
  }

  // Respect frame timing
  if (millis() < gifNextFrameTime) return true;

  int frameDelay = 0;
  if (!gif.playFrame(false, &frameDelay)) {
    // GIF finished (all frames played)
    gif.close();
    gifPlaying = false;
    return false;
  }

  // Schedule next frame
  gifNextFrameTime = millis() + frameDelay;
  return true;
}

// ==================== Display Functions ====================

// Display GIF mode: plays random GIFs from /gifs/ directory
void displayGif() {
  if (!sdMounted || totalGifFiles == 0) return;

  // If no GIF is playing, start a random one
  if (!gifPlaying) {
    if (!startRandomGif()) return;
  }

  // Play one frame
  if (!playGifFrame()) {
    // Current GIF finished, start next random one
    startRandomGif();
  }
}

// Display a service GIF (MQTT-requested specific path)
void displayServiceGif() {
  if (!serviceGifActive || serviceGifPath.length() == 0) return;

  // Check if duration has expired
  if (serviceGifDuration > 0) {
    uint32_t elapsedMs = millis() - serviceGifStartTime;
    if (elapsedMs > (serviceGifDuration * 1000UL)) {
      serviceGifActive = false;
      if (gifPlaying) {
        gif.close();
        gifPlaying = false;
      }
      LOG("[GIF] Service GIF duration expired");
      return;
    }
  }

  // Start the GIF if not already playing
  if (!gifPlaying) {
    if (!startSpecificGif(serviceGifPath.c_str())) {
      serviceGifActive = false;
      LOG("[GIF] Service GIF failed to open, deactivating");
      return;
    }
  }

  // Play one frame; if GIF ends, loop it
  if (!playGifFrame()) {
    // Re-open for looping
    openGif(serviceGifPath.c_str());
  }
}

// ==================== Splash GIF ====================

bool splashGifReady = false;

// Open splash.gif for looping display during boot
void initSplashGif() {
  if (!sdMounted) return;
  if (!SD.exists(SPLASH_GIF_FILE)) {
    LOGF("[GIF] Splash file not found: %s\n", SPLASH_GIF_FILE);
    return;
  }
  if (openGif(SPLASH_GIF_FILE)) {
    splashGifReady = true;
    LOG("[GIF] Splash GIF ready");
  }
}

// Display one frame of splash.gif (non-blocking, loops)
void displaySplashGif() {
  if (!splashGifReady) return;

  if (!gifPlaying) {
    // Re-open to loop (silent — no log spam)
    if (!openGif(SPLASH_GIF_FILE, true)) {
      splashGifReady = false;
      return;
    }
  }

  if (!playGifFrame()) {
    // GIF ended, re-open for loop (silent)
    openGif(SPLASH_GIF_FILE, true);
  }
}

// Stop splash GIF playback
void stopSplashGif() {
  if (gifPlaying) {
    gif.close();
    gifPlaying = false;
  }
  splashGifReady = false;
}

// ==================== Initialization ======================================

void initGifManager() {
  LOG("\n========== GIF Manager ==========");
  gif.begin(LITTLE_ENDIAN_PIXELS);
  randomSeed(analogRead(0) + millis());
  countGifFiles();
}

#endif // GIF_MANAGER_H
