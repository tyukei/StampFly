#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>

// WiFi設定（.envから読み込み）
#ifndef WIFI_SSID
#define WIFI_SSID "default_ssid"
#endif
#ifndef WIFI_PASSWORD  
#define WIFI_PASSWORD "default_password"
#endif

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

// UDP設定
WiFiUDP udp;
const int UDP_PORT = 4210;
char packetBuffer[255];

// モータPWM出力ピン
const int pwmFrontLeft  = 5;
const int pwmFrontRight = 42;
const int pwmRearLeft   = 10;
const int pwmRearRight  = 41;

// モータPWM設定
const int freq = 50;
const int resolution = 12;
const int FrontLeft_motor  = 0;
const int FrontRight_motor = 1;
const int RearLeft_motor   = 2;
const int RearRight_motor  = 3;

// LEDピン
const int LED_PINS[] = {2, 8, 48};

// EEG制御変数
float current_eeg_value = 0.0f;
unsigned long last_eeg_update = 0;
unsigned long motor_start_time = 0;
bool eeg_active = false;
bool motors_running = false;

// 基本推力設定（浮上可能レベル）
const float BASE_THRUST = 0.35f;  // 基本推力（浮上可能レベル）

// ESC PWM制御
void setMotorESC(int motor_channel, float throttle) {
    if (throttle < 0.05f) throttle = 0.05f;  // ESC最小値
    if (throttle > 0.5f) throttle = 0.5f;    // 安全最大値
    
    uint32_t pulse_min = (uint32_t)(4096 * 0.05);
    uint32_t pulse_max = (uint32_t)(4096 * 0.10);
    uint32_t pulse_width = pulse_min + (uint32_t)((pulse_max - pulse_min) * throttle);
    
    ledcWrite(motor_channel, pulse_width);
}

// LED制御
void setLED(uint8_t r, uint8_t g, uint8_t b) {
    for (int i = 0; i < 3; i++) {
        digitalWrite(LED_PINS[i], (r > 127) ? HIGH : LOW);
    }
}

