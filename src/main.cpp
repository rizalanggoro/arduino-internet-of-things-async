#include <Arduino.h>
#include <ArduinoJson.h>
#include <AsyncMqttClient.h>
#include <ESP8266WiFi.h>
#include <Ticker.h>
#include <WS2812FX.h>

#include "iot_config.hpp"

AsyncMqttClient mqttClient;
Ticker mqttReconnectTimer;

WiFiEventHandler wifiConnectHandler;
WiFiEventHandler wifiDisconnectHandler;
Ticker wifiReconnectTimer;

WS2812FX fx(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

// todo: prototype functions
void beginWifi();
void connectToWifi();
void onWifiConnect(const WiFiEventStationModeGotIP& event);
void onWifiDisconnect(const WiFiEventStationModeDisconnected& event);

void beginMqtt();
void initMqttTopics();
void connectToMqtt();
void onMqttMessage(char* topic, char* payload,
                   AsyncMqttClientMessageProperties properties, size_t len,
                   size_t index, size_t total);
void onMqttConnect(bool sessionPresent);
void onMqttDisconnect(AsyncMqttClientDisconnectReason reason);

void parser(String topic, String data);
void streamParser();
// todo: end -> prototype functions

// todo: device config
int deviceMode = FX_MODE_STATIC;
int deviceBrightness = 100;
int deviceSpeed = 2048;
int deviceColor = BLUE;
int deviceAutoEnable = 0;
int deviceAutoDelay = 10240;
DynamicJsonDocument docAutoValues(256);
String deviceAutoValuesStr = "";
// todo: end -> device config

// todo: mqtt topics
String topicWill;
String topicMode;
String topicBrightness;
String topicSpeed;
String topicColor;
String topicAuto;
String topicAutoDelay;
String topicAutoValues;
// todo: end -> mqtt topics

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);

  digitalWrite(LED_BUILTIN, HIGH);

  fx.init();
  fx.setMode(deviceMode);
  fx.setBrightness(deviceBrightness);
  fx.setSpeed(deviceSpeed);
  fx.setColor(deviceColor);
  fx.start();

  initMqttTopics();
  beginMqtt();
  beginWifi();
}

void loop() {
  fx.service();

  streamParser();
}

// todo: wifi functions
void beginWifi() {
  wifiConnectHandler = WiFi.onStationModeGotIP(onWifiConnect);
  wifiDisconnectHandler = WiFi.onStationModeDisconnected(onWifiDisconnect);

  connectToWifi();
}

void connectToWifi() {
  Serial.println("Connecting to Wi-Fi...");
  WiFi.begin(WIFI_NAME, WIFI_PASS);
}

void onWifiConnect(const WiFiEventStationModeGotIP& event) {
  Serial.println("Connected to Wi-Fi.");
  connectToMqtt();
}

void onWifiDisconnect(const WiFiEventStationModeDisconnected& event) {
  Serial.println("Disconnected from Wi-Fi.");
  mqttReconnectTimer.detach();
  wifiReconnectTimer.once(2, connectToWifi);
}
// todo: end -> wifi functions

// todo: mqtt functions
void beginMqtt() {
  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onMessage(onMqttMessage);
  mqttClient.setServer(BROKER_HOST, BROKER_PORT);
  mqttClient.setWill(topicWill.c_str(), 2, true, "off");
}

void connectToMqtt() {
  Serial.println("Connecting to MQTT...");
  mqttClient.connect();
}

void initMqttTopics() {
  String prefix = String(COMPANY) + "-" + String(DEVICE_ID);

  topicWill = prefix + "/status";
  topicMode = prefix + "/mode";
  topicBrightness = prefix + "/brightness";
  topicSpeed = prefix + "/speed";
  topicColor = prefix + "/color";
  topicAuto = prefix + "/auto";
  topicAutoDelay = prefix + "/auto_delay";
  topicAutoValues = prefix + "/auto_values";
}

void onMqttMessage(char* topic, char* payload,
                   AsyncMqttClientMessageProperties properties, size_t len,
                   size_t index, size_t total) {
  Serial.print("[");
  Serial.print(topic);
  Serial.print("] ");

  String data = "";
  for (size_t a = 0; a < len; a++) data += (char)payload[a];

  Serial.println(data);

  parser(String(topic), data);
}

