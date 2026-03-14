#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <WiFi.h>
#include <Preferences.h>
#include <time.h>
#include "config.h"

// Forward declarations for global variables
extern String ssidWifi;
extern String passwordWifi;
extern String scrollText;
extern bool wifiConnected;
extern bool hasWifiCredentials;
extern bool modeClockEnabled;
extern bool modeTextEnabled;
extern bool modeGifEnabled;
extern uint16_t modeClockDuration;
extern uint16_t modeTextDuration;
extern uint16_t modeGifDuration;
extern uint32_t wifiConnectAttempt;
extern Preferences preferences;
extern DNSServer dnsServer;

// WiFi configuration constants
extern const char* WIFI_SSID;
extern const IPAddress softAPIP;
extern const IPAddress gateway;
extern const IPAddress subnet;

// Non-blocking NTP time synchronization
void syncNTP() {
  if (!wifiConnected) return;

  Serial.println("[NTP] Iniciando sincronización de hora (no bloqueante)...");
  // configTime starts NTP sync in background without blocking
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  // The ESP32 will complete NTP sync asynchronously
  // Time will be available once sync completes, no blocking needed
  Serial.println("[NTP] NTP sincronización en progreso (sin bloquear)");
}

// Initialize WiFi manager, either connect to saved network or start AP
void initWiFiManager() {
  Serial.println("\n========== WiFi Manager ==========");

  // Load WiFi credentials and display settings from preferences
  preferences.begin("wifi", false);
  ssidWifi = preferences.getString("ssid", "");
  passwordWifi = preferences.getString("password", "");
  scrollText = preferences.getString("scrollText", "MAXIMO Y VICTOR");
  modeClockEnabled = preferences.getBool("modeClock", true);
  modeTextEnabled = preferences.getBool("modeText", true);
  modeGifEnabled = preferences.getBool("modeGif", true);
  modeClockDuration = preferences.getInt("clockDur", 10);
  modeTextDuration = preferences.getInt("textDur", 60);
  modeGifDuration = preferences.getInt("gifDur", 30);
  preferences.end();

  Serial.println("[WIFI] Cargadas credenciales de Preferences");
  if (ssidWifi.length() > 0) {
    Serial.println("[WIFI] SSID guardado: " + ssidWifi);
    hasWifiCredentials = true;
  } else {
    Serial.println("[WIFI] Sin credenciales guardadas - deshabilitado modo reloj");
    hasWifiCredentials = false;
    modeClockEnabled = false;  // No clock without WiFi/NTP
  }

  // Start WiFi connection if credentials exist (non-blocking)
  if (ssidWifi.length() > 0 && passwordWifi.length() > 0) {
    Serial.println("[WIFI] Intentando conectar a: " + ssidWifi);
    WiFi.mode(WIFI_AP_STA);  // Both AP and STA for fallback config portal
    WiFi.begin(ssidWifi.c_str(), passwordWifi.c_str());
    wifiConnectAttempt = millis();
    Serial.println("[WIFI] Conexion en progreso... (se completará en loop)");
  } else {
    WiFi.mode(WIFI_AP);
  }

  Serial.println("[WIFI] Iniciando Soft AP...");
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

#endif // WIFI_MANAGER_H
