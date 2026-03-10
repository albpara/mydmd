#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <time.h>

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

String getPortalHTML() {
  return String(R"=====(<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>RetroPixel LED</title>
  <style>
    * {
      font-family: 'Courier New', Courier, monospace;
      margin: 0;
      padding: 0;
      box-sizing: border-box;
    }

    html, body { height: 100%; }

    body {
      background-color: #a2a2a2;
      color: #000;
      padding: 20px;
      min-height: 100vh;
      display: flex;
      justify-content: center;
      flex-direction: column;
    }

    h1 {
      text-align: center;
      font-size: 28px;
      font-weight: 700;
      margin-bottom: 20px;
      text-shadow: -0.3rem 0.3rem 0 rgba(29, 30, 28, 0.26);
      color: #fff;
    }

    .c {
      max-width: 800px;
      margin: 0 auto;
      background-color: #fff;
      border: 2px solid #000;
      box-shadow: -0.6rem 0.6rem 0 rgba(29, 30, 28, 0.26);
      padding: 0;
    }

    .c > h1 {
      font-size: 18px;
      padding: 12px;
      border-bottom: 2px solid #000;
      margin: 0;
      text-shadow: none;
      color: #000;
    }

    .t {
      display: none;
    }

    fieldset {
      border: 2px solid #000 !important;
      margin: 15px !important;
      padding: 15px !important;
      background-color: #f5f5f5;
    }

    legend {
      font-weight: 700;
      color: #000;
      border: 2px solid #000;
      padding: 4px 8px;
      background-color: #dadad3;
    }

    label {
      display: block;
      font-weight: 700;
      margin: 12px 0 6px 0;
      font-size: 12px;
      color: #000;
    }

    .hdr {
      display: flex;
      justify-content: space-between;
      align-items: center;
      gap: 10px;
      margin-bottom: 10px;
    }

    .hdr label {
      margin: 0;
      flex: 1;
    }

    input, textarea {
      width: 100%;
      border: 2px solid #000;
      background-color: #dadad3;
      padding: 8px;
      font-family: 'Courier New', Courier, monospace;
      font-size: 12px;
      margin-bottom: 10px;
      box-shadow: none !important;
    }

    input:focus, textarea:focus {
      outline: none;
      background-color: #e8e8e1;
      border: 2px solid #000;
    }

    button {
      width: 100%;
      padding: 10px;
      background-color: #dadad3;
      border: 2px solid #000;
      font-weight: 700;
      cursor: pointer;
      margin: 10px 0;
      font-size: 12px;
      box-shadow: none !important;
      transition: all 0.1s;
      font-family: 'Courier New', Courier, monospace;
    }

    button:hover {
      background-color: #c8c8c1;
      border: 2px solid #000;
    }

    button:active {
      transform: translate(-2px, 2px);
      box-shadow: -0.2rem 0.2rem 0 rgba(29, 30, 28, 0.26);
    }

    .sbtn {
      width: auto;
      min-width: 80px;
      margin: 0 0 0 10px;
      padding: 8px 12px;
    }

    .n {
      border: 2px solid #000;
      max-height: 180px;
      overflow-y: auto;
      margin: 10px 0;
      background-color: #f5f5f5;
    }

    .ni {
      padding: 10px;
      cursor: pointer;
      border-bottom: 1px solid #ddd;
      display: flex;
      justify-content: space-between;
      font-size: 12px;
      background-color: #fff;
    }

    .ni:hover {
      background-color: #e8e8e1;
    }

    .ni.s {
      background-color: #d4d4cc;
      border-left: 4px solid #000;
      font-weight: 700;
    }

    .msg {
      display: none;
      padding: 10px;
      margin: 10px 0;
      border: 2px solid #000;
      font-size: 12px;
      background-color: #f5f5f5;
      font-weight: 500;
    }

    .msg.show {
      display: block;
    }

    .msg.ok {
      background-color: #d4edda;
      border-color: #000;
      color: #155724;
    }

    .msg.er {
      background-color: #f8d7da;
      border-color: #000;
      color: #721c24;
    }

    .fs-title {
      font-size: 18px;
      padding: 12px;
      border-bottom: 2px solid #000;
      margin: 0;
      text-shadow: none;
      color: #000;
    }
  </style>
</head>
<body>
  <h1>RETRO PIXEL</h1>
  <div class="c">
    <h1 class="fs-title">Settings</h1>

    <fieldset>
      <legend>WiFi Configuration</legend>
      <label>SSID:</label>
      <input id="ss" placeholder="Network name">
      <label>Password:</label>
      <input type="password" id="pw" placeholder="WiFi password">
      <button onclick="cn()">Connect</button>
      <div id="wm" class="msg"></div>
    </fieldset>

    <fieldset>
      <legend>Display Text</legend>
      <label>Scroll Text (50 max):</label>
      <textarea id="tx" placeholder="Scroll text..."></textarea>
      <button onclick="sv()">Save Text</button>
      <div id="tm" class="msg"></div>
    </fieldset>

    <fieldset>
      <legend>Display Modes</legend>
      <label style="display: flex; align-items: center; margin: 12px 0 6px 0; font-weight: 700; color: #000;"><input type="checkbox" id="mc" style="margin-right: 8px; cursor: pointer;"> Clock</label>
      <label style="display: flex; align-items: center; margin: 12px 0 6px 0; font-weight: 700; color: #000;"><input type="checkbox" id="mt" style="margin-right: 8px; cursor: pointer;"> Text</label>
      <label>Change Interval (seconds):</label>
      <input type="number" id="mi" placeholder="10" min="1" max="300">
      <button onclick="sm()">Save Modes</button>
      <div id="mmsg" class="msg"></div>
    </fieldset>

    <fieldset>
      <legend>System</legend>
      <button onclick="rb()">Reboot Device</button>
      <div id="sm" class="msg"></div>
    </fieldset>
  </div>

  <script>
    function ldWiFi() {
      fetch('/api/wifi-status')
        .then(r => r.json())
        .then(d => {
          if (d.ssid) {
            document.getElementById('ss').value = d.ssid;
            document.getElementById('pw').value = '••••••••';
          }
        })
        .catch(e => console.log(e));
    }

    function cn() {
      var s = document.getElementById('ss').value.trim();
      var p = document.getElementById('pw').value;
      if (!s || !p) {
        alert('Enter SSID and password');
        return;
      }
      var m = document.getElementById('wm');
      m.textContent = 'Connecting...';
      m.className = 'msg show';
      var f = new FormData();
      f.append('ssid', s);
      f.append('password', p);
      fetch('/api/connect-wifi', { method: 'POST', body: f })
        .then(r => r.json())
        .then(d => {
          if (d.success) {
            m.textContent = 'Connected! Redirecting...';
            m.className = 'msg show ok';
            setTimeout(function() { location.reload(); }, 2000);
          } else {
            m.textContent = 'Connection failed';
            m.className = 'msg show er';
          }
        })
        .catch(e => {
          m.textContent = 'Error';
          m.className = 'msg show er';
        });
    }

    function ld() {
      fetch('/api/get-text')
        .then(r => r.json())
        .then(d => {
          if (d.text) document.getElementById('tx').value = d.text;
        })
        .catch(e => console.log(e));
    }

    function sv() {
      var t = document.getElementById('tx').value.trim();
      if (!t) {
        alert('Enter text');
        return;
      }
      var m = document.getElementById('tm');
      m.textContent = 'Saving...';
      m.className = 'msg show';
      var f = new FormData();
      f.append('text', t);
      fetch('/api/update-text', { method: 'POST', body: f })
        .then(r => r.json())
        .then(d => {
          if (d.success) {
            m.textContent = 'Saved!';
            m.className = 'msg show ok';
          } else {
            m.textContent = 'Error';
            m.className = 'msg show er';
          }
        })
        .catch(e => {
          m.textContent = 'Failed';
          m.className = 'msg show er';
        });
    }

    function rb() {
      var m = document.getElementById('sm');
      m.textContent = 'Rebooting...';
      m.className = 'msg show';
      fetch('/api/reboot', { method: 'POST' })
        .then(r => r.json())
        .then(d => {
          m.textContent = 'Device is restarting...';
          m.className = 'msg show ok';
          setTimeout(() => { location.reload(); }, 3000);
        })
        .catch(e => {
          m.textContent = 'Reboot failed';
          m.className = 'msg show er';
        });
    }

    function ldModes() {
      fetch('/api/get-modes')
        .then(r => r.json())
        .then(d => {
          if (d.clockEnabled) document.getElementById('mc').checked = true;
          if (d.textEnabled) document.getElementById('mt').checked = true;
          if (d.interval) document.getElementById('mi').value = d.interval;
        })
        .catch(e => console.log(e));
    }

    function sm() {
      var mc = document.getElementById('mc').checked;
      var mt = document.getElementById('mt').checked;
      var mi = document.getElementById('mi').value || 10;

      if (!mc && !mt) {
        alert('Select at least one mode');
        return;
      }

      var m = document.getElementById('mmsg');
      m.textContent = 'Saving...';
      m.className = 'msg show';
      var f = new FormData();
      f.append('clock', mc ? 1 : 0);
      f.append('text', mt ? 1 : 0);
      f.append('interval', mi);
      fetch('/api/update-modes', { method: 'POST', body: f })
        .then(r => r.json())
        .then(d => {
          if (d.success) {
            m.textContent = 'Saved!';
            m.className = 'msg show ok';
          } else {
            m.textContent = 'Error';
            m.className = 'msg show er';
          }
        })
        .catch(e => {
          m.textContent = 'Failed';
          m.className = 'msg show er';
        });
    }

    window.onload = function() {
      ldWiFi();
      ld();
      ldModes();
    }
  </script>
</body>
</html>)=====" );
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

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.println("[WEB] GET / - Enviando portal HTML (" + String(getPortalHTML().length()) + " bytes)");
    request->send(200, "text/html; charset=utf-8", getPortalHTML());
  });

  server.on("/api/scan-wifi", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.println("[API] GET /api/scan-wifi");
    int numNetworks = WiFi.scanNetworks();
    Serial.println("[API]   Encontradas " + String(numNetworks) + " redes");
    String json;
    json.reserve(JSON_BUFFER_SIZE);
    json = "{\"networks\":[";
    for (int i = 0; i < numNetworks && i < 20; i++) {
      if (i > 0) json += ",";
      json += "{\"ssid\":\"" + String(WiFi.SSID(i)) + "\",\"rssi\":" + String(WiFi.RSSI(i)) + "}";
    }
    json += "]}";
    request->send(200, "application/json", json);
  });

  server.on("/api/connect-wifi", HTTP_POST, [](AsyncWebServerRequest *request) {
    Serial.println("[API] POST /api/connect-wifi");
    String response = "{\"success\":false}";
    if (request->hasParam("ssid", true) && request->hasParam("password", true)) {
      String ssid = request->getParam("ssid", true)->value();
      String password = request->getParam("password", true)->value();

      Serial.println("[API]   SSID: " + ssid);
      Serial.println("[API]   Password: " + String(password.length()) + " caracteres");

      if (ssid.length() > 0 && password.length() > 0) {
        preferences.begin("wifi", false);
        preferences.putString("ssid", ssid);
        preferences.putString("password", password);
        preferences.end();
        Serial.println("[API]   Credenciales guardadas en Preferences");

        ssidWifi = ssid;
        passwordWifi = password;
        if (!wifiConnected) {
          WiFi.mode(WIFI_STA);
          WiFi.disconnect();
        }
        WiFi.begin(ssid.c_str(), password.c_str());
        Serial.println("[API]   Iniciando conexion WiFi...");
        response = "{\"success\":true}";
        wifiConnectAttempt = millis();
      } else {
        Serial.println("[API]   ERROR: Parametros vacios");
      }
    } else {
      Serial.println("[API]   ERROR: Faltan ssid o password");
    }
    request->send(200, "application/json", response);
  });

  server.on("/api/wifi-status", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.println("[API] GET /api/wifi-status");
    String json;
    if (wifiConnected) {
      Serial.println("[API]   Conectado a: " + String(WiFi.SSID()));
      json = "{\"connected\":true,\"ssid\":\"" + String(WiFi.SSID()) + "\",\"ip\":\"" + WiFi.localIP().toString() + "\"}";
    } else {
      Serial.println("[API]   No conectado");
      json = "{\"connected\":false}";
    }
    request->send(200, "application/json", json);
  });

  server.on("/api/get-text", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.println("[API] GET /api/get-text");
    Serial.println("[API]   Texto actual: " + scrollText);
    String json = "{\"text\":\"" + scrollText + "\"}";
    request->send(200, "application/json", json);
  });

  server.on("/api/update-text", HTTP_POST, [](AsyncWebServerRequest *request) {
    Serial.println("[API] POST /api/update-text");
    String response = "{\"success\":false}";
    if (request->hasParam("text", true)) {
      String newText = request->getParam("text", true)->value();

      Serial.println("[API]   Nuevo texto: " + newText);

      if (newText.length() > 0 && newText.length() <= 50) {
        preferences.begin("wifi", false);
        preferences.putString("scrollText", newText);
        preferences.end();
        scrollText = newText;
        textScrollX = 128;
        response = "{\"success\":true}";
        Serial.println("[API]   Texto guardado");
      } else {
        Serial.println("[API]   ERROR: Tamaño invalido (" + String(newText.length()) + " chars)");
      }
    } else {
      Serial.println("[API]   ERROR: Falta parametro 'text'");
    }
    request->send(200, "application/json", response);
  });

  server.on("/api/reboot", HTTP_POST, [](AsyncWebServerRequest *request) {
    Serial.println("[API] POST /api/reboot");
    request->send(200, "application/json", "{\"success\":true,\"message\":\"Rebooting...\"}");
    delay(500);
    ESP.restart();
  });

  server.on("/api/get-modes", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.println("[API] GET /api/get-modes");
    String json;
    json.reserve(100);
    json = "{\"clockEnabled\":" + String(modeClockEnabled ? 1 : 0) +
           ",\"textEnabled\":" + String(modeTextEnabled ? 1 : 0) +
           ",\"interval\":" + String(modeChangeInterval) + "}";
    request->send(200, "application/json", json);
  });

  server.on("/api/update-modes", HTTP_POST, [](AsyncWebServerRequest *request) {
    Serial.println("[API] POST /api/update-modes");
    String response = "{\"success\":false}";
    if (request->hasParam("clock", true) && request->hasParam("text", true) && request->hasParam("interval", true)) {
      bool clockEnabled = request->getParam("clock", true)->value() == "1";
      bool textEnabled = request->getParam("text", true)->value() == "1";
      int interval = atoi(request->getParam("interval", true)->value().c_str());

      if (!clockEnabled && !textEnabled) {
        Serial.println("[API]   ERROR: At least one mode must be enabled");
        request->send(200, "application/json", response);
        return;
      }

      if (interval < 1 || interval > 300) {
        Serial.println("[API]   ERROR: Invalid interval");
        request->send(200, "application/json", response);
        return;
      }

      modeClockEnabled = clockEnabled;
      modeTextEnabled = textEnabled;
      modeChangeInterval = interval;
      lastModeChange = 0;
      currentMode = 0;

      preferences.begin("wifi", false);
      preferences.putBool("modeClock", clockEnabled);
      preferences.putBool("modeText", textEnabled);
      preferences.putInt("modeInterval", interval);
      preferences.end();

      response = "{\"success\":true}";
      Serial.println("[API]   Modos guardados");
    }
    request->send(200, "application/json", response);
  });

  server.onNotFound([](AsyncWebServerRequest *request) {
    Serial.println("[WEB] Request a ruta desconocida: " + request->url());
    request->send(200, "text/html; charset=utf-8", getPortalHTML());
  });

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