void onMqttConnect(bool sessionPresent) {
  digitalWrite(LED_BUILTIN, LOW);
  Serial.println("Connected to MQTT.");

  mqttClient.publish(topicWill.c_str(), 2, true, "on");

  // todo: subscribe to all topics
  mqttClient.subscribe(topicMode.c_str(), 2);
  mqttClient.subscribe(topicBrightness.c_str(), 2);
  mqttClient.subscribe(topicSpeed.c_str(), 2);
  mqttClient.subscribe(topicColor.c_str(), 2);
  mqttClient.subscribe(topicAuto.c_str(), 2);
  mqttClient.subscribe(topicAutoDelay.c_str(), 2);
  mqttClient.subscribe(topicAutoValues.c_str(), 2);
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  digitalWrite(LED_BUILTIN, HIGH);
  Serial.println("Disconnected from MQTT.");

  if (WiFi.isConnected()) {
    mqttReconnectTimer.once(2, connectToMqtt);
  }
}
// todo: end -> mqtt functions

// todo: parser functions
unsigned long parserAutoPreviousMillis = 0;
int parserAutoValueIndex = 0;
void parserEnableAuto() {
  parserAutoPreviousMillis = 0;
  parserAutoValueIndex = 0;
  deviceAutoEnable = 1;
}

void parserDisableAuto() {
  fx.setMode(deviceMode);
  deviceAutoEnable = 0;
}

void parser(String topic, String data) {
  // todo: auto mode
  if (topic == topicAuto) {
    int enable = data.toInt();
    if (deviceAutoEnable != enable) {
      deviceAutoEnable = enable;
      if (deviceAutoEnable == 1)
        parserEnableAuto();
      else
        parserDisableAuto();
    }
  }

  // todo: auto values
  if (topic == topicAutoValues) {
    if (deviceAutoValuesStr != data) {
      deviceAutoValuesStr = data;
      deserializeJson(docAutoValues, deviceAutoValuesStr);

      if (deviceAutoEnable == 1) parserEnableAuto();
    }
  }

  // todo: auto delay
  if (topic == topicAutoDelay) {
    int delay = data.toInt();
    if (deviceAutoDelay != delay) {
      deviceAutoDelay = delay;
    }
  }

  // todo: mode
  if (topic == topicMode) {
    int mode = data.toInt();
    if (deviceMode != mode) {
      fx.setMode(mode);
      deviceMode = mode;
    }
  }

  // todo: brightness
  if (topic == topicBrightness) {
    int brightness = data.toInt();
    if (deviceBrightness != brightness) {
      fx.setBrightness(brightness);
      deviceBrightness = brightness;
    }
  }

  // todo: speed
  if (topic == topicSpeed) {
    int speed = data.toInt();
    if (deviceSpeed != speed) {
      fx.setSpeed(speed);
      deviceSpeed = speed;
    }
  }

  // todo: color
  if (topic == topicColor) {
    // todo: sample data -> 0.0.0
    int a0 = data.indexOf(".");
    int a1 = data.lastIndexOf(".");
    int a2 = data.length();

    String rStr = data.substring(0, a0);
    String gStr = data.substring(a0 + 1, a1);
    String bStr = data.substring(a1 + 1, a2);

    int color = WS2812FX::Color(rStr.toInt(), gStr.toInt(), bStr.toInt());
    if (deviceColor != color) {
      fx.setColor(color);
      deviceColor = color;
    }
  }
}

void streamParser() {
  if (deviceAutoEnable == 1) {
    unsigned long currentMillis = millis();
    if (currentMillis - parserAutoPreviousMillis >=
        (unsigned long)deviceAutoDelay) {
      parserAutoPreviousMillis = currentMillis;

      if ((docAutoValues).containsKey("n")) {
        int count = (docAutoValues)["n"].as<int>();

        if (count > 0) {
          // todo: auto mode -> custom from user
          int currentAutoMode =
              (docAutoValues)["v"][parserAutoValueIndex].as<int>();

          fx.setMode(currentAutoMode);

          if (parserAutoValueIndex < (count - 1))
            parserAutoValueIndex++;
          else
            parserAutoValueIndex = 0;
        } else {
          fx.setMode(parserAutoValueIndex);

          if (parserAutoValueIndex < 55)
            parserAutoValueIndex++;
          else
            parserAutoValueIndex = 0;
        }
      } else {
        Serial.println("json doc -> null");
      }
    }
  }
}
// todo: end -> parser functions