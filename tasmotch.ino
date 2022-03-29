#include "utils.h"
#include "config.h"

#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>

Output builtin_led{LED_BUILTIN, false};

const char* const base_unique_name = "tasmotch_";
char unique_name[64] = {0};

void init_unique_name() {
    Serial.print("MAC Address is ");
    Serial.println(WiFi.macAddress());
    // MAC Address format is AA:BB:CC:DD:EE:FF
    // We will use the last half as DDEEFF
    append_to_string(
        append_to_string(&unique_name[0], base_unique_name),
        WiFi.macAddress().c_str() + 9, ':');
    Serial.print("Name is ");
    Serial.println(&unique_name[0]);
}

WiFiClient wifi;

void setup_wifi() {
    delay(10);
    Serial.print("Wifi: Connecting to ");
    Serial.println(wifi_ssid);
    WiFi.begin(wifi_ssid, wifi_password);
    while (WiFi.status() != WL_CONNECTED) {
        builtin_led.flash(500);
        Serial.print(".");
    }
    Serial.println();
    Serial.print("WiFi connected - IP address: ");
    Serial.println(WiFi.localIP());
}

PubSubClient mqtt(wifi);

void mqtt_reconnect() {
    while (!mqtt.connected()) {
        Serial.print("Attempting MQTT connection...");
        if (mqtt.connect(unique_name)) {
            Serial.println("connected");
            mqtt.subscribe("tasmota/discovery/+/config");
        } else {
            builtin_led.set(true);
            Serial.print("failed, rc=");
            Serial.print(mqtt.state());
            Serial.println(" try again in 5 seconds");
            delay(5000);
            builtin_led.set(false);
        }
    }
}

const size_t max_json_size = 4096;

void mqtt_callback(char* topic, byte* payload, unsigned int length) {
    Serial.print("Received ");
    Serial.print(length);
    Serial.print(" byte message on ");
    Serial.println(topic);

    DynamicJsonDocument doc(max_json_size);
    DeserializationError error = deserializeJson(doc, payload);
    if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        return;
    }

    const char* const device_name = doc["dn"].as<const char*>();
    const char* const topic_name = doc["t"].as<const char*>();
    Serial.print("Got ");
    Serial.print(device_name);
    Serial.print(" ");
    Serial.println(topic_name);

    all_switches.found_device(device_name, topic_name);
}

void setup() {
    Serial.begin(9600);
    Serial.println();

    builtin_led.init();
    control_init();
    all_switches.init();
    init_unique_name();
    setup_wifi();

    builtin_led.flash(100, 4);

    mqtt.setServer(mqtt_broker_address, mqtt_broker_port);
    if (!mqtt.setBufferSize(max_json_size)) {
        Serial.println("Failed to set mqtt buffer");
    }
    mqtt.setCallback(mqtt_callback);

    builtin_led.flash(100, 4);
}

void loop() {
    if (!mqtt.connected()) {
        mqtt_reconnect();
        builtin_led.flash(100, 4);
    }
    mqtt.loop();

    all_switches.check(mqtt);
}
