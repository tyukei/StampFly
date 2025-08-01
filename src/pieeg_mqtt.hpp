#ifndef PIEEG_MQTT_HPP
#define PIEEG_MQTT_HPP

#include <PubSubClient.h>

// MQTT初期化
bool initMQTT(const char* mqtt_server, int port = 1883);

// MQTT接続
bool connectMQTT(const char* clientId);

// MQTTループ処理
void mqttLoop();

// データ送信
bool publishData(const char* topic, const char* data);
bool publishEEGData(float eegValue, unsigned long timestamp);
bool publishFlightData(float roll, float pitch, float yaw, float altitude);

// EEGデータ受信コールバック設定
void setEEGDataCallback(void (*callback)(float eegValue, unsigned long timestamp));

// グローバルMQTTクライアント
extern PubSubClient mqttClient;

#endif // PIEEG_MQTT_HPP