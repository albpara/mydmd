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

// ==================== GIF Inventory (zero-RAM, recursive) ====================

// Helper: check if filename ends with .gif (case-insensitive)
static bool isGifFile(const char* fname) {
  size_t len = strlen(fname);
  if (len < 5) return false;
  const char* ext = fname + len - 4;
  return (strcasecmp(ext, ".gif") == 0);
}

// Recursively count GIF files in dirPath and all subdirectories
static void countGifFilesInDir(const char* dirPath) {
  File dir = SD.open(dirPath);
  if (!dir || !dir.isDirectory()) {
    Serial.printf("[GIF] Cannot open dir: %s\n", dirPath);
    if (dir) dir.close();
    return;
  }

  Serial.printf("[GIF] Scanning: %s\n", dirPath);
  int localCount = 0;

  File file = dir.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      char subPath[256];
      const char* fname = file.name();
      if (fname[0] == '/') {
        strncpy(subPath, fname, sizeof(subPath) - 1);
        subPath[sizeof(subPath) - 1] = '\0';
      } else {
        snprintf(subPath, sizeof(subPath), "%s/%s", dirPath, fname);
      }
      file.close();
      countGifFilesInDir(subPath);
    } else {
      if (isGifFile(file.name())) {
        totalGifFiles++;
        localCount++;
        if (totalGifFiles % 500 == 0) {
          Serial.printf("[GIF] ... %d files found so far\n", totalGifFiles);
        }
      }
      file.close();
    }
    file = dir.openNextFile();
  }
  dir.close();
  Serial.printf("[GIF] %s -> %d gifs (total so far: %d)\n", dirPath, localCount, totalGifFiles);
}

// Count all GIF files under GIF_DIRECTORY (recursive)
void countGifFiles() {
  if (!sdMounted) {
    Serial.println("[GIF] SD card not mounted, skipping scan");
    return;
  }

  totalGifFiles = 0;
  unsigned long scanStart = millis();
  Serial.println("[GIF] Counting GIF files in " + String(GIF_DIRECTORY) + " (recursive)...");
  countGifFilesInDir(GIF_DIRECTORY);
  unsigned long scanTime = millis() - scanStart;
  Serial.printf("[GIF] Scan complete: %d GIF files found in %lu ms\n", totalGifFiles, scanTime);
}

// Recursively find the Nth GIF file across dirPath and all subdirectories.
// *currentCount tracks position across recursive calls.
static bool findGifByIndex(const char* dirPath, int targetIndex, int* currentCount, char* outPath, size_t outSize) {
  File dir = SD.open(dirPath);
  if (!dir || !dir.isDirectory()) {
    if (dir) dir.close();
    return false;
  }

  Serial.printf("[GIF] Seeking #%d in %s (at %d)\n", targetIndex, dirPath, *currentCount);
  File file = dir.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      char subPath[256];
      const char* fname = file.name();
      if (fname[0] == '/') {
        strncpy(subPath, fname, sizeof(subPath) - 1);
        subPath[sizeof(subPath) - 1] = '\0';
      } else {
        snprintf(subPath, sizeof(subPath), "%s/%s", dirPath, fname);
      }
      file.close();
      if (findGifByIndex(subPath, targetIndex, currentCount, outPath, outSize)) {
        dir.close();
        return true;
      }
    } else {
      if (isGifFile(file.name())) {
        if (*currentCount == targetIndex) {
          const char* fname = file.name();
          if (fname[0] == '/') {
            strncpy(outPath, fname, outSize - 1);
            outPath[outSize - 1] = '\0';
          } else {
            snprintf(outPath, outSize, "%s/%s", dirPath, fname);
          }
          file.close();
          dir.close();
          return true;
        }
        (*currentCount)++;
      }
      file.close();
    }
    file = dir.openNextFile();
  }
  dir.close();
  return false;
}

// Get the path of the Nth .gif file by iterating all subdirectories on-demand.
bool getGifPathByIndex(int index, char *outPath, size_t outSize) {
  int count = 0;
  return findGifByIndex(GIF_DIRECTORY, index, &count, outPath, outSize);
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
bool openGif(const char *path) {
  if (gifPlaying) {
    gif.close();
    gifPlaying = false;
  }

  if (!gif.open(path, GIFOpenFile, GIFCloseFile, GIFReadFile, GIFSeekFile, GIFDraw)) {
    Serial.printf("[GIF] Failed to open: %s\n", path);
    return false;
  }

  Serial.printf("[GIF] Playing: %s (%dx%d)\n", path, gif.getCanvasWidth(), gif.getCanvasHeight());
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
    Serial.printf("[GIF] Could not find GIF at index %d\n", idx);
    return false;
  }

  return openGif(currentGifPath);
}

// Start playing a specific GIF by path (for MQTT service)
bool startSpecificGif(const char *path) {
  if (!sdMounted) {
    Serial.println("[GIF] SD not mounted");
    return false;
  }

  // Verify file exists
  if (!SD.exists(path)) {
    Serial.printf("[GIF] File not found: %s\n", path);
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
      Serial.println("[GIF] Service GIF duration expired");
      return;
    }
  }

  // Start the GIF if not already playing
  if (!gifPlaying) {
    if (!startSpecificGif(serviceGifPath.c_str())) {
      serviceGifActive = false;
      Serial.println("[GIF] Service GIF failed to open, deactivating");
      return;
    }
  }

  // Play one frame; if GIF ends, loop it
  if (!playGifFrame()) {
    // Re-open for looping
    openGif(serviceGifPath.c_str());
  }
}

// ==================== Initialization ====================

void initGifManager() {
  Serial.println("\n========== GIF Manager ==========");
  gif.begin(LITTLE_ENDIAN_PIXELS);
  randomSeed(analogRead(0) + millis());
  countGifFiles();
}

#endif // GIF_MANAGER_H
