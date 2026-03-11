#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <time.h>

// Custom headers
#include "portal_html.h"
#include "web_server.h"

const char* WIFI_SSID = "RetroPixelLED";
const char* WIFI_PASSWORD = "";
const IPAddress softAPIP(192, 168, 4, 1);
const IPAddress gateway(192, 168, 4, 1);
const IPAddress subnet(255, 255, 255, 0);

DNSServer dnsServer;
AsyncWebServer server(80);
Preferences preferences;

String ssidWifi = "";
String passwordWifi = "";
String scrollText = "Lorem Ipsum - RetroPixel LED Matrix Display";
bool wifiConnected = false;
uint32_t wifiConnectAttempt = 0;

// Mode system variables
bool modeClockEnabled = true;
bool modeTextEnabled = true;
uint16_t modeChangeInterval = 10; // seconds
uint8_t currentMode = 0; // 0: clock, 1: text
unsigned long lastModeChange = 0;

#define R1_PIN 25
#define G1_PIN 26
#define B1_PIN 27
#define R2_PIN 14
#define G2_PIN 12
#define B2_PIN 13
#define A_PIN 33
#define B_PIN 32
#define C_PIN 22
#define D_PIN 17
#define CLK_PIN 16
#define LAT_PIN 4
#define OE_PIN 15

#define PANEL_WIDTH 64
#define PANEL_HEIGHT 32

#define BRIGHTNESS 16
#define SCROLL_START_X 128
#define DISPLAY_Y_OFFSET 8
#define TEXT_SIZE 2
#define SCROLL_SPEED 1
#define LOOP_DELAY_MS 50
#define JSON_BUFFER_SIZE 256
#define MAGENTA_COLOR dma_display->color565(255, 100, 255)

MatrixPanel_I2S_DMA *dma_display = nullptr;
float colorHue = 0.0;
int textScrollX = SCROLL_START_X;

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

int calculateTextWidth(const String& text) {
  return text.length() * (6 * TEXT_SIZE);
}

void displayScrollText() {
  dma_display->setTextSize(TEXT_SIZE);
  dma_display->setTextWrap(false);
  uint16_t currentColor = hsvToRGB(colorHue);
  dma_display->setTextColor(currentColor);

  int textWidth = calculateTextWidth(scrollText);
  dma_display->setCursor(textScrollX, DISPLAY_Y_OFFSET);
  dma_display->print(scrollText.c_str());

  textScrollX -= SCROLL_SPEED;
  if (textScrollX < -textWidth) textScrollX = SCROLL_START_X;

  colorHue += 1.0;
  if (colorHue >= 360.0) colorHue = 0.0;
}

void syncNTP() {
  if (!wifiConnected) return;

  Serial.println("[NTP] Sincronizando hora...");
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

  time_t now = time(nullptr);
  int attempts = 0;
  while (now < 24 * 3600 && attempts < 20) {
    delay(500);
    now = time(nullptr);
    attempts++;
  }

  if (now > 24 * 3600) {
    Serial.println("[NTP] Hora sincronizada");
  } else {
    Serial.println("[NTP] Error al sincronizar");
  }
}

void displayClock() {
  if (!wifiConnected) return;

  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);
  char timeStr[9];
  strftime(timeStr, sizeof(timeStr), "%H:%M:%S", timeinfo);

  dma_display->setTextSize(TEXT_SIZE);
  dma_display->setTextColor(MAGENTA_COLOR);

  // 10 chars * 12px + some padding, centered on 128px width
  int centerX = max(0, (PANEL_WIDTH * 2 - (10 + 2) * 8) / 2);

  dma_display->setCursor(centerX, DISPLAY_Y_OFFSET);
  dma_display->print(timeStr);
}

void updateMode() {
  if (lastModeChange == 0) {
    lastModeChange = millis();
    return;
  }

  unsigned long elapsed = millis() - lastModeChange;
  if (elapsed >= (modeChangeInterval * 1000UL)) {
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

void initWiFiManager() {
  Serial.println("\n========== WiFi Manager ==========");

  preferences.begin("wifi", false);
  ssidWifi = preferences.getString("ssid", "");
  passwordWifi = preferences.getString("password", "");
  scrollText = preferences.getString("scrollText", "MAXIMO Y VICTOR");
  modeClockEnabled = preferences.getBool("modeClock", true);
  modeTextEnabled = preferences.getBool("modeText", true);
  modeChangeInterval = preferences.getInt("modeInterval", 10);
  preferences.end();

  Serial.println("[WIFI] Cargadas credenciales de Preferences");
  if (ssidWifi.length() > 0) {
    Serial.println("[WIFI] SSID guardado: " + ssidWifi);
  } else {
    Serial.println("[WIFI] Sin credenciales guardadas");
  }

  if (ssidWifi.length() > 0 && passwordWifi.length() > 0) {
    Serial.println("[WIFI] Intentando conectar a: " + ssidWifi);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssidWifi.c_str(), passwordWifi.c_str());

    wifiConnectAttempt = millis();
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(500);
      Serial.print(".");
      attempts++;
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
      wifiConnected = true;
      Serial.println("[WIFI] CONECTADO: " + String(WiFi.SSID()));
      Serial.println("[WIFI] IP: " + WiFi.localIP().toString());
      syncNTP();
      return;
    } else {
      Serial.println("[WIFI] Conexion fallida, entrando en Soft AP");
    }
  }

  Serial.println("[WIFI] Iniciando Soft AP...");
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(softAPIP, gateway, subnet);
  bool ok = WiFi.softAP(WIFI_SSID);

  if (ok) {
    Serial.println("[WIFI] Soft AP activo");
    Serial.println("[WIFI] SSID: " + String(WIFI_SSID));
    Serial.println("[WIFI] IP: " + WiFi.softAPIP().toString());
  } else {
    Serial.println("[WIFI] ERROR: No se pudo crear Soft AP");
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
  dma_display->setBrightness(BRIGHTNESS);

  initWiFiManager();
  delay(100);
  setupAsyncServer();
  delay(100);
  if (wifiConnected) {
    syncNTP();
  } else {
    setupDNS();
  }

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
    } else if (millis() - wifiConnectAttempt > 30000) {
      if (ssidWifi.length() > 0 && passwordWifi.length() > 0) {
        Serial.println("[LOOP] Reintentando WiFi...");
        WiFi.mode(WIFI_STA);
        WiFi.begin(ssidWifi.c_str(), passwordWifi.c_str());
        wifiConnectAttempt = millis();
      }
    }
  }

  dma_display->fillScreen(0);

  // Update mode logic if multiple modes enabled
  if ((modeClockEnabled && modeTextEnabled) && modeChangeInterval > 0) {
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

  delay(LOOP_DELAY_MS);
}
