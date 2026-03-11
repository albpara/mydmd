#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <PubSubClient.h>
#include <WiFi.h>
#include <Preferences.h>
#include "config.h"

// Forward declarations for global variables
extern WiFiClient espClient;
extern String scrollText;
extern bool wifiConnected;
extern int displayBrightness;
extern MatrixPanel_I2S_DMA *dma_display;
extern Preferences preferences;
extern bool modeClockEnabled;
extern bool modeTextEnabled;
extern uint16_t modeClockDuration;
extern uint16_t modeTextDuration;
extern String serviceText;
extern uint16_t serviceTextDuration;
extern bool serviceTextActive;
extern uint32_t serviceTextStartTime;

// MQTT variables
PubSubClient *mqttClient = nullptr;
String mqttBroker = "192.168.1.100";
uint16_t mqttPort = 1883;
String mqttUsername = "";
String mqttPassword = "";
String mqttClientName = "RetroPixelLED";
String mqttTopicPrefix = "retropixel";
bool mqttConnected = false;
uint32_t lastMqttReconnect = 0;
const uint32_t MQTT_RECONNECT_INTERVAL = 5000; // 5 seconds
uint32_t lastMqttStatePublish = 0;
const uint32_t MQTT_STATE_PUBLISH_INTERVAL = 30000; // 30 seconds
uint32_t lastMqttDiagPublish = 0;
const uint32_t MQTT_DIAG_PUBLISH_INTERVAL = 60000; // 60 seconds

// Device identifiers for Home Assistant
String deviceId = "";
bool pendingReboot = false;

// Forward declare functions
void initializeDeviceId();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void publishHomeAssistantDiscovery();
void publishDisplayStatus();
void publishModeStates();
void publishDiagnostics();
void connectMqtt();

// Initialize device ID from MAC address
void initializeDeviceId() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char id[13];
  snprintf(id, sizeof(id), "%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  deviceId = String(id);
}

// MQTT callback handler for incoming messages
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  // Null-terminate payload in stack buffer
  char msg[256];
  unsigned int copyLen = (length < sizeof(msg) - 1) ? length : sizeof(msg) - 1;
  memcpy(msg, payload, copyLen);
  msg[copyLen] = '\0';

  Serial.printf("[MQTT] %s: %s\n", topic, msg);

  // Control topic: power (on/off)
  if (strstr(topic, "/power/set")) {
    if (strcasecmp(msg, "ON") == 0) {
      displayBrightness = 16;
    } else {
      displayBrightness = 0;
      if (dma_display) dma_display->fillScreen(0);
    }
    publishDisplayStatus();
  }
  // Mode control: clock
  else if (strstr(topic, "/modes/clock/set")) {
    modeClockEnabled = (strcasecmp(msg, "ON") == 0);
    preferences.begin("wifi", false);
    preferences.putBool("modeClock", modeClockEnabled);
    preferences.end();
  }
  // Mode control: text
  else if (strstr(topic, "/modes/text/set")) {
    modeTextEnabled = (strcasecmp(msg, "ON") == 0);
    preferences.begin("wifi", false);
    preferences.putBool("modeText", modeTextEnabled);
    preferences.end();
  }
  // Duration control: clock
  else if (strstr(topic, "/modes/clockDuration/set")) {
    int dur = atoi(msg);
    if (dur >= 1 && dur <= 300) {
      modeClockDuration = dur;
      preferences.begin("wifi", false);
      preferences.putInt("clockDur", dur);
      preferences.end();
    }
  }
  // Duration control: text
  else if (strstr(topic, "/modes/textDuration/set")) {
    int dur = atoi(msg);
    if (dur >= 1 && dur <= 300) {
      modeTextDuration = dur;
      preferences.begin("wifi", false);
      preferences.putInt("textDur", dur);
      preferences.end();
    }
  }
  // Text service: JSON {"text": "message", "duration": seconds}
  else if (strstr(topic, "/text/set")) {
    // Simple C-string JSON parsing
    char* textStart = strstr(msg, "\"text\"");
    char* durStart = strstr(msg, "\"duration\"");
    
    if (textStart) {
      char* colon = strchr(textStart, ':');
      if (colon) {
        char* q1 = strchr(colon, '"');
        if (q1) {
          q1++;
          char* q2 = strchr(q1, '"');
          if (q2) {
            *q2 = '\0';
            serviceText = String(q1);
            *q2 = '"'; // restore
          }
        }
      }
    }
    
    if (durStart) {
      char* colon = strchr(durStart, ':');
      if (colon) {
        uint16_t dur = atoi(colon + 1);
        if (dur >= 1 && dur <= 3600) {
          serviceTextDuration = dur;
        }
      }
    }
    
    if (serviceText.length() > 0 && serviceTextDuration > 0) {
      serviceTextActive = true;
      serviceTextStartTime = millis();
    }
  }
  // Reboot command
  else if (strstr(topic, "/system/reboot")) {
    pendingReboot = true;
  }
}

