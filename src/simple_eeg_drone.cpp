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

// モータPWM設定（ESC用：50Hz、1000-2000μsパルス幅）
const int freq = 50;        // 50Hz（20ms周期）
const int resolution = 12;  // 12bit分解能（ESP32-S3対応）
const int FrontLeft_motor  = 0;
const int FrontRight_motor = 1;
const int RearLeft_motor   = 2;
const int RearRight_motor  = 3;

// LEDピン（M5Stamp S3）
const int LED_PINS[] = {2, 8, 48};
const int NUM_LED_PINS = 3;

// EEG制御変数
float current_eeg_value = 0.0f;
unsigned long last_eeg_update = 0;
unsigned long motor_start_time = 0;  // モーター開始時刻
bool eeg_active = false;
bool motors_running = false;          // モーター稼働状態

// ドローン制御変数
float base_thrust = 0.0f;   // 基本推力（EEGデータ受信時のみ有効）
float yaw_rate = 0.0f;      // 回転速度

// ESC PWM制御（1000-2000μs、1500μsが停止）
void setMotorESC(int motor_channel, float throttle) {
    if (throttle < 0.0f) throttle = 0.0f;
    if (throttle > 1.0f) throttle = 1.0f;
    
    // 1000μs(停止) から 2000μs(最大) のパルス幅
    // 50Hz、12bit分解能で計算
    uint32_t pulse_min = (uint32_t)(4096 * 0.05);   // 1000μs / 20000μs = 0.05
    uint32_t pulse_max = (uint32_t)(4096 * 0.10);   // 2000μs / 20000μs = 0.10
    uint32_t pulse_width = pulse_min + (uint32_t)((pulse_max - pulse_min) * throttle);
    
    ledcWrite(motor_channel, pulse_width);
}

void stopAllMotors() {
    setMotorESC(FrontLeft_motor, 0.0f);
    setMotorESC(FrontRight_motor, 0.0f);
    setMotorESC(RearLeft_motor, 0.0f);
    setMotorESC(RearRight_motor, 0.0f);
}

// ESC初期化（アームシーケンス）
void initializeESCs() {
    Serial.println("Initializing ESCs...");
    
    // 最大スロットルで1秒（ESCキャリブレーション）
    for (int i = 0; i < 4; i++) {
        setMotorESC(i, 1.0f);
    }
    delay(1000);
    
    // 最小スロットルで1秒
    for (int i = 0; i < 4; i++) {
        setMotorESC(i, 0.0f);
    }
    delay(1000);
    
    Serial.println("ESC initialization complete");
}

// LED制御
void setLED(uint8_t r, uint8_t g, uint8_t b) {
    // 簡単なLED制御（M5Stamp S3のRGB LED）
    for (int i = 0; i < NUM_LED_PINS; i++) {
        digitalWrite(LED_PINS[i], (r > 127) ? HIGH : LOW);
    }
}

