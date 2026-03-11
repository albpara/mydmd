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
extern bool modeClockEnabled;
extern bool modeTextEnabled;
extern uint16_t modeChangeInterval;
extern uint32_t wifiConnectAttempt;
extern Preferences preferences;
extern DNSServer dnsServer;

// WiFi configuration constants
extern const char* WIFI_SSID;
extern const IPAddress softAPIP;
extern const IPAddress gateway;
extern const IPAddress subnet;

// Synchronize time with NTP server
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
  modeChangeInterval = preferences.getInt("modeInterval", 10);
  preferences.end();

  Serial.println("[WIFI] Cargadas credenciales de Preferences");
  if (ssidWifi.length() > 0) {
    Serial.println("[WIFI] SSID guardado: " + ssidWifi);
  } else {
    Serial.println("[WIFI] Sin credenciales guardadas");
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