// Publish current display status
void publishDisplayStatus() {
  if (!mqttClient || !mqttClient->connected()) return;
  char topic[60];
  snprintf(topic, sizeof(topic), "%s/display/state", mqttTopicPrefix.c_str());
  mqttClient->publish(topic, displayBrightness > 0 ? "ON" : "OFF", true);
}

// Publish current mode states and durations
void publishModeStates() {
  if (!mqttClient || !mqttClient->connected()) return;
  char topic[60];
  char val[8];
  const char* pfx = mqttTopicPrefix.c_str();

  snprintf(topic, sizeof(topic), "%s/modes/clock/state", pfx);
  mqttClient->publish(topic, modeClockEnabled ? "ON" : "OFF", true);
  
  snprintf(topic, sizeof(topic), "%s/modes/text/state", pfx);
  mqttClient->publish(topic, modeTextEnabled ? "ON" : "OFF", true);
  
  snprintf(topic, sizeof(topic), "%s/modes/clockDuration/state", pfx);
  snprintf(val, sizeof(val), "%u", modeClockDuration);
  mqttClient->publish(topic, val, true);
  
  snprintf(topic, sizeof(topic), "%s/modes/textDuration/state", pfx);
  snprintf(val, sizeof(val), "%u", modeTextDuration);
  mqttClient->publish(topic, val, true);
}

// Publish diagnostic information
void publishDiagnostics() {
  if (!mqttClient || !mqttClient->connected()) return;
  char topic[60];
  char val[20];
  const char* pfx = mqttTopicPrefix.c_str();

  snprintf(topic, sizeof(topic), "%s/system/ip/state", pfx);
  mqttClient->publish(topic, WiFi.localIP().toString().c_str(), true);
  
  snprintf(topic, sizeof(topic), "%s/system/uptime/state", pfx);
  snprintf(val, sizeof(val), "%lu", millis() / 1000);
  mqttClient->publish(topic, val, true);
  
  snprintf(topic, sizeof(topic), "%s/system/link_quality/state", pfx);
  snprintf(val, sizeof(val), "%d", WiFi.RSSI());
  mqttClient->publish(topic, val, true);
}

