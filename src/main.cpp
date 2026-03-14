#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <time.h>
#include <PubSubClient.h>

// Custom headers
#include "config.h"
#include "portal_html.h"
#include "web_server.h"
#include "wifi_manager.h"
#include "display_manager.h"
#include "mqtt_manager.h"
#include "sd_manager.h"
#include "gif_manager.h"

const char* WIFI_SSID = "RetroPixelLED";
const IPAddress softAPIP(192, 168, 4, 1);
const IPAddress gateway(192, 168, 4, 1);
const IPAddress subnet(255, 255, 255, 0);

DNSServer dnsServer;
AsyncWebServer server(80);
Preferences preferences;

// WiFi client for MQTT
WiFiClient espClient;

String ssidWifi = "";
String passwordWifi = "";
String scrollText = "Lorem Ipsum - RetroPixel LED Matrix Display";
bool wifiConnected = false;
bool ntpSynchronized = false;
bool hasWifiCredentials = false;
bool wifiTimeoutOccurred = false;
bool isBootupPhase = true;
uint32_t wifiConnectAttempt = 0;
uint32_t lastNtpCheck = 0;

// Mode system variables
bool modeClockEnabled = true;
bool modeTextEnabled = true;
bool modeGifEnabled = true;
uint16_t modeClockDuration = 10; // seconds to display clock
uint16_t modeTextDuration = 60; // seconds to display text
uint16_t modeGifDuration = 30;  // seconds to display GIFs
uint8_t currentMode = 0; // 0: clock, 1: text, 2: gif
unsigned long lastModeChange = 0;

// Display brightness control (MQTT)
int displayBrightness = BRIGHTNESS;

// Text service variables
String serviceText = "";
uint16_t serviceTextDuration = 0;
bool serviceTextActive = false;
uint32_t serviceTextStartTime = 0;

MatrixPanel_I2S_DMA *dma_display = nullptr;

float colorHue = 0.0;
int textScrollX = SCROLL_START_X;

// Display loading text continuously during boot/NTP sync
void displayLoadingText() {
  dma_display->fillScreen(0);
  dma_display->setTextSize(1);
  
  // Smooth color animation for loading
  uint32_t colorCycle = (millis() / 100) % 6;
  uint16_t color;
  switch (colorCycle) {
    case 0: case 1: color = dma_display->color565(255, 0, 100); break;   // Red-pink
    case 2: case 3: color = dma_display->color565(100, 0, 255); break;   // Blue-purple
    default: color = dma_display->color565(0, 255, 200); break;          // Cyan
  }
  dma_display->setTextColor(color);

  int centerX = max(0, (PANEL_WIDTH * 2 - (7) * 6) / 2);
  dma_display->setCursor(centerX, 12);
  dma_display->print("LOADING");
}

void checkNtpSynchronization() {
  // Only check periodically to avoid overhead
  if (millis() - lastNtpCheck < 5000) return;
  lastNtpCheck = millis();

  if (!wifiConnected) {
    ntpSynchronized = false;
    return;
  }

  time_t now = time(nullptr);
  // NTP is synchronized if time is greater than Jan 1, 2020 (1577836800 seconds)
  // This indicates time has been set from network
  if (now > 1577836800) {
    if (!ntpSynchronized) {
      Serial.println("[NTP] Sincronización completada");
      isBootupPhase = false;  // Exit boot phase when NTP syncs
    }
    ntpSynchronized = true;
  } else {
    ntpSynchronized = false;
  }
}

