#ifndef SD_MANAGER_H
#define SD_MANAGER_H

#include <SD.h>
#include <SPI.h>
#include "config.h"

// Global SD card mount status
bool sdMounted = false;

// Initialize SD card with custom SPI pins
bool initSDCard() {
  Serial.println("\n========== SD Card Manager ==========");
  
  // Configure SPI with custom pins
  SPI.begin(SD_CLK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
  
  // Attempt to mount SD card
  if (!SD.begin(SD_CS_PIN, SPI, 25000000)) {
    Serial.println("[SD] ERROR: No se pudo montar la tarjeta SD");
    Serial.println("[SD] Verifica la conexión y la tarjeta");
    sdMounted = false;
    return false;
  }
  
  sdMounted = true;
  Serial.println("[SD] Tarjeta SD montada exitosamente");
  
  // Get card type
  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("[SD] ERROR: Tipo de tarjeta desconocido");
    sdMounted = false;
    return false;
  } else if (cardType == CARD_MMC) {
    Serial.println("[SD] Tipo de tarjeta: MMC");
  } else if (cardType == CARD_SD) {
    Serial.println("[SD] Tipo de tarjeta: SDSC");
  } else if (cardType == CARD_SDHC) {
    Serial.println("[SD] Tipo de tarjeta: SDHC");
  }
  
  // Get card size
  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.println("[SD] Tamaño de tarjeta: " + String(cardSize) + " MB");
  
  return true;
}

// List files in a directory
void listSDFiles(const char* dirName = "/") {
  if (!sdMounted) {
    Serial.println("[SD] Tarjeta SD no montada");
    return;
  }
  
  Serial.println("\n[SD] Contenido de " + String(dirName) + ":");
  
  File root = SD.open(dirName);
  if (!root) {
    Serial.println("[SD] ERROR: No se puede abrir el directorio");
    return;
  }
  
  if (!root.isDirectory()) {
    Serial.println("[SD] ERROR: No es un directorio");
    root.close();
    return;
  }
  
  File file = root.openNextFile();
  int fileCount = 0;
  int dirCount = 0;
  
  while (file) {
    if (file.isDirectory()) {
      Serial.println("[SD] [DIR]  " + String(file.name()));
      dirCount++;
    } else {
      uint32_t fileSize = file.size();
      String sizeStr;
      if (fileSize < 1024) {
        sizeStr = String(fileSize) + " B";
      } else if (fileSize < 1024 * 1024) {
        sizeStr = String(fileSize / 1024) + " KB";
      } else {
        sizeStr = String(fileSize / (1024 * 1024)) + " MB";
      }
      Serial.println("[SD] [FILE] " + String(file.name()) + " (" + sizeStr + ")");
      fileCount++;
    }
    file.close();
    file = root.openNextFile();
  }
  
  root.close();
  Serial.println("[SD] Total: " + String(dirCount) + " directorios, " + String(fileCount) + " archivos\n");
}

#endif // SD_MANAGER_H
