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

// Device identifiers for Home Assistant
String deviceId = "";

// Forward declare functions
void initializeDeviceId();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void publishHomeAssistantDiscovery();
void publishDisplayStatus();
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

  // Discovery for power/display toggle
  String discoveryTopic = "homeassistant/light/" + deviceId + "/display/config";
  String discoveryPayload = "{";
  discoveryPayload += "\"name\":\"Display\",";
  discoveryPayload += "\"unique_id\":\"retropixel_" + deviceId + "_display\",";
  discoveryPayload += "\"state_topic\":\"" + mqttTopicPrefix + "/display/state\",";
  discoveryPayload += "\"command_topic\":\"" + mqttTopicPrefix + "/display/power/set\",";
  discoveryPayload += "\"payload_on\":\"ON\",";
  discoveryPayload += "\"payload_off\":\"OFF\",";
  discoveryPayload += "\"device\":" + deviceInfo;
  discoveryPayload += "}";

  Serial.println("[MQTT] Discovery topic: " + discoveryTopic);
  Serial.println("[MQTT] Discovery payload size: " + String(discoveryPayload.length()) + " bytes");
  Serial.println("[MQTT] Publishing discovery...");
  
  bool published = mqttClient->publish(discoveryTopic.c_str(), discoveryPayload.c_str(), true);
  if (published) {
    Serial.println("[MQTT] ✓ Discovery message published successfully");
  } else {
    int state = mqttClient->state();
    Serial.println("[MQTT] ✗ FAILED to publish discovery message");
    Serial.println("[MQTT] Client state: " + String(state));
    if (state != 0) {
      Serial.println("[MQTT] (0=connected, 1=bad protocol, 2=bad client ID, 3=unavailable, 4=bad credentials, 5=unauthorized)");
    }
  }

  // Publish initial state
  publishDisplayStatus();
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
  if (!wifiConnected) {
    return;
  }

  if (!mqttClient) {
    return;
  }

  if (mqttClient->connected()) {
    mqttClient->loop();
  } else {
    // Attempt reconnect periodically
    if (millis() - lastMqttReconnect > MQTT_RECONNECT_INTERVAL) {
      lastMqttReconnect = millis();
      connectMqtt();
    }
  }
}

#endif // MQTT_MANAGER_H