void updateMode() {
  if (lastModeChange == 0) {
    lastModeChange = millis();
    return;
  }

  // Get duration for current mode
  uint16_t currentDuration;
  switch (currentMode) {
    case 0: currentDuration = modeClockDuration; break;
    case 1: currentDuration = modeTextDuration; break;
    case 2: currentDuration = modeGifDuration; break;
    default: currentDuration = modeTextDuration; break;
  }
  unsigned long elapsed = millis() - lastModeChange;
  
  if (elapsed >= (currentDuration * 1000UL)) {
    lastModeChange = millis();

    // Count enabled modes
    int enabledModes = 0;
    if (modeClockEnabled) enabledModes++;
    if (modeTextEnabled) enabledModes++;
    if (modeGifEnabled && totalGifFiles > 0) enabledModes++;

    if (enabledModes <= 1) return;

    // Switch to next enabled mode
    bool found = false;
    for (int i = 0; i < 3; i++) {
      currentMode = (currentMode + 1) % 3;
      if ((currentMode == 0 && modeClockEnabled) ||
          (currentMode == 1 && modeTextEnabled) ||
          (currentMode == 2 && modeGifEnabled && totalGifFiles > 0)) {
        found = true;
        break;
      }
    }
    if (!found) currentMode = 1; // fallback to text
  }
}

void setupAsyncServer() {
  Serial.println("\n========== Web Server ==========");
  
  // Setup all web server routes from the header
  setupWebServerRoutes();
  
  // Start the server
  server.begin();
  Serial.println("[WEB] Servidor iniciado en puerto 80");
  Serial.println("[WEB] Accede a: http://192.168.4.1");
}