// Publish Home Assistant MQTT Discovery messages
void publishHomeAssistantDiscovery() {
  if (!mqttClient || !mqttClient->connected()) return;

  Serial.println("[MQTT] Publishing HA Discovery");

  const char* did = deviceId.c_str();
  const char* pfx = mqttTopicPrefix.c_str();

  // Reusable stack buffers
  char topic[80];
  char payload[512];

  // Device info snippet (using HA abbreviated keys)
  char dev[180];
  snprintf(dev, sizeof(dev),
    "\"dev\":{\"ids\":[\"rp_%s\"],"
    "\"mf\":\"RetroPixel\","
    "\"mdl\":\"LED Matrix ESP32\","
    "\"name\":\"RetroPixel LED\","
    "\"sw\":\"1.0\"}", did);

  // 1. Display power (Light entity)
  snprintf(topic, sizeof(topic), "homeassistant/light/%s/display/config", did);
  snprintf(payload, sizeof(payload),
    "{\"name\":\"Display\",\"uniq_id\":\"rp_%s_disp\","
    "\"stat_t\":\"%s/display/state\","
    "\"cmd_t\":\"%s/display/power/set\","
    "\"pl_on\":\"ON\",\"pl_off\":\"OFF\",%s}", did, pfx, pfx, dev);
  mqttClient->publish(topic, payload, true);

  // 2. Clock Mode (Switch)
  snprintf(topic, sizeof(topic), "homeassistant/switch/%s/clock_mode/config", did);
  snprintf(payload, sizeof(payload),
    "{\"name\":\"Clock Mode\",\"uniq_id\":\"rp_%s_clk\","
    "\"stat_t\":\"%s/modes/clock/state\","
    "\"cmd_t\":\"%s/modes/clock/set\","
    "\"pl_on\":\"ON\",\"pl_off\":\"OFF\",%s}", did, pfx, pfx, dev);
  mqttClient->publish(topic, payload, true);

  // 3. Text Mode (Switch)
  snprintf(topic, sizeof(topic), "homeassistant/switch/%s/text_mode/config", did);
  snprintf(payload, sizeof(payload),
    "{\"name\":\"Text Mode\",\"uniq_id\":\"rp_%s_txt\","
    "\"stat_t\":\"%s/modes/text/state\","
    "\"cmd_t\":\"%s/modes/text/set\","
    "\"pl_on\":\"ON\",\"pl_off\":\"OFF\",%s}", did, pfx, pfx, dev);
  mqttClient->publish(topic, payload, true);

  // 4. Clock Duration (Number entity - Config)
  snprintf(topic, sizeof(topic), "homeassistant/number/%s/clock_dur/config", did);
  snprintf(payload, sizeof(payload),
    "{\"name\":\"Clock Duration\",\"uniq_id\":\"rp_%s_clkdur\","
    "\"stat_t\":\"%s/modes/clockDuration/state\","
    "\"cmd_t\":\"%s/modes/clockDuration/set\","
    "\"unit_of_meas\":\"s\",\"ent_cat\":\"config\","
    "\"min\":1,\"max\":300,\"step\":1,%s}", did, pfx, pfx, dev);
  mqttClient->publish(topic, payload, true);

  // 5. Text Duration (Number entity - Config)
  snprintf(topic, sizeof(topic), "homeassistant/number/%s/text_dur/config", did);
  snprintf(payload, sizeof(payload),
    "{\"name\":\"Text Duration\",\"uniq_id\":\"rp_%s_txtdur\","
    "\"stat_t\":\"%s/modes/textDuration/state\","
    "\"cmd_t\":\"%s/modes/textDuration/set\","
    "\"unit_of_meas\":\"s\",\"ent_cat\":\"config\","
    "\"min\":1,\"max\":300,\"step\":1,%s}", did, pfx, pfx, dev);
  mqttClient->publish(topic, payload, true);

  // 6. IP Address (Diagnostic sensor)
  snprintf(topic, sizeof(topic), "homeassistant/sensor/%s/ip/config", did);
  snprintf(payload, sizeof(payload),
    "{\"name\":\"IP Address\",\"uniq_id\":\"rp_%s_ip\","
    "\"stat_t\":\"%s/system/ip/state\","
    "\"ent_cat\":\"diagnostic\",\"ic\":\"mdi:ip\",%s}", did, pfx, dev);
  mqttClient->publish(topic, payload, true);

  // 7. Uptime (Diagnostic sensor)
  snprintf(topic, sizeof(topic), "homeassistant/sensor/%s/uptime/config", did);
  snprintf(payload, sizeof(payload),
    "{\"name\":\"Uptime\",\"uniq_id\":\"rp_%s_up\","
    "\"stat_t\":\"%s/system/uptime/state\","
    "\"unit_of_meas\":\"s\",\"ent_cat\":\"diagnostic\","
    "\"ic\":\"mdi:clock-outline\",%s}", did, pfx, dev);
  mqttClient->publish(topic, payload, true);

  // 8. Link Quality (Diagnostic sensor)
  snprintf(topic, sizeof(topic), "homeassistant/sensor/%s/rssi/config", did);
  snprintf(payload, sizeof(payload),
    "{\"name\":\"Link Quality\",\"uniq_id\":\"rp_%s_rssi\","
    "\"stat_t\":\"%s/system/link_quality/state\","
    "\"unit_of_meas\":\"dBm\",\"ent_cat\":\"diagnostic\","
    "\"ic\":\"mdi:signal\",%s}", did, pfx, dev);
  mqttClient->publish(topic, payload, true);

  // 9. Reboot Button
  snprintf(topic, sizeof(topic), "homeassistant/button/%s/reboot/config", did);
  snprintf(payload, sizeof(payload),
    "{\"name\":\"Reboot\",\"uniq_id\":\"rp_%s_reboot\","
    "\"cmd_t\":\"%s/system/reboot\","
    "\"pl_press\":\"true\",\"ent_cat\":\"config\","
    "\"ic\":\"mdi:restart\",%s}", did, pfx, dev);
  mqttClient->publish(topic, payload, true);

  // 10. Text Service
  snprintf(topic, sizeof(topic), "homeassistant/text/%s/text_svc/config", did);
  snprintf(payload, sizeof(payload),
    "{\"name\":\"Display Text\",\"uniq_id\":\"rp_%s_textsvc\","
    "\"cmd_t\":\"%s/text/set\","
    "\"ent_cat\":\"config\",\"ic\":\"mdi:message\",%s}", did, pfx, dev);
  mqttClient->publish(topic, payload, true);

  Serial.println("[MQTT] HA Discovery complete (10 entities)");

  // Publish initial states
  publishDisplayStatus();
  publishModeStates();
  publishDiagnostics();
}

