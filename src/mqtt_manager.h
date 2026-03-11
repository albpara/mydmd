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
extern uint8_t displayMode;
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
  String topicStr = String(topic);
  String payloadStr = "";
  for (unsigned int i = 0; i < length; i++) {
    payloadStr += (char)payload[i];
  }

  Serial.println("[MQTT] Received on " + topicStr + ": " + payloadStr);

  // Control topic: retropixel/display/power (on/off)
  if (topicStr.endsWith("/power/set")) {
    if (payloadStr == "ON" || payloadStr == "on") {
      displayBrightness = 16; // Resume brightness
      Serial.println("[MQTT] Display ON");
    } else if (payloadStr == "OFF" || payloadStr == "off") {
      displayBrightness = 0; // Turn off
      if (dma_display) {
        dma_display->fillScreen(0);
      }
      Serial.println("[MQTT] Display OFF");
    }
    publishDisplayStatus();
  }
  
  // Mode control: retropixel/modes/clock/set (on/off)
  else if (topicStr.endsWith("/modes/clock/set")) {
    modeClockEnabled = (payloadStr == "ON" || payloadStr == "on");
    Serial.println("[MQTT] Clock mode: " + String(modeClockEnabled ? "ON" : "OFF"));
    preferences.begin("wifi", false);
    preferences.putBool("modeClock", modeClockEnabled);
    preferences.end();
  }
  
  // Mode control: retropixel/modes/text/set (on/off)
  else if (topicStr.endsWith("/modes/text/set")) {
    modeTextEnabled = (payloadStr == "ON" || payloadStr == "on");
    Serial.println("[MQTT] Text mode: " + String(modeTextEnabled ? "ON" : "OFF"));
    preferences.begin("wifi", false);
    preferences.putBool("modeText", modeTextEnabled);
    preferences.end();
  }
  
  // Duration control: retropixel/modes/clockDuration/set (seconds)
  else if (topicStr.endsWith("/modes/clockDuration/set")) {
    int dur = atoi(payloadStr.c_str());
    if (dur >= 1 && dur <= 300) {
      modeClockDuration = dur;
      Serial.println("[MQTT] Clock duration: " + String(dur) + "s");
      preferences.begin("wifi", false);
      preferences.putInt("clockDur", dur);
      preferences.end();
    } else {
      Serial.println("[MQTT] Invalid clock duration: " + payloadStr);
    }
  }
  
  // Duration control: retropixel/modes/textDuration/set (seconds)
  else if (topicStr.endsWith("/modes/textDuration/set")) {
    int dur = atoi(payloadStr.c_str());
    if (dur >= 1 && dur <= 300) {
      modeTextDuration = dur;
      Serial.println("[MQTT] Text duration: " + String(dur) + "s");
      preferences.begin("wifi", false);
      preferences.putInt("textDur", dur);
      preferences.end();
    } else {
      Serial.println("[MQTT] Invalid text duration: " + payloadStr);
    }
  }
  
  // Text service: retropixel/text/set (JSON: {"text": "message", "duration": seconds})
  else if (topicStr.endsWith("/text/set")) {
    Serial.println("[MQTT] Text service command: " + payloadStr);
    
    // Simple JSON parsing for text and duration
    int textStart = payloadStr.indexOf("\"text\"");
    int durationStart = payloadStr.indexOf("\"duration\"");
    
    if (textStart >= 0) {
      // Extract text value
      int colonPos = payloadStr.indexOf(':', textStart);
      int quoteStart = payloadStr.indexOf('"', colonPos) + 1;
      int quoteEnd = payloadStr.indexOf('"', quoteStart);
      
      if (quoteStart > 0 && quoteEnd > quoteStart) {
        serviceText = payloadStr.substring(quoteStart, quoteEnd);
        Serial.println("[MQTT] Text to display: " + serviceText);
      }
    }
    
    if (durationStart >= 0) {
      // Extract duration value
      int colonPos = payloadStr.indexOf(':', durationStart);
      int numStart = colonPos + 1;
      int numEnd = payloadStr.indexOf(',', numStart);
      if (numEnd < 0) numEnd = payloadStr.indexOf('}', numStart);
      
      if (numStart > 0 && numEnd > numStart) {
        String durStr = payloadStr.substring(numStart, numEnd);
        durStr.trim();
        uint16_t dur = atoi(durStr.c_str());
        if (dur >= 1 && dur <= 3600) {
          serviceTextDuration = dur;
          Serial.println("[MQTT] Text duration: " + String(dur) + "s");
        }
      }
    }
    
    if (serviceText.length() > 0 && serviceTextDuration > 0) {
      serviceTextActive = true;
      serviceTextStartTime = millis();
      Serial.println("[MQTT] Text service activated!");
    }
  }
  
  // Reboot command: retropixel/system/reboot
  else if (topicStr.endsWith("/system/reboot")) {
    Serial.println("[MQTT] Reboot command received! Payload: " + payloadStr);
    pendingReboot = true;
  }
}