// EEG値に基づく回転制御（診断強化版）
void controlDroneWithEEG(float eegValue) {
    float thrust_fl, thrust_fr, thrust_rl, thrust_rr;
    
    // EEG値に基づく基本推力と回転速度決定
    if (eegValue < 0.5f) {
        base_thrust = 0.0f;     // 完全停止
        yaw_rate = 0.0f;        
        setLED(0, 0, 255);      // 青色LED
    }
    else if (eegValue < 1.5f) {
        base_thrust = 0.15f;    // 基本推力でゆっくり右回転
        yaw_rate = 0.05f;       
        setLED(0, 255, 0);      // 緑色LED
    }
    else if (eegValue < 2.5f) {
        base_thrust = 0.15f;    // 基本推力で中速右回転
        yaw_rate = 0.1f;        
        setLED(255, 0, 0);      // 赤色LED
    }
    else if (eegValue < 4.0f) {
        base_thrust = 0.15f;    // 基本推力で高速右回転
        yaw_rate = 0.15f;       
        setLED(255, 0, 255);    // マゼンタLED
    }
    else {
        base_thrust = 0.15f;    // 基本推力で左回転
        yaw_rate = -0.1f;       
        setLED(255, 255, 255);  // 白色LED
    }
    
    // 回転制御（Yaw軸回転）
    // 右回転：FR+, RL+ / FL-, RR-
    // 左回転：FL+, RR+ / FR-, RL-
    thrust_fl = base_thrust - yaw_rate;
    thrust_fr = base_thrust + yaw_rate;
    thrust_rl = base_thrust - yaw_rate;
    thrust_rr = base_thrust + yaw_rate;
    
    // 推力制限（ESC用）
    if (thrust_fl < 0.05f) thrust_fl = 0.05f;  // ESC最小値
    if (thrust_fr < 0.05f) thrust_fr = 0.05f;
    if (thrust_rl < 0.05f) thrust_rl = 0.05f;
    if (thrust_rr < 0.05f) thrust_rr = 0.05f;
    
    if (thrust_fl > 0.5f) thrust_fl = 0.5f;   // 安全のため最大50%
    if (thrust_fr > 0.5f) thrust_fr = 0.5f;
    if (thrust_rl > 0.5f) thrust_rl = 0.5f;
    if (thrust_rr > 0.5f) thrust_rr = 0.5f;
    
    // 15秒安全タイマーチェック
    if (motors_running && (millis() - motor_start_time > 15000)) {
        Serial.println("*** 15秒安全タイマー作動 - モーター強制停止 ***");
        stopAllMotors();
        motors_running = false;
        eeg_active = false;
        setLED(255, 165, 0); // オレンジ色で警告表示
        return;
    }
    
    // モータ制御（詳細ログ付き）
    if (eeg_active && base_thrust > 0.0f) {
        // モーター開始時刻を記録
        if (!motors_running) {
            motor_start_time = millis();
            motors_running = true;
            Serial.println("*** モーター開始 - 15秒後に自動停止 ***");
        }
        
        // PWM値を計算して表示（12bit分解能）
        uint32_t pulse_min = (uint32_t)(4096 * 0.05);
        uint32_t pulse_max = (uint32_t)(4096 * 0.10);
        
        uint32_t pwm_fl = pulse_min + (uint32_t)((pulse_max - pulse_min) * thrust_fl);
        uint32_t pwm_fr = pulse_min + (uint32_t)((pulse_max - pulse_min) * thrust_fr);
        uint32_t pwm_rl = pulse_min + (uint32_t)((pulse_max - pulse_min) * thrust_rl);
        uint32_t pwm_rr = pulse_min + (uint32_t)((pulse_max - pulse_min) * thrust_rr);
        
        setMotorESC(FrontLeft_motor, thrust_fl);
        setMotorESC(FrontRight_motor, thrust_fr);
        setMotorESC(RearLeft_motor, thrust_rl);
        setMotorESC(RearRight_motor, thrust_rr);
        
        unsigned long remaining_time = 15000 - (millis() - motor_start_time);
        Serial.printf("EEG: %.2f -> Yaw: %.2f (残り時間: %lums)\r\n", eegValue, yaw_rate, remaining_time);
        Serial.printf("Motors - FL: %.2f(PWM:%u) FR: %.2f(PWM:%u) RL: %.2f(PWM:%u) RR: %.2f(PWM:%u)\r\n", 
                     thrust_fl, pwm_fl, thrust_fr, pwm_fr, thrust_rl, pwm_rl, thrust_rr, pwm_rr);
        Serial.println("---");
    } else {
        if (motors_running) {
            motors_running = false;
            Serial.println("EEG停止 - モーター停止");
        }
        stopAllMotors();
    }
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    
    Serial.println("====================================");
    Serial.println("Simple EEG Drone Control System");
    Serial.println("====================================");
    
    // LED初期化
    for (int i = 0; i < NUM_LED_PINS; i++) {
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
    
    // 安全のため初期は停止
    stopAllMotors();
    
    // ESC初期化
    Serial.println("WARNING: ESC initialization will start motors!");
    Serial.println("Make sure propellers are REMOVED!");
    delay(3000);
    initializeESCs();
    
    // WiFi接続
    Serial.printf("Connecting to WiFi: %s\r\n", ssid);
    WiFi.begin(ssid, password);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println();
        Serial.println("WiFi Connected!");
        Serial.printf("IP Address: %s\r\n", WiFi.localIP().toString().c_str());
    } else {
        Serial.println("WiFi connection failed!");
        return;
    }
    
    // UDP開始
    if (udp.begin(UDP_PORT)) {
        Serial.printf("UDP server started on port: %d\r\n", UDP_PORT);
    }
    
    Serial.println("Simple EEG Drone Ready!");
    Serial.println("EEG Control Mapping:");
    Serial.println("0.0-0.5: Stop (Blue LED)");
    Serial.println("0.5-1.5: Slow Right Turn (Green LED)");
    Serial.println("1.5-2.5: Medium Right Turn (Red LED)");
    Serial.println("2.5-4.0: Fast Right Turn (Magenta LED)");
    Serial.println("4.0+: Left Turn (White LED)");
    Serial.println("-----------------------------------");
    Serial.println("WARNING: Motors will start when EEG data received!");
    Serial.println("Make sure propellers are REMOVED for testing!");
    
    // 起動時LED点滅
    for (int i = 0; i < 5; i++) {
        setLED(255, 255, 0); // 黄色
        delay(200);
        setLED(0, 0, 0);
        delay(200);
    }
}

void loop() {
    static unsigned long lastPrint = 0;
    static unsigned long packetCount = 0;
    
    // 5秒毎に状態表示
    if (millis() - lastPrint > 5000) {
        Serial.printf("Status - Packets: %lu, EEG: %.2f, Active: %s\r\n", 
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
        
        // 妥当性チェック
        if (newEegValue >= 0.0f && newEegValue <= 10.0f) {
            current_eeg_value = newEegValue;
            last_eeg_update = millis();
            eeg_active = true;
            
            controlDroneWithEEG(current_eeg_value);
        }
    }
    
    // EEGデータタイムアウト（3秒間データなしで停止）
    if (eeg_active && (millis() - last_eeg_update > 3000)) {
        Serial.println("EEG timeout - stopping motors");
        eeg_active = false;
        stopAllMotors();
        setLED(255, 165, 0); // オレンジ色でタイムアウト表示
    }
    
    delay(10);
}