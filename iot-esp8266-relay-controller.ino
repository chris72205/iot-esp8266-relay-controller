#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <ESP8266WiFi.h>

#include "Sensitive.h"

// Wifi
const char* wifiSsid = WIFI_SSID;
const char* wifiPass = WIFI_PASS;
WiFiClient wifiClient;

// MQTT
const char* mqttHostname = MQTT_HOSTNAME;
const char* mqttClientId = MQTT_CLIENT_ID;
const char* mqttUsername = MQTT_USERNAME;
const char* mqttPassword = MQTT_PASSWORD;
const char* mqttPubTopic = MQTT_TOPIC_PUB;
PubSubClient mqttClient(wifiClient);

// Pin inputs and outputs
const int switchPin = 13;
const int relayPin = 14;

// States
bool switchState;
bool relayState;
unsigned long lastRelayChange;

// Start time
unsigned long startTime;
unsigned long statusLastSentAt;

// Important times
const unsigned int debouncePeriod = 2000; // Period of time (in ms) that we must wait before toggling relay
const unsigned int publishInterval = 5000; // Interval in which we should publish relay status

/*
 * ---------------------------------------------
 * Connection methods
 * ---------------------------------------------
 */
void ensureWifiConnection() {
  // (re) connect to wifi
  if(WiFi.status() == WL_CONNECTED) {
    return;
  }

  Serial.print("Wifi not connected, attempting to connect...");

  WiFi.begin(wifiSsid, wifiPass);
  while(WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  Serial.print("wifi connected!");
  Serial.print("  IP address: ");
  Serial.println(WiFi.localIP());
}

void ensureMqttConnection() {
  // (re) connect to MQTT and ensure we're listening properly
  if(mqttClient.connected()) {
    return;
  }

  Serial.print("MQTT not connected, attempting to connect...");
  while(!mqttClient.connected()) {
    mqttClient.setServer(mqttHostname, 1883);
    mqttClient.setCallback(mqttCallback);
    if(mqttClient.connect(mqttClientId, mqttUsername, mqttPassword)) {
      Serial.println("MQTT connected!");

      // todo(chrisjacob): see if there's a cleaner way to do this
      char subTopicWithClientId[sizeof(MQTT_TOPIC_SUB) + 4 + sizeof(MQTT_CLIENT_ID) + 4] = {};

      strcpy(subTopicWithClientId, MQTT_TOPIC_SUB);
      strcat(subTopicWithClientId, ".");
      strcat(subTopicWithClientId, MQTT_CLIENT_ID);

      Serial.print("Subscribing to topic [");
      Serial.print(subTopicWithClientId);
      Serial.print("] ...");
      mqttClient.subscribe(subTopicWithClientId);
      Serial.println("subscribed!");
    } else {
      Serial.print(".");
      delay(5000);
    }
  }
}

/**
 * Publish device info – should only be done once on boot when connection is established
 */
void publishDeviceInfo() {
  const char* pubTopic = "devices.info.connected";

  DynamicJsonDocument doc(128);

  String macAddress = WiFi.macAddress();
  String localIP = WiFi.localIP().toString();

  doc["clientId"] = mqttClientId;
  doc["macAddress"] = macAddress;
  doc["ipAddress"] = localIP;

  String payload;
  const unsigned int jsonSize = serializeJson(doc, payload);

  char mqttPayload[jsonSize + 4];

  payload.toCharArray(mqttPayload, jsonSize + 4);
  

  mqttClient.publish(pubTopic, mqttPayload);
}


/*
 * ---------------------------------------------
 * Input/output handling
 * ---------------------------------------------
 */
 

 /**
  * Report relay state as needed and record when it was last reported
  */
void reportRelayState() {
  // todo(chrisjacob): improve payload of this message
  mqttClient.publish(mqttPubTopic, relayState ? "relay on" : "relay off");

  statusLastSentAt = millis();
}

/**
 * Compares the current switch state with the last state we had
 */
bool hasSwitchStateChanged(bool currentState) {
  if(currentState != switchState) {
    return true;
  } else {
    return false;
  }
}

/**
 * Handle incoming messages that we've subscribed to
 */
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  for(int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }

  Serial.println();

  // todo(chrisjacob): parse out the payload so that we can act on it
}

/**
 * Toggle the relay state if certain conditions are met
 * 
 * Must be outside of the debounce period since the relay
 * state last changed
 */
void toggleRelay() {
  unsigned long currentMillis = millis();

  if(currentMillis < lastRelayChange && currentMillis > debouncePeriod) {
    // we have rolled over and current time is outside of debouncePeriod, so we can do this
    relayState = !relayState;
    lastRelayChange = currentMillis;
  } else if(currentMillis > (lastRelayChange + debouncePeriod)) {
    // current time is more than the debounce period since the last relay change
    relayState = !relayState;
    lastRelayChange = currentMillis;
  }

  // do nothing, since we are within the debounce period
}

/**
 * Update relay pin based on relayState
 */
void updateRelay() {
  Serial.println("Updating relay state");
  digitalWrite(relayPin, relayState ? HIGH : LOW);
}

/**
 * Publish current relay state if needed
 * 
 * Publishes state if it's been {publishInterval} since the last
 * update.  This method takes into account millis() wrapping
 */
void publishStateIfNeeded() {
  unsigned long currentMillis = millis();

  // currentTime > 5 seconds ago || millis() < statusLastSentAt (for millis() wrapping) && currentMillis >= 5 seconds
  if(currentMillis >= (statusLastSentAt + publishInterval) || (currentMillis < statusLastSentAt && currentMillis >= publishInterval)) {
    Serial.println("Reporting relay state");
    reportRelayState();
  }
}

// ---------------------------------------------------------------------------

void setup() {
  // set Serial rate
  Serial.begin(115200);
  
  // setup pins
  pinMode(switchPin, INPUT);
  pinMode(relayPin, OUTPUT);
  
  // read initial switch(es) state
  switchState = digitalRead(switchPin);
  relayState = false;
  Serial.print("switchState is: ");
  Serial.println(switchState);

  // update relay to reflect initial state
  updateRelay();
  
  // connect to wifi
  ensureWifiConnection();
  
  // connect to mqtt
  ensureMqttConnection();

  // publish device info to mqtt
  publishDeviceInfo();
}

void loop() {
  // ensure connected to wifi
  ensureWifiConnection();
  
  // ensure connected to mqtt
  ensureMqttConnection();
  mqttClient.loop();

  // check if switch state has changed
  if(hasSwitchStateChanged(digitalRead(switchPin))) {
    Serial.println("Switch state has changed");
    
    // change the relay to opposite of it's current state
    toggleRelay();
    updateRelay();
    
    // publish to topic that we are changing the state
    reportRelayState();
  }

  // publish status depending on time elapsed
  publishStateIfNeeded();
}