// Publish current display status
void publishDisplayStatus() {
  if (!mqttClient || !mqttClient->connected()) {
    return;
  }

  String stateTopic = mqttTopicPrefix + "/display/state";
  String state = (displayBrightness > 0) ? "ON" : "OFF";
  
  mqttClient->publish(stateTopic.c_str(), state.c_str(), true);
  Serial.println("[MQTT] Published state [" + state + "] to " + stateTopic);
}

// Publish current mode states and durations
void publishModeStates() {
  if (!mqttClient || !mqttClient->connected()) {
    return;
  }

  // Publish clock mode state
  String clockStateStr = modeClockEnabled ? "ON" : "OFF";
  mqttClient->publish((mqttTopicPrefix + "/modes/clock/state").c_str(), clockStateStr.c_str(), true);
  
  // Publish text mode state
  String textStateStr = modeTextEnabled ? "ON" : "OFF";
  mqttClient->publish((mqttTopicPrefix + "/modes/text/state").c_str(), textStateStr.c_str(), true);
  
  // Publish clock duration
  mqttClient->publish(
    (mqttTopicPrefix + "/modes/clockDuration/state").c_str(), 
    String(modeClockDuration).c_str(), 
    true
  );
  
  // Publish text duration
  mqttClient->publish(
    (mqttTopicPrefix + "/modes/textDuration/state").c_str(), 
    String(modeTextDuration).c_str(), 
    true
  );

  Serial.println("[MQTT] Mode states published");
}

// Publish diagnostic information (IP, uptime, link quality)
void publishDiagnostics() {
  if (!mqttClient || !mqttClient->connected()) {
    return;
  }

  // Publish IP address
  String ipAddr = WiFi.localIP().toString();
  mqttClient->publish((mqttTopicPrefix + "/system/ip/state").c_str(), ipAddr.c_str(), true);
  
  // Publish uptime in seconds
  uint32_t uptimeSeconds = millis() / 1000;
  mqttClient->publish(
    (mqttTopicPrefix + "/system/uptime/state").c_str(), 
    String(uptimeSeconds).c_str(), 
    true
  );
  
  // Publish link quality (WiFi RSSI in dBm)
  int32_t rssi = WiFi.RSSI();
  mqttClient->publish(
    (mqttTopicPrefix + "/system/link_quality/state").c_str(), 
    String(rssi).c_str(), 
    true
  );

  Serial.println("[MQTT] Diagnostics published - IP: " + ipAddr + " | Uptime: " + String(uptimeSeconds) + "s | RSSI: " + String(rssi) + "dBm");
}

