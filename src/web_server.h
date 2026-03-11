#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <ESPAsyncWebServer.h>
#include "portal_html.h"

// Forward declarations for global variables
extern AsyncWebServer server;
extern String scrollText;
extern bool wifiConnected;
extern String ssidWifi;
extern String passwordWifi;
extern bool modeClockEnabled;
extern bool modeTextEnabled;
extern uint16_t modeChangeInterval;
extern uint16_t modeClockDuration;
extern uint16_t modeTextDuration;
extern uint32_t wifiConnectAttempt;
extern Preferences preferences;
extern int textScrollX;
extern bool mqttConnected;
extern String mqttBroker;
extern uint16_t mqttPort;
extern String mqttUsername;
extern String mqttPassword;
extern String mqttClientName;
extern String mqttTopicPrefix;
extern void updateMqttSettings(String broker, uint16_t port, String username, String password, String clientname, String prefix);

// Setup all web server routes
void setupWebServerRoutes() {
  // Root - serve HTML portal
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.println("[WEB] GET / - Enviando portal HTML (" + String(getPortalHTML().length()) + " bytes)");
    request->send(200, "text/html; charset=utf-8", getPortalHTML());
  });

  // WiFi scanning endpoint
  server.on("/api/scan-wifi", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.println("[API] GET /api/scan-wifi");
    int numNetworks = WiFi.scanNetworks();
    Serial.println("[API]   Encontradas " + String(numNetworks) + " redes");
    String json;
    json.reserve(256);
    json = "{\"networks\":[";
    for (int i = 0; i < numNetworks && i < 20; i++) {
      if (i > 0) json += ",";
      json += "{\"ssid\":\"" + String(WiFi.SSID(i)) + "\",\"rssi\":" + String(WiFi.RSSI(i)) + "}";
    }
    json += "]}";
    request->send(200, "application/json", json);
  });

  // WiFi connection endpoint
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

  // WiFi status endpoint
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

  // Get scroll text endpoint
  server.on("/api/get-text", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.println("[API] GET /api/get-text");
    Serial.println("[API]   Texto actual: " + scrollText);
    String json = "{\"text\":\"" + scrollText + "\"}";
    request->send(200, "application/json", json);
  });

  // Update scroll text endpoint
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

  // Reboot endpoint
  server.on("/api/reboot", HTTP_POST, [](AsyncWebServerRequest *request) {
    Serial.println("[API] POST /api/reboot");
    request->send(200, "application/json", "{\"success\":true,\"message\":\"Rebooting...\"}");
    delay(500);
    ESP.restart();
  });

  // Get display modes endpoint
  server.on("/api/get-modes", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.println("[API] GET /api/get-modes");
    String json;
    json.reserve(150);
    json = "{\"clockEnabled\":" + String(modeClockEnabled ? 1 : 0) +
           ",\"textEnabled\":" + String(modeTextEnabled ? 1 : 0) +
           ",\"interval\":" + String(modeChangeInterval) +
           ",\"clockDuration\":" + String(modeClockDuration) +
           ",\"textDuration\":" + String(modeTextDuration) + "}";
    request->send(200, "application/json", json);
  });

  // Update display modes endpoint
  server.on("/api/update-modes", HTTP_POST, [](AsyncWebServerRequest *request) {
    Serial.println("[API] POST /api/update-modes");
    String response = "{\"success\":false}";
    if (request->hasParam("clock", true) && request->hasParam("text", true)) {
      bool clockEnabled = request->getParam("clock", true)->value() == "1";
      bool textEnabled = request->getParam("text", true)->value() == "1";
      
      // Get individual durations (optional, with fallbacks)
      int clockDur = request->hasParam("clockDuration", true) ? 
        atoi(request->getParam("clockDuration", true)->value().c_str()) : modeClockDuration;
      int textDur = request->hasParam("textDuration", true) ? 
        atoi(request->getParam("textDuration", true)->value().c_str()) : modeTextDuration;
      
      // Keep interval for compatibility
      int interval = request->hasParam("interval", true) ? 
        atoi(request->getParam("interval", true)->value().c_str()) : modeChangeInterval;

      if (!clockEnabled && !textEnabled) {
        Serial.println("[API]   ERROR: At least one mode must be enabled");
        request->send(200, "application/json", response);
        return;
      }

      if (clockDur < 1 || clockDur > 300 || textDur < 1 || textDur > 300) {
        Serial.println("[API]   ERROR: Invalid duration (1-300 seconds)");
        request->send(200, "application/json", response);
        return;
      }

      modeClockEnabled = clockEnabled;
      modeTextEnabled = textEnabled;
      modeChangeInterval = interval;
      modeClockDuration = clockDur;
      modeTextDuration = textDur;

      preferences.begin("wifi", false);
      preferences.putBool("modeClock", clockEnabled);
      preferences.putBool("modeText", textEnabled);
      preferences.putInt("modeInterval", interval);
      preferences.putInt("clockDur", clockDur);
      preferences.putInt("textDur", textDur);
      preferences.end();

      Serial.println("[API]   Clock: " + String(clockDur) + "s, Text: " + String(textDur) + "s");
      response = "{\"success\":true}";
      Serial.println("[API]   Modos guardados");
    }
    request->send(200, "application/json", response);
  });

  // Get MQTT configuration endpoint
  server.on("/api/mqtt-config", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.println("[API] GET /api/mqtt-config");
    String json;
    json.reserve(256);
    json = "{\"broker\":\"" + mqttBroker + "\",";
    json += "\"port\":" + String(mqttPort) + ",";
    json += "\"username\":\"" + mqttUsername + "\",";
    json += "\"password\":\"" + mqttPassword + "\",";
    json += "\"clientname\":\"" + mqttClientName + "\",";
    json += "\"prefix\":\"" + mqttTopicPrefix + "\",";
    json += "\"connected\":" + String(mqttConnected ? 1 : 0) + "}";
    request->send(200, "application/json", json);
  });

  // Configure MQTT endpoint
  server.on("/api/configure-mqtt", HTTP_POST, [](AsyncWebServerRequest *request) {
    Serial.println("[API] POST /api/configure-mqtt");
    String response = "{\"success\":false}";

    if (request->hasParam("broker", true) && request->hasParam("port", true)) {
      String broker = request->getParam("broker", true)->value();
      String portStr = request->getParam("port", true)->value();
      String username = request->hasParam("username", true) ? request->getParam("username", true)->value() : "";
      String password = request->hasParam("password", true) ? request->getParam("password", true)->value() : "";
      String clientname = request->hasParam("clientname", true) ? request->getParam("clientname", true)->value() : "RetroPixelLED";
      String prefix = request->hasParam("prefix", true) ? request->getParam("prefix", true)->value() : "retropixel";

      uint16_t port = atoi(portStr.c_str());

      if (broker.length() > 0 && port > 0 && port <= 65535 && clientname.length() > 0 && prefix.length() > 0) {
        Serial.println("[API]   Broker: " + broker);
        Serial.println("[API]   Port: " + String(port));
        Serial.println("[API]   Client Name: " + clientname);
        Serial.println("[API]   Username: " + String(username.length() > 0 ? "***" : "(none)"));
        Serial.println("[API]   Prefix: " + prefix);

        updateMqttSettings(broker, port, username, password, clientname, prefix);
        response = "{\"success\":true}";
      } else {
        Serial.println("[API]   ERROR: Invalid parameters");
      }
    } else {
      Serial.println("[API]   ERROR: Missing broker or port");
    }
    request->send(200, "application/json", response);
  });

  // Catch all - serve HTML portal
  server.onNotFound([](AsyncWebServerRequest *request) {
    Serial.println("[WEB] Request a ruta desconocida: " + request->url());
    request->send(200, "text/html; charset=utf-8", getPortalHTML());
  });
}

#endif // WEB_SERVER_H
