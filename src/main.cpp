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
uint32_t wifiConnectAttempt = 0;

// Mode system variables
bool modeClockEnabled = true;
bool modeTextEnabled = true;
uint16_t modeClockDuration = 10; // seconds to display clock
uint16_t modeTextDuration = 60; // seconds to display text
uint8_t currentMode = 0; // 0: clock, 1: text
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

void updateMode() {
  if (lastModeChange == 0) {
    lastModeChange = millis();
    return;
  }

  // Get duration for current mode
  uint16_t currentDuration = (currentMode == 0) ? modeClockDuration : modeTextDuration;
  unsigned long elapsed = millis() - lastModeChange;
  
  if (elapsed >= (currentDuration * 1000UL)) {
    lastModeChange = millis();

    // Count enabled modes
    int enabledModes = 0;
    if (modeClockEnabled) enabledModes++;
    if (modeTextEnabled) enabledModes++;

    if (enabledModes <= 1) return;

    // Switch to next enabled mode
    bool found = false;
    for (int i = 0; i < 2; i++) {
      currentMode = (currentMode + 1) % 2;
      if ((currentMode == 0 && modeClockEnabled) || (currentMode == 1 && modeTextEnabled)) {
        found = true;
        break;
      }
    }
    if (!found) currentMode = 0;
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
  if (!wifiConnected) {
    dnsServer.processNextRequest();
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\n[LOOP] WiFi CONECTADO!");
      Serial.println("[LOOP] SSID: " + String(WiFi.SSID()));
      Serial.println("[LOOP] IP: " + WiFi.localIP().toString());
      wifiConnected = true;
      dnsServer.stop();
      Serial.println("[LOOP] DNS detenido");
      syncNTP();
      // Immediately attempt MQTT connection now that WiFi is up
      connectMqtt();
    } else if (millis() - wifiConnectAttempt > 30000) {
      if (ssidWifi.length() > 0 && passwordWifi.length() > 0) {
        Serial.println("[LOOP] Reintentando WiFi...");
        WiFi.mode(WIFI_STA);
        WiFi.begin(ssidWifi.c_str(), passwordWifi.c_str());
        wifiConnectAttempt = millis();
      }
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

  dma_display->fillScreen(0);

  // Display service text if active (takes priority)
  if (serviceTextActive && serviceText.length() > 0) {
    displayServiceText();
  } else {
    // Update mode logic if multiple modes enabled
    if (modeClockEnabled && modeTextEnabled) {
      updateMode();
    }

    // Display based on current mode
    if (currentMode == 0 && modeClockEnabled && wifiConnected) {
      displayClock();
    } else if (currentMode == 1 && modeTextEnabled) {
      displayScrollText();
    } else if (!wifiConnected && modeClockEnabled && currentMode == 0) {
      // Show text if clock is selected but no WiFi
      displayScrollText();
    }
  }

  delay(LOOP_DELAY_MS);
}
