#include "pieeg_mqtt.hpp"
#include <WiFi.h>
#include <ArduinoJson.h>

// WiFi認証情報
const char* ssid = "OPPORenon";
const char* password = "bxyk6037";

// MQTTクライアントインスタンス
WiFiClient espClient;
PubSubClient mqttClient(espClient);

// EEGデータコールバック（外部から設定可能）
void (*eegDataCallback)(float, unsigned long) = nullptr;

// コールバック関数（受信用）
void mqttCallback(char* topic, byte* payload, unsigned int length) {
    String message = "";
    for (int i = 0; i < length; i++) {
        message += (char)payload[i];
    }
    
    // JSON解析
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, message);
    
    if (!error) {
        float eegValue = doc["eeg"];
        unsigned long timestamp = doc["time"];
        
        // EEGデータ処理コールバック実行
        if (eegDataCallback != nullptr) {
            eegDataCallback(eegValue, timestamp);
        }
        
        Serial.print("EEG受信: ");
        Serial.print(eegValue);
        Serial.print(" @ ");
        Serial.println(timestamp);
    }
}

void setEEGDataCallback(void (*callback)(float, unsigned long)) {
    eegDataCallback = callback;
}

bool initMQTT(const char* mqtt_server, int port) {
    // WiFi接続
    USBSerial.printf("WiFi connecting to: %s\r\n", ssid);
    WiFi.begin(ssid, password);
    USBSerial.printf("WiFi接続中");
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        USBSerial.printf(".");
        attempts++;
    }
    
    if (WiFi.status() != WL_CONNECTED) {
        USBSerial.printf("\nWiFi接続失敗 - Status: %d\r\n", WiFi.status());
        return false;
    }
    
    USBSerial.printf("\nWiFi接続完了\r\n");
    USBSerial.printf("IPアドレス: %s\r\n", WiFi.localIP().toString().c_str());
    USBSerial.printf("サブネットマスク: %s\r\n", WiFi.subnetMask().toString().c_str());
    USBSerial.printf("ゲートウェイ: %s\r\n", WiFi.gatewayIP().toString().c_str());
    USBSerial.printf("DNS: %s\r\n", WiFi.dnsIP().toString().c_str());
    
    // MQTT設定
    USBSerial.printf("Setting MQTT server: %s:%d\r\n", mqtt_server, port);
    mqttClient.setServer(mqtt_server, port);
    mqttClient.setCallback(mqttCallback);
    
    // MQTT接続テスト
    USBSerial.printf("Testing MQTT connection...\r\n");
    if (mqttClient.connect("StampS3_TestClient")) {
        USBSerial.printf("MQTT connection successful!\r\n");
        mqttClient.subscribe("pieeg/data");
        USBSerial.printf("Subscribed to pieeg/data\r\n");
        return true;
    } else {
        USBSerial.printf("MQTT connection failed, state: %d\r\n", mqttClient.state());
        return false;
    }
}

bool connectMQTT(const char* clientId) {
    if (!mqttClient.connected()) {
        Serial.print("MQTT接続中...");
        if (mqttClient.connect(clientId)) {
            Serial.println("接続成功");
            // トピックをサブスクライブ
            mqttClient.subscribe("pieeg/data");
            mqttClient.subscribe("pieeg/control");
            return true;
        } else {
            Serial.print("失敗, rc=");
            Serial.println(mqttClient.state());
            return false;
        }
    }
    return true;
}

void mqttLoop() {
    if (!mqttClient.connected()) {
        connectMQTT("M5Fly_Client");
    }
    mqttClient.loop();
}

bool publishData(const char* topic, const char* data) {
    if (!mqttClient.connected()) {
        return false;
    }
    return mqttClient.publish(topic, data);
}

bool publishEEGData(float eegValue, unsigned long timestamp) {
    if (!mqttClient.connected()) {
        return false;
    }
    
    // JSON形式でデータ作成
    StaticJsonDocument<200> doc;
    doc["eeg"] = eegValue;
    doc["time"] = timestamp;
    
    char buffer[256];
    serializeJson(doc, buffer);
    
    return mqttClient.publish("pieeg/data", buffer);
}

bool publishFlightData(float roll, float pitch, float yaw, float altitude) {
    if (!mqttClient.connected()) {
        return false;
    }
    
    // JSON形式でデータ作成
    StaticJsonDocument<200> doc;
    doc["roll"] = roll;
    doc["pitch"] = pitch;
    doc["yaw"] = yaw;
    doc["alt"] = altitude;
    doc["time"] = millis();
    
    char buffer[256];
    serializeJson(doc, buffer);
    
    return mqttClient.publish("m5fly/telemetry", buffer);
}