// EEG値に基づく回転速度制御
void controlRotationWithEEG(float eegValue) {
    float thrust_fl, thrust_fr, thrust_rl, thrust_rr;
    float yaw_rate;
    
    // EEG値を回転速度にマッピング（大幅な推力差で明確な制御）
    if (eegValue < 1.0f) {
        yaw_rate = -0.20f;      // 強い左回転
        setLED(255, 0, 255);    // マゼンタ
    }
    else if (eegValue < 2.0f) {
        yaw_rate = -0.10f;      // 弱い左回転
        setLED(0, 0, 255);      // 青
    }
    else if (eegValue < 3.0f) {
        yaw_rate = 0.0f;        // 直進浮上
        setLED(0, 255, 0);      // 緑
    }
    else if (eegValue < 4.0f) {
        yaw_rate = 0.10f;       // 弱い右回転
        setLED(255, 255, 0);    // 黄色
    }
    else {
        yaw_rate = 0.20f;       // 強い右回転
        setLED(255, 0, 0);      // 赤
    }
    
    // 15秒安全タイマー
    if (motors_running && (millis() - motor_start_time > 15000)) {
        Serial.println("*** 15秒タイマー - 基本回転に戻る ***");
        yaw_rate = 0.0f;  // 直進回転に戻る
        setLED(255, 165, 0); // オレンジ
    }
    
    // 回転制御計算
    thrust_fl = BASE_THRUST - yaw_rate;
    thrust_fr = BASE_THRUST + yaw_rate;
    thrust_rl = BASE_THRUST - yaw_rate;
    thrust_rr = BASE_THRUST + yaw_rate;
    
    // モーター制御
    if (eeg_active) {
        if (!motors_running) {
            motor_start_time = millis();
            motors_running = true;
            Serial.println("*** EEG制御開始 - 15秒間有効 ***");
        }
        
        setMotorESC(FrontLeft_motor, thrust_fl);
        setMotorESC(FrontRight_motor, thrust_fr);
        setMotorESC(RearLeft_motor, thrust_rl);
        setMotorESC(RearRight_motor, thrust_rr);
        
        unsigned long remaining = 15000 - (millis() - motor_start_time);
        Serial.printf("EEG: %.2f -> Yaw: %.3f | FL:%.2f FR:%.2f RL:%.2f RR:%.2f | 残り:%lums\n", 
                     eegValue, yaw_rate, thrust_fl, thrust_fr, thrust_rl, thrust_rr, remaining);
    } else {
        // EEGデータなし - 基本回転を維持
        setMotorESC(FrontLeft_motor, BASE_THRUST);
        setMotorESC(FrontRight_motor, BASE_THRUST);
        setMotorESC(RearLeft_motor, BASE_THRUST);
        setMotorESC(RearRight_motor, BASE_THRUST);
        
        if (motors_running) {
            motors_running = false;
            Serial.println("EEGデータなし - 基本回転に戻る");
        }
        setLED(100, 100, 100); // グレー
    }
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    
    Serial.println("======================================");
    Serial.println("EEG Rotation Control System");
    Serial.println("======================================");
    
    // LED初期化
    for (int i = 0; i < 3; i++) {
        pinMode(LED_PINS[i], OUTPUT);
        digitalWrite(LED_PINS[i], LOW);
    }
    
    // PWM初期化
    ledcSetup(FrontLeft_motor, freq, resolution);
    ledcSetup(FrontRight_motor, freq, resolution);
    ledcSetup(RearLeft_motor, freq, resolution);
    ledcSetup(RearRight_motor, freq, resolution);
    
    ledcAttachPin(pwmFrontLeft, FrontLeft_motor);
    ledcAttachPin(pwmFrontRight, FrontRight_motor);
    ledcAttachPin(pwmRearLeft, RearLeft_motor);
    ledcAttachPin(pwmRearRight, RearRight_motor);
    
    // 基本回転で開始
    Serial.printf("基本推力で回転開始: %.2f\n", BASE_THRUST);
    for (int i = 0; i < 4; i++) {
        setMotorESC(i, BASE_THRUST);
    }
    
    // WiFi接続
    Serial.printf("WiFi接続中: %s\n", ssid);
    WiFi.begin(ssid, password);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi接続成功!");
        Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
        
        if (udp.begin(UDP_PORT)) {
            Serial.printf("UDP server started on port: %d\n", UDP_PORT);
        }
    }
    
    Serial.println("\n=== EEG Rotation Control Ready ===");
    Serial.println("EGG値マッピング (浮上可能レベル):");
    Serial.println("0.0-1.0: 強い左回転 (マゼンタ) - 推力差40%");
    Serial.println("1.0-2.0: 弱い左回転 (青) - 推力差20%");
    Serial.println("2.0-3.0: 直進浮上 (緑) - 均等推力35%");
    Serial.println("3.0-4.0: 弱い右回転 (黄) - 推力差20%");
    Serial.println("4.0+: 強い右回転 (赤) - 推力差40%");
    Serial.println("=====================================");
    
    // 起動LED表示
    for (int i = 0; i < 3; i++) {
        setLED(0, 255, 255);
        delay(300);
        setLED(0, 0, 0);
        delay(300);
    }
}

void loop() {
    static unsigned long lastPrint = 0;
    static unsigned long packetCount = 0;
    
    // 5秒毎に状態表示
    if (millis() - lastPrint > 5000) {
        Serial.printf("Status - Packets: %lu, EEG: %.2f, Active: %s\n", 
                     packetCount, current_eeg_value, eeg_active ? "YES" : "NO");
        lastPrint = millis();
    }
    
    // UDPパケット受信
    int packetSize = udp.parsePacket();
    if (packetSize) {
        packetCount++;
        
        int len = udp.read(packetBuffer, 255);
        if (len > 0) {
            packetBuffer[len] = 0;
        }
        
        float newEegValue = atof(packetBuffer);
        
        if (newEegValue >= 0.0f && newEegValue <= 10.0f) {
            current_eeg_value = newEegValue;
            last_eeg_update = millis();
            eeg_active = true;
            
            controlRotationWithEEG(current_eeg_value);
        }
    }
    
    // EEGデータタイムアウト（3秒）
    if (eeg_active && (millis() - last_eeg_update > 3000)) {
        Serial.println("EEGタイムアウト - 基本回転に戻る");
        eeg_active = false;
        controlRotationWithEEG(0.0f); // 基本状態で呼び出し
    }
    
    delay(10);
}