void setupDNS() {
  Serial.println("\n========== DNS Spoofing ==========");
  dnsServer.setErrorReplyCode(DNSReplyCode::ServerFailure);
  bool ok = dnsServer.start(53, "*", softAPIP);
  if (ok) {
    Serial.println("[DNS] Servidor DNS iniciado");
    Serial.println("[DNS] Redirigiendo *.* -> " + softAPIP.toString());
  } else {
    Serial.println("[DNS] ERROR: No se pudo iniciar DNS");
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n\n========== RETRO PIXEL LED ==========");
  Serial.println("[BOOT] Inicializando...");

  Serial.println("[BOOT] Configurando GPIO matriz...");
  HUB75_I2S_CFG mxconfig(PANEL_WIDTH, PANEL_HEIGHT, 2);
  mxconfig.i2sspeed = HUB75_I2S_CFG::HZ_10M;
  mxconfig.latch_blanking = 2;
  mxconfig.min_refresh_rate = 90;
  mxconfig.clkphase = false;

  mxconfig.gpio.r1 = R1_PIN;
  mxconfig.gpio.g1 = G1_PIN;
  mxconfig.gpio.b1 = B1_PIN;
  mxconfig.gpio.r2 = R2_PIN;
  mxconfig.gpio.g2 = G2_PIN;
  mxconfig.gpio.b2 = B2_PIN;
  mxconfig.gpio.a = A_PIN;
  mxconfig.gpio.b = B_PIN;
  mxconfig.gpio.c = C_PIN;
  mxconfig.gpio.d = D_PIN;
  mxconfig.gpio.clk = CLK_PIN;
  mxconfig.gpio.lat = LAT_PIN;
  mxconfig.gpio.oe = OE_PIN;

  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  if (dma_display->begin()) {
    Serial.println("[BOOT] Matriz LED inicializada OK");
  } else {
    Serial.println("[BOOT] ERROR: No se pudo inicializar matriz");
  }

  dma_display->setTextColor(65535);
  dma_display->fillScreen(0);
  dma_display->setBrightness(displayBrightness);

  // Show loading animation
  displayBootAnimation();

  // Initialize SD Card
  delay(100);
  initSDCard();
  listSDFiles();

  // Initialize GIF Manager
  initGifManager();

  initWiFiManager();
  delay(100);
  setupAsyncServer();
  delay(100);
  if (!wifiConnected) {
    setupDNS();
  }
  delay(100);
  initMqtt();

  Serial.println("\n========== SISTEMA LISTO ==========\n");
}

void loop() {
  // Detect WiFi disconnection
  if (wifiConnected && WiFi.status() != WL_CONNECTED) {
    Serial.println("[LOOP] WiFi DESCONECTADO!");
    wifiConnected = false;
    mqttConnected = false;
    ntpSynchronized = false;
    wifiConnectAttempt = millis();  // Start reconnect timer
  }

  if (!wifiConnected) {
    dnsServer.processNextRequest();
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\n[LOOP] WiFi CONECTADO!");
      Serial.println("[LOOP] SSID: " + String(WiFi.SSID()));
      Serial.println("[LOOP] IP: " + WiFi.localIP().toString());
      wifiConnected = true;
      dnsServer.stop();
      Serial.println("[LOOP] DNS detenido");
      syncNTP();  // Non-blocking NTP sync
      // Immediately attempt MQTT connection now that WiFi is up
      connectMqtt();
    } else if (millis() - wifiConnectAttempt > 30000) {
      if (ssidWifi.length() > 0 && passwordWifi.length() > 0) {
        Serial.println("[LOOP] Reintentando WiFi...");
        WiFi.disconnect();
        WiFi.begin(ssidWifi.c_str(), passwordWifi.c_str());
        wifiConnectAttempt = millis();
      }
    }
  }

  // Check NTP synchronization status periodically
  checkNtpSynchronization();

  // Handle WiFi connection timeout (30 seconds)
  // If no WiFi connection after timeout, disable clock mode permanently
  if (!wifiConnected && hasWifiCredentials && !wifiTimeoutOccurred) {
    if (millis() - wifiConnectAttempt > 30000) {
      wifiTimeoutOccurred = true;
      modeClockEnabled = false;  // Disable clock since WiFi/NTP won't sync
      isBootupPhase = false;      // Exit boot phase and show text
      Serial.println("[BOOT] WiFi timeout - deshabilitado modo reloj");
    }
  }

  // Process MQTT
  processMqtt();

  // Update LED brightness only when changed
  static int lastBrightness = -1;
  if (displayBrightness != lastBrightness) {
    dma_display->setBrightness(displayBrightness);
    lastBrightness = displayBrightness;
  }

  // Only clear screen when NOT playing a GIF (GIF draws its own frames)
  bool isGifMode = (serviceGifActive && serviceGifPath.length() > 0) ||
                   (!serviceTextActive && !isBootupPhase && currentMode == 2 && modeGifEnabled && totalGifFiles > 0);
  if (!isGifMode) {
    dma_display->fillScreen(0);
  }

  // Priority 1: Service GIF (MQTT-requested specific GIF)
  if (serviceGifActive && serviceGifPath.length() > 0) {
    displayServiceGif();
  }
  // Priority 2: Service text (MQTT-requested text)
  else if (serviceTextActive && serviceText.length() > 0) {
    displayServiceText();
  } else if (isBootupPhase && hasWifiCredentials) {
    // Show loading while waiting for WiFi/NTP sync
    displayLoadingText();
  } else {
    // If current mode is disabled, immediately switch to an enabled mode
    if ((currentMode == 0 && !modeClockEnabled) ||
        (currentMode == 1 && !modeTextEnabled) ||
        (currentMode == 2 && (!modeGifEnabled || totalGifFiles == 0))) {
      if (modeClockEnabled) {
        currentMode = 0;
      } else if (modeTextEnabled) {
        currentMode = 1;
      } else if (modeGifEnabled && totalGifFiles > 0) {
        currentMode = 2;
      }
      lastModeChange = millis();
    }

    // Count enabled modes for auto-cycling
    int enabledModes = 0;
    if (modeClockEnabled) enabledModes++;
    if (modeTextEnabled) enabledModes++;
    if (modeGifEnabled && totalGifFiles > 0) enabledModes++;

    if (enabledModes > 1) {
      updateMode();
    }

    // Display based on current mode
    if (currentMode == 0 && modeClockEnabled && wifiConnected) {
      displayClock();
    } else if (currentMode == 1 && modeTextEnabled) {
      displayScrollText();
    } else if (currentMode == 2 && modeGifEnabled && totalGifFiles > 0) {
      displayGif();
    } else if (!wifiConnected && modeClockEnabled && currentMode == 0) {
      displayScrollText();
    }
  }

  delay(LOOP_DELAY_MS);
}
