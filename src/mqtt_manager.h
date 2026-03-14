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
extern bool modeGifEnabled;
extern uint16_t modeClockDuration;
extern uint16_t modeTextDuration;
extern uint16_t modeGifDuration;
extern String serviceText;
extern uint16_t serviceTextDuration;
extern bool serviceTextActive;
extern uint32_t serviceTextStartTime;
extern int serviceTextScrollX;
extern bool serviceTextScrollCompleted;

// GIF service externs
extern String serviceGifPath;
extern bool serviceGifActive;
extern uint16_t serviceGifDuration;
extern uint32_t serviceGifStartTime;
extern int totalGifFiles;
extern bool sdMounted;
bool startSpecificGif(const char *path);

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
const uint32_t MQTT_RECONNECT_INTERVAL = 15000; // 15 seconds (avoid frequent blocking)
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

  LOGF("[MQTT] %s: %s\n", topic, msg);

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
  // Mode control: gif
  else if (strstr(topic, "/modes/gif/set")) {
    modeGifEnabled = (strcasecmp(msg, "ON") == 0);
    preferences.begin("wifi", false);
    preferences.putBool("modeGif", modeGifEnabled);
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
  // Duration control: gif
  else if (strstr(topic, "/modes/gifDuration/set")) {
    int dur = atoi(msg);
    if (dur >= 1 && dur <= 300) {
      modeGifDuration = dur;
      preferences.begin("wifi", false);
      preferences.putInt("gifDur", dur);
      preferences.end();
    }
  }
  // Text/GIF service: JSON {"text": "...", "duration": seconds}
  // If "text" value starts with / and ends with .gif, treat as GIF path
  else if (strstr(topic, "/text/set")) {
    // Simple C-string JSON parsing
    char* textStart = strstr(msg, "\"text\"");
    char* durStart = strstr(msg, "\"duration\"");
    
    String parsedText = "";
    uint16_t parsedDuration = 0;
    
    if (textStart) {
      char* colon = strchr(textStart, ':');
      if (colon) {
        char* q1 = strchr(colon, '"');
        if (q1) {
          q1++;
          char* q2 = strchr(q1, '"');
          if (q2) {
            *q2 = '\0';
            parsedText = String(q1);
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
          parsedDuration = dur;
        }
      }
    }
    
    if (parsedText.length() > 0 && parsedDuration > 0) {
      // Check if payload is a GIF path (starts with / and ends with .gif)
      if (parsedText.startsWith("/") &&
          (parsedText.endsWith(".gif") || parsedText.endsWith(".GIF"))) {
        // GIF service request
        serviceTextActive = false;  // Cancel any active text
        serviceGifPath = parsedText;
        serviceGifDuration = parsedDuration;
        serviceGifStartTime = millis();
        serviceGifActive = true;
        LOGF("[MQTT] Service GIF: %s (%ds)\n", parsedText.c_str(), parsedDuration);
      } else {
        // Text service request (notification — does not override scrollText)
        serviceGifActive = false;  // Cancel any active GIF
        serviceText = parsedText;
        serviceTextDuration = parsedDuration;
        serviceTextActive = true;
        serviceTextStartTime = millis();
        serviceTextScrollX = DISPLAY_WIDTH;  // Reset scroll position
        serviceTextScrollCompleted = false;  // Require at least one full cycle
      }
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
  
  snprintf(topic, sizeof(topic), "%s/modes/gif/state", pfx);
  mqttClient->publish(topic, modeGifEnabled ? "ON" : "OFF", true);
  
  snprintf(topic, sizeof(topic), "%s/modes/clockDuration/state", pfx);
  snprintf(val, sizeof(val), "%u", modeClockDuration);
  mqttClient->publish(topic, val, true);
  
  snprintf(topic, sizeof(topic), "%s/modes/textDuration/state", pfx);
  snprintf(val, sizeof(val), "%u", modeTextDuration);
  mqttClient->publish(topic, val, true);
  
  snprintf(topic, sizeof(topic), "%s/modes/gifDuration/state", pfx);
  snprintf(val, sizeof(val), "%u", modeGifDuration);
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

// Helper to publish a single HA discovery entity
static void publishHAEntity(const char* did, const char* pfx, const char* dev,
  const char* haType, const char* objId, const char* name, const char* uid,
  const char* statSuffix, const char* cmdSuffix, const char* extra) {
  char topic[80], payload[512];
  snprintf(topic, sizeof(topic), "homeassistant/%s/%s/%s/config", haType, did, objId);
  int n = snprintf(payload, sizeof(payload), "{\"name\":\"%s\",\"uniq_id\":\"rp_%s_%s\"", name, did, uid);
  if (statSuffix) n += snprintf(payload+n, sizeof(payload)-n, ",\"stat_t\":\"%s/%s\"", pfx, statSuffix);
  if (cmdSuffix) n += snprintf(payload+n, sizeof(payload)-n, ",\"cmd_t\":\"%s/%s\"", pfx, cmdSuffix);
  if (extra) n += snprintf(payload+n, sizeof(payload)-n, ",%s", extra);
  snprintf(payload+n, sizeof(payload)-n, ",%s}", dev);
  mqttClient->publish(topic, payload, true);
}

// Publish Home Assistant MQTT Discovery messages
void publishHomeAssistantDiscovery() {
  if (!mqttClient || !mqttClient->connected()) return;

  LOG("[MQTT] Publishing HA Discovery");

  const char* did = deviceId.c_str();
  const char* pfx = mqttTopicPrefix.c_str();

  char dev[180];
  snprintf(dev, sizeof(dev),
    "\"dev\":{\"ids\":[\"rp_%s\"],"
    "\"mf\":\"RetroPixel\","
    "\"mdl\":\"LED Matrix ESP32\","
    "\"name\":\"RetroPixel LED\","
    "\"sw\":\"1.0\"}", did);

  // Shared extra fragments
  static const char ONOFF[] = "\"pl_on\":\"ON\",\"pl_off\":\"OFF\"";
  static const char NUM_CFG[] = "\"unit_of_meas\":\"s\",\"ent_cat\":\"config\",\"min\":1,\"max\":300,\"step\":1";

  // Light
  publishHAEntity(did, pfx, dev, "light", "display", "Display", "disp",
    "display/state", "display/power/set", ONOFF);
  // Switches
  publishHAEntity(did, pfx, dev, "switch", "clock_mode", "Mode Clock", "clk",
    "modes/clock/state", "modes/clock/set", ONOFF);
  publishHAEntity(did, pfx, dev, "switch", "text_mode", "Mode Text", "txt",
    "modes/text/state", "modes/text/set", ONOFF);
  publishHAEntity(did, pfx, dev, "switch", "gif_mode", "Mode GIF", "gif",
    "modes/gif/state", "modes/gif/set", "\"ic\":\"mdi:gif\",\"pl_on\":\"ON\",\"pl_off\":\"OFF\"");
  // Number entities
  publishHAEntity(did, pfx, dev, "number", "clock_dur", "Duration Clock", "clkdur",
    "modes/clockDuration/state", "modes/clockDuration/set", NUM_CFG);
  publishHAEntity(did, pfx, dev, "number", "text_dur", "Duration Text", "txtdur",
    "modes/textDuration/state", "modes/textDuration/set", NUM_CFG);
  publishHAEntity(did, pfx, dev, "number", "gif_dur", "Duration GIF", "gifdur",
    "modes/gifDuration/state", "modes/gifDuration/set", NUM_CFG);
  // Diagnostic sensors
  publishHAEntity(did, pfx, dev, "sensor", "ip", "IP Address", "ip",
    "system/ip/state", NULL, "\"ent_cat\":\"diagnostic\",\"ic\":\"mdi:ip\"");
  publishHAEntity(did, pfx, dev, "sensor", "uptime", "Uptime", "up",
    "system/uptime/state", NULL, "\"unit_of_meas\":\"s\",\"ent_cat\":\"diagnostic\",\"ic\":\"mdi:clock-outline\"");
  publishHAEntity(did, pfx, dev, "sensor", "rssi", "Link Quality", "rssi",
    "system/link_quality/state", NULL, "\"unit_of_meas\":\"dBm\",\"ent_cat\":\"diagnostic\",\"ic\":\"mdi:signal\"");
  // Button
  publishHAEntity(did, pfx, dev, "button", "reboot", "Reboot", "reboot",
    NULL, "system/reboot", "\"pl_press\":\"true\",\"ent_cat\":\"config\",\"ic\":\"mdi:restart\"");

  LOG("[MQTT] HA Discovery complete");

  // Publish initial states
  publishDisplayStatus();
  publishModeStates();
  publishDiagnostics();
}

// Connect to MQTT broker
void connectMqtt() {
  if (!wifiConnected) {
    LOG("[MQTT] Skipping: No WiFi connection");
    return;
  }

  if (mqttBroker.length() == 0) {
    LOG("[MQTT] Skipping: Broker not configured");
    return;
  }

  // Create client if not exists
  if (!mqttClient) {
    mqttClient = new PubSubClient(espClient);
    mqttClient->setServer(mqttBroker.c_str(), mqttPort);
    mqttClient->setCallback(mqttCallback);
    espClient.setTimeout(2);  // 2-second TCP timeout (default is ~15s)
    LOG("[MQTT] Client created");
  }

  if (mqttClient->connected()) {
    // Already connected
    return;
  }

  LOG("[MQTT] Connecting to broker: " + mqttBroker + ":" + String(mqttPort));

  bool connected = false;

  if (mqttUsername.length() > 0 && mqttPassword.length() > 0) {
    connected = mqttClient->connect(
      mqttClientName.c_str(),
      mqttUsername.c_str(),
      mqttPassword.c_str()
    );
    LOG("[MQTT] Attempting connection with credentials as: " + mqttClientName);
  } else {
    connected = mqttClient->connect(mqttClientName.c_str());
    LOG("[MQTT] Attempting connection without credentials as: " + mqttClientName);
  }

  if (connected) {
    mqttConnected = true;
    LOG("[MQTT] Connected!");

    // Subscribe to control topics using stack buffer
    char topic[60];
    const char* pfx = mqttTopicPrefix.c_str();
    
    snprintf(topic, sizeof(topic), "%s/display/power/set", pfx);
    mqttClient->subscribe(topic);
    
    snprintf(topic, sizeof(topic), "%s/modes/clock/set", pfx);
    mqttClient->subscribe(topic);
    
    snprintf(topic, sizeof(topic), "%s/modes/text/set", pfx);
    mqttClient->subscribe(topic);
    
    snprintf(topic, sizeof(topic), "%s/modes/gif/set", pfx);
    mqttClient->subscribe(topic);
    
    snprintf(topic, sizeof(topic), "%s/modes/clockDuration/set", pfx);
    mqttClient->subscribe(topic);
    
    snprintf(topic, sizeof(topic), "%s/modes/textDuration/set", pfx);
    mqttClient->subscribe(topic);
    
    snprintf(topic, sizeof(topic), "%s/modes/gifDuration/set", pfx);
    mqttClient->subscribe(topic);
    
    snprintf(topic, sizeof(topic), "%s/text/set", pfx);
    mqttClient->subscribe(topic);
    
    snprintf(topic, sizeof(topic), "%s/system/reboot", pfx);
    mqttClient->subscribe(topic);

    // Publish discovery
    publishHomeAssistantDiscovery();
  } else {
    mqttConnected = false;
    LOGF("[MQTT] Connection failed, code: %d\n", mqttClient->state());
  }
}

// Initialize MQTT from saved preferences
void initMqtt() {
  LOG("\n========== MQTT Setup ==========");

  initializeDeviceId();
  LOG("[MQTT] Device ID: " + deviceId);

  // Load MQTT settings from preferences
  preferences.begin("mqtt", false);
  mqttBroker = preferences.getString("broker", "192.168.1.100");
  mqttPort = preferences.getUInt("port", 1883);
  mqttUsername = preferences.getString("username", "");
  mqttPassword = preferences.getString("password", "");
  mqttClientName = preferences.getString("clientname", "RetroPixelLED");
  mqttTopicPrefix = preferences.getString("prefix", "retropixel");
  preferences.end();

  LOG("[MQTT] Broker: " + mqttBroker);
  LOG("[MQTT] Port: " + String(mqttPort));
  LOG("[MQTT] Client Name: " + mqttClientName);
  LOG("[MQTT] Topic Prefix: " + mqttTopicPrefix);

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
  LOG("[MQTT] Updating settings");

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

  LOG("[MQTT] Settings saved and will reconnect");

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
    LOG("[MQTT] Executing reboot...");
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
    // Only attempt MQTT reconnect if WiFi is actually connected
    if (WiFi.status() == WL_CONNECTED) {
      if (millis() - lastMqttReconnect > MQTT_RECONNECT_INTERVAL) {
        lastMqttReconnect = millis();
        LOG("[MQTT] Attempting reconnect...");
        connectMqtt();
      }
    }
  }
}

#endif // MQTT_MANAGER_H