// Connect to MQTT broker
void connectMqtt() {
  if (!wifiConnected) {
    Serial.println("[MQTT] Skipping: No WiFi connection");
    return;
  }

  if (mqttBroker.length() == 0) {
    Serial.println("[MQTT] Skipping: Broker not configured");
    return;
  }

  // Create client if not exists
  if (!mqttClient) {
    mqttClient = new PubSubClient(espClient);
    mqttClient->setServer(mqttBroker.c_str(), mqttPort);
    mqttClient->setCallback(mqttCallback);
    Serial.println("[MQTT] Client created");
  }

  if (mqttClient->connected()) {
    // Already connected
    return;
  }

  Serial.println("[MQTT] Connecting to broker: " + mqttBroker + ":" + String(mqttPort));

  bool connected = false;

  if (mqttUsername.length() > 0 && mqttPassword.length() > 0) {
    connected = mqttClient->connect(
      mqttClientName.c_str(),
      mqttUsername.c_str(),
      mqttPassword.c_str()
    );
    Serial.println("[MQTT] Attempting connection with credentials as: " + mqttClientName);
  } else {
    connected = mqttClient->connect(mqttClientName.c_str());
    Serial.println("[MQTT] Attempting connection without credentials as: " + mqttClientName);
  }

  if (connected) {
    mqttConnected = true;
    Serial.println("[MQTT] Connected!");

    // Subscribe to control topics using stack buffer
    char topic[60];
    const char* pfx = mqttTopicPrefix.c_str();
    
    snprintf(topic, sizeof(topic), "%s/display/power/set", pfx);
    mqttClient->subscribe(topic);
    
    snprintf(topic, sizeof(topic), "%s/modes/clock/set", pfx);
    mqttClient->subscribe(topic);
    
    snprintf(topic, sizeof(topic), "%s/modes/text/set", pfx);
    mqttClient->subscribe(topic);
    
    snprintf(topic, sizeof(topic), "%s/modes/clockDuration/set", pfx);
    mqttClient->subscribe(topic);
    
    snprintf(topic, sizeof(topic), "%s/modes/textDuration/set", pfx);
    mqttClient->subscribe(topic);
    
    snprintf(topic, sizeof(topic), "%s/text/set", pfx);
    mqttClient->subscribe(topic);
    
    snprintf(topic, sizeof(topic), "%s/system/reboot", pfx);
    mqttClient->subscribe(topic);

    // Publish discovery
    publishHomeAssistantDiscovery();
  } else {
    mqttConnected = false;
    Serial.printf("[MQTT] Connection failed, code: %d\n", mqttClient->state());
  }
}