// Publish Home Assistant MQTT Discovery messages
void publishHomeAssistantDiscovery() {
  if (!mqttClient) {
    Serial.println("[MQTT] Discovery: Client not initialized");
    return;
  }
  
  if (!mqttClient->connected()) {
    Serial.println("[MQTT] Discovery: Not connected to broker");
    return;
  }

  Serial.println("[MQTT] Publishing Home Assistant Discovery");

  // Device info object for all entities
  String deviceInfo = "{";
  deviceInfo += "\"identifiers\":[\"retropixel_" + deviceId + "\"],";
  deviceInfo += "\"manufacturer\":\"RetroPixel\",";
  deviceInfo += "\"model\":\"LED Matrix ESP32\",";
  deviceInfo += "\"name\":\"RetroPixel LED\",";
  deviceInfo += "\"sw_version\":\"1.0.0\"";
  deviceInfo += "}";

  // 1. Discovery for power/display toggle (Light entity)
  String displayTopic = "homeassistant/light/" + deviceId + "/display/config";
  String displayPayload = "{";
  displayPayload += "\"name\":\"Display\",";
  displayPayload += "\"unique_id\":\"retropixel_" + deviceId + "_display\",";
  displayPayload += "\"state_topic\":\"" + mqttTopicPrefix + "/display/state\",";
  displayPayload += "\"command_topic\":\"" + mqttTopicPrefix + "/display/power/set\",";
  displayPayload += "\"payload_on\":\"ON\",";
  displayPayload += "\"payload_off\":\"OFF\",";
  displayPayload += "\"device\":" + deviceInfo;
  displayPayload += "}";
  mqttClient->publish(displayTopic.c_str(), displayPayload.c_str(), true);
  Serial.println("[MQTT] ✓ Display discovery published");

  // 2. Discovery for Clock Mode (Switch entity)
  String clockModeTopic = "homeassistant/switch/" + deviceId + "/clock_mode/config";
  String clockModePayload = "{";
  clockModePayload += "\"name\":\"Clock Mode\",";
  clockModePayload += "\"unique_id\":\"retropixel_" + deviceId + "_clock_mode\",";
  clockModePayload += "\"state_topic\":\"" + mqttTopicPrefix + "/modes/clock/state\",";
  clockModePayload += "\"command_topic\":\"" + mqttTopicPrefix + "/modes/clock/set\",";
  clockModePayload += "\"payload_on\":\"ON\",";
  clockModePayload += "\"payload_off\":\"OFF\",";
  clockModePayload += "\"device\":" + deviceInfo;
  clockModePayload += "}";
  mqttClient->publish(clockModeTopic.c_str(), clockModePayload.c_str(), true);
  Serial.println("[MQTT] ✓ Clock mode discovery published");

  // 3. Discovery for Text Mode (Switch entity)
  String textModeTopic = "homeassistant/switch/" + deviceId + "/text_mode/config";
  String textModePayload = "{";
  textModePayload += "\"name\":\"Text Mode\",";
  textModePayload += "\"unique_id\":\"retropixel_" + deviceId + "_text_mode\",";
  textModePayload += "\"state_topic\":\"" + mqttTopicPrefix + "/modes/text/state\",";
  textModePayload += "\"command_topic\":\"" + mqttTopicPrefix + "/modes/text/set\",";
  textModePayload += "\"payload_on\":\"ON\",";
  textModePayload += "\"payload_off\":\"OFF\",";
  textModePayload += "\"device\":" + deviceInfo;
  textModePayload += "}";
  mqttClient->publish(textModeTopic.c_str(), textModePayload.c_str(), true);
  Serial.println("[MQTT] ✓ Text mode discovery published");

  // 4. Discovery for Clock Duration (Number entity - Configuration)
  String clockDurTopic = "homeassistant/number/" + deviceId + "/clock_duration/config";
  String clockDurPayload = "{";
  clockDurPayload += "\"name\":\"Clock Duration\",";
  clockDurPayload += "\"unique_id\":\"retropixel_" + deviceId + "_clock_duration\",";
  clockDurPayload += "\"state_topic\":\"" + mqttTopicPrefix + "/modes/clockDuration/state\",";
  clockDurPayload += "\"command_topic\":\"" + mqttTopicPrefix + "/modes/clockDuration/set\",";
  clockDurPayload += "\"unit_of_measurement\":\"s\",";
  clockDurPayload += "\"entity_category\":\"config\",";
  clockDurPayload += "\"min\":1,";
  clockDurPayload += "\"max\":300,";
  clockDurPayload += "\"step\":1,";
  clockDurPayload += "\"device\":" + deviceInfo;
  clockDurPayload += "}";
  mqttClient->publish(clockDurTopic.c_str(), clockDurPayload.c_str(), true);
  Serial.println("[MQTT] ✓ Clock duration discovery published");

  // 5. Discovery for Text Duration (Number entity - Configuration)
  String textDurTopic = "homeassistant/number/" + deviceId + "/text_duration/config";
  String textDurPayload = "{";
  textDurPayload += "\"name\":\"Text Duration\",";
  textDurPayload += "\"unique_id\":\"retropixel_" + deviceId + "_text_duration\",";
  textDurPayload += "\"state_topic\":\"" + mqttTopicPrefix + "/modes/textDuration/state\",";
  textDurPayload += "\"command_topic\":\"" + mqttTopicPrefix + "/modes/textDuration/set\",";
  textDurPayload += "\"unit_of_measurement\":\"s\",";
  textDurPayload += "\"entity_category\":\"config\",";
  textDurPayload += "\"min\":1,";
  textDurPayload += "\"max\":300,";
  textDurPayload += "\"step\":1,";
  textDurPayload += "\"device\":" + deviceInfo;
  textDurPayload += "}";
  mqttClient->publish(textDurTopic.c_str(), textDurPayload.c_str(), true);
  Serial.println("[MQTT] ✓ Text duration discovery published");

  // 6. Discovery for IP Address (Diagnostic sensor)
  String ipTopic = "homeassistant/sensor/" + deviceId + "/ip_address/config";
  String ipPayload = "{";
  ipPayload += "\"name\":\"IP Address\",";
  ipPayload += "\"unique_id\":\"retropixel_" + deviceId + "_ip_address\",";
  ipPayload += "\"state_topic\":\"" + mqttTopicPrefix + "/system/ip/state\",";
  ipPayload += "\"entity_category\":\"diagnostic\",";
  ipPayload += "\"icon\":\"mdi:ip\",";
  ipPayload += "\"device\":" + deviceInfo;
  ipPayload += "}";
  mqttClient->publish(ipTopic.c_str(), ipPayload.c_str(), true);
  Serial.println("[MQTT] ✓ IP address discovery published");

  // 7. Discovery for Uptime (Diagnostic sensor)
  String uptimeTopic = "homeassistant/sensor/" + deviceId + "/uptime/config";
  String uptimePayload = "{";
  uptimePayload += "\"name\":\"Uptime\",";
  uptimePayload += "\"unique_id\":\"retropixel_" + deviceId + "_uptime\",";
  uptimePayload += "\"state_topic\":\"" + mqttTopicPrefix + "/system/uptime/state\",";
  uptimePayload += "\"unit_of_measurement\":\"s\",";
  uptimePayload += "\"entity_category\":\"diagnostic\",";
  uptimePayload += "\"icon\":\"mdi:clock-outline\",";
  uptimePayload += "\"device\":" + deviceInfo;
  uptimePayload += "}";
  mqttClient->publish(uptimeTopic.c_str(), uptimePayload.c_str(), true);
  Serial.println("[MQTT] ✓ Uptime discovery published");

  // 8. Discovery for Link Quality/RSSI (Diagnostic sensor)
  String rssiTopic = "homeassistant/sensor/" + deviceId + "/link_quality/config";
  String rssiPayload = "{";
  rssiPayload += "\"name\":\"Link Quality\",";
  rssiPayload += "\"unique_id\":\"retropixel_" + deviceId + "_link_quality\",";
  rssiPayload += "\"state_topic\":\"" + mqttTopicPrefix + "/system/link_quality/state\",";
  rssiPayload += "\"unit_of_measurement\":\"dBm\",";
  rssiPayload += "\"entity_category\":\"diagnostic\",";
  rssiPayload += "\"icon\":\"mdi:signal\",";
  rssiPayload += "\"device\":" + deviceInfo;
  rssiPayload += "}";
  mqttClient->publish(rssiTopic.c_str(), rssiPayload.c_str(), true);
  Serial.println("[MQTT] ✓ Link quality discovery published");

  // 9. Discovery for Reboot Button
  String rebootTopic = "homeassistant/button/" + deviceId + "/reboot/config";
  String rebootPayload = "{";
  rebootPayload += "\"name\":\"Reboot\",";
  rebootPayload += "\"unique_id\":\"retropixel_" + deviceId + "_reboot\",";
  rebootPayload += "\"command_topic\":\"" + mqttTopicPrefix + "/system/reboot\",";
  rebootPayload += "\"payload_press\":\"true\",";
  rebootPayload += "\"entity_category\":\"config\",";
  rebootPayload += "\"icon\":\"mdi:restart\",";
  rebootPayload += "\"device\":" + deviceInfo;
  rebootPayload += "}";
  mqttClient->publish(rebootTopic.c_str(), rebootPayload.c_str(), true);
  Serial.println("[MQTT] ✓ Reboot button discovery published");

  // 10. Discovery for Text Display Service (Command topic)
  String textServiceTopic = "homeassistant/text/" + deviceId + "/text_service/config";
  String textServicePayload = "{";
  textServicePayload += "\"name\":\"Display Text\",";
  textServicePayload += "\"unique_id\":\"retropixel_" + deviceId + "_text_service\",";
  textServicePayload += "\"command_topic\":\"" + mqttTopicPrefix + "/text/set\",";
  textServicePayload += "\"entity_category\":\"config\",";
  textServicePayload += "\"icon\":\"mdi:message\",";
  textServicePayload += "\"device\":" + deviceInfo;
  textServicePayload += "}";
  mqttClient->publish(textServiceTopic.c_str(), textServicePayload.c_str(), true);
  Serial.println("[MQTT] ✓ Text service discovery published");

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

    // Subscribe to control topics
    String powerSetTopic = mqttTopicPrefix + "/display/power/set";
    mqttClient->subscribe(powerSetTopic.c_str());
    Serial.println("[MQTT] Subscribed to: " + powerSetTopic);
    
    // Subscribe to mode control topics
    String clockModeTopic = mqttTopicPrefix + "/modes/clock/set";
    mqttClient->subscribe(clockModeTopic.c_str());
    Serial.println("[MQTT] Subscribed to: " + clockModeTopic);
    
    String textModeTopic = mqttTopicPrefix + "/modes/text/set";
    mqttClient->subscribe(textModeTopic.c_str());
    Serial.println("[MQTT] Subscribed to: " + textModeTopic);
    
    String clockDurTopic = mqttTopicPrefix + "/modes/clockDuration/set";
    mqttClient->subscribe(clockDurTopic.c_str());
    Serial.println("[MQTT] Subscribed to: " + clockDurTopic);
    
    String textDurTopic = mqttTopicPrefix + "/modes/textDuration/set";
    mqttClient->subscribe(textDurTopic.c_str());
    Serial.println("[MQTT] Subscribed to: " + textDurTopic);
    
    // Subscribe to text service
    String textServiceTopic = mqttTopicPrefix + "/text/set";
    mqttClient->subscribe(textServiceTopic.c_str());
    Serial.println("[MQTT] Subscribed to: " + textServiceTopic);
    
    // Subscribe to system topics
    String rebootTopic = mqttTopicPrefix + "/system/reboot";
    mqttClient->subscribe(rebootTopic.c_str());
    Serial.println("[MQTT] Subscribed to: " + rebootTopic);

    // Publish discovery
    publishHomeAssistantDiscovery();
  } else {
    mqttConnected = false;
    int state = mqttClient->state();
    Serial.println("[MQTT] Connection failed, code: " + String(state));
    switch(state) {
      case -4: Serial.println("[MQTT] Error: Connection timeout"); break;
      case -3: Serial.println("[MQTT] Error: Connection lost"); break;
      case -2: Serial.println("[MQTT] Error: Connect failed"); break;
      case -1: Serial.println("[MQTT] Error: Disconnected"); break;
      case 1: Serial.println("[MQTT] Error: Bad protocol"); break;
      case 2: Serial.println("[MQTT] Error: Bad client ID"); break;
      case 3: Serial.println("[MQTT] Error: Unavailable"); break;
      case 4: Serial.println("[MQTT] Error: Bad credentials"); break;
      case 5: Serial.println("[MQTT] Error: Unauthorized"); break;
    }
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