// Initialize MQTT from saved preferences
void initMqtt() {
  Serial.println("\n========== MQTT Setup ==========");

  initializeDeviceId();
  Serial.println("[MQTT] Device ID: " + deviceId);

  // Load MQTT settings from preferences
  preferences.begin("mqtt", false);
  mqttBroker = preferences.getString("broker", "192.168.1.100");
  mqttPort = preferences.getUInt("port", 1883);
  mqttUsername = preferences.getString("username", "");
  mqttPassword = preferences.getString("password", "");
  mqttClientName = preferences.getString("clientname", "RetroPixelLED");
  mqttTopicPrefix = preferences.getString("prefix", "retropixel");
  preferences.end();

  Serial.println("[MQTT] Broker: " + mqttBroker);
  Serial.println("[MQTT] Port: " + String(mqttPort));
  Serial.println("[MQTT] Client Name: " + mqttClientName);
  Serial.println("[MQTT] Topic Prefix: " + mqttTopicPrefix);

  // Ensure any old client is disconnected
  if (mqttClient) {
    mqttClient->disconnect();
    delete mqttClient;
    mqttClient = nullptr;
  }
  
  // First connection attempt (will be retried in loop)
  connectMqtt();
}

// Update MQTT settings (typically called from web API)
void updateMqttSettings(String broker, uint16_t port, String username, String password, String clientname, String prefix) {
  Serial.println("[MQTT] Updating settings");

  mqttBroker = broker;
  mqttPort = port;
  mqttUsername = username;
  mqttPassword = password;
  mqttClientName = clientname;
  mqttTopicPrefix = prefix;

  // Save to preferences
  preferences.begin("mqtt", false);
  preferences.putString("broker", broker);
  preferences.putUInt("port", port);
  preferences.putString("username", username);
  preferences.putString("password", password);
  preferences.putString("clientname", clientname);
  preferences.putString("prefix", prefix);
  preferences.end();

  Serial.println("[MQTT] Settings saved and will reconnect");

  // Reconnect with new settings
  if (mqttClient && mqttClient->connected()) {
    mqttClient->disconnect();
  }
  connectMqtt();
}

// Process MQTT in main loop
void processMqtt() {
  // Check for pending reboot command
  if (pendingReboot) {
    Serial.println("[MQTT] Executing reboot...");
    delay(500);
    ESP.restart();
  }

  if (!wifiConnected) {
    return;
  }

  if (!mqttClient) {
    return;
  }

  if (mqttClient->connected()) {
    mqttClient->loop();
    
    // Periodically publish mode states for HA sync
    if (millis() - lastMqttStatePublish > MQTT_STATE_PUBLISH_INTERVAL) {
      lastMqttStatePublish = millis();
      publishModeStates();
    }
    
    // Periodically publish diagnostics
    if (millis() - lastMqttDiagPublish > MQTT_DIAG_PUBLISH_INTERVAL) {
      lastMqttDiagPublish = millis();
      publishDiagnostics();
    }
  } else {
    // Attempt reconnect periodically
    if (millis() - lastMqttReconnect > MQTT_RECONNECT_INTERVAL) {
      lastMqttReconnect = millis();
      connectMqtt();
    }
  }
}

#endif // MQTT_MANAGER_H
