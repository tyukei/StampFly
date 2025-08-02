#include <WiFi.h>
#include <WiFiUdp.h>

// WiFi設定
const char* ssid = "OPPORenon";
const char* password = "bxyk6037";

// UDP設定
WiFiUDP udp;
const int UDP_PORT = 4210;
char packetBuffer[255];

// M5Stamp S3のRGB LED設定
const int RED_PIN = 2;
const int GREEN_PIN = 8;
const int BLUE_PIN = 48;

void setRGB(int r, int g, int b) {
  // PWM設定 (0チャンネル:赤, 1チャンネル:緑, 2チャンネル:青)
  ledcSetup(0, 5000, 8);  // 赤
  ledcSetup(1, 5000, 8);  // 緑
  ledcSetup(2, 5000, 8);  // 青
  
  ledcAttachPin(RED_PIN, 0);
  ledcAttachPin(GREEN_PIN, 1);
  ledcAttachPin(BLUE_PIN, 2);
  
  ledcWrite(0, r);
  ledcWrite(1, g);
  ledcWrite(2, b);
}

// 点滅制御用変数
unsigned long lastBlinkChange = 0;
bool ledState = false;
int blinkInterval = 1000;  // デフォルト1秒

void setEEGBlink(float eegValue) {
  // EEG値に応じて点滅速度を設定（より大きな差をつける）
  if (eegValue < 0.5f) {
    blinkInterval = 3000;   // 3秒間隔 - 低集中
    Serial.println("LED: Very Slow Blink (Low Focus)");
  }
  else if (eegValue < 1.5f) {
    blinkInterval = 1500;   // 1.5秒間隔 - 中集中
    Serial.println("LED: Slow Blink (Mid Focus)");
  }
  else if (eegValue < 2.5f) {
    blinkInterval = 600;    // 0.6秒間隔 - 高集中
    Serial.println("LED: Medium Blink (High Focus)");
  }
  else if (eegValue < 4.0f) {
    blinkInterval = 200;    // 0.2秒間隔 - 超高集中
    Serial.println("LED: Fast Blink (Very High Focus)");
  }
  else {
    blinkInterval = 80;     // 0.08秒間隔 - 最高集中
    Serial.println("LED: Ultra Fast Blink (Max Focus)");
  }
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("======================");
  Serial.println("EEG-LED Color Controller");
  Serial.println("======================");
  
  // ピン設定
  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);
  
  // 起動時カラーテスト
  Serial.println("Color Test Starting...");
  
  Serial.println("Red");
  setRGB(255, 0, 0);
  delay(1000);
  
  Serial.println("Green");
  setRGB(0, 255, 0);
  delay(1000);
  
  Serial.println("Blue");
  setRGB(0, 0, 255);
  delay(1000);
  
  Serial.println("Magenta");
  setRGB(255, 0, 255);
  delay(1000);
  
  Serial.println("White");
  setRGB(255, 255, 255);
  delay(1000);
  
  Serial.println("Off");
  setRGB(0, 0, 0);
  delay(500);
  
  // WiFi接続
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println();
  Serial.println("WiFi Connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  
  // UDP開始
  if (udp.begin(UDP_PORT)) {
    Serial.print("UDP server started on port: ");
    Serial.println(UDP_PORT);
  }
  
  Serial.println("Ready for EEG data!");
  Serial.println("EEG Blink Ranges:");
  Serial.println("0.0-0.5: 3s interval (Very Slow)");
  Serial.println("0.5-1.5: 1.5s interval (Slow)");
  Serial.println("1.5-2.5: 0.6s interval (Medium)");
  Serial.println("2.5-4.0: 0.2s interval (Fast)");
  Serial.println("4.0+: 0.08s interval (Ultra Fast)");
  
  setRGB(50, 50, 50);  // 薄い白で待機
}

void loop() {
  static unsigned long lastPrint = 0;
  static unsigned long packetCount = 0;
  static unsigned long lastEEGChange = 0;
  static float currentEEG = 0.0;
  static float targetEEG = 0.0;
  static bool hasNewEEG = false;
  
  // 10秒毎に状態表示
  if (millis() - lastPrint > 10000) {
    Serial.print("Running... Packets received: ");
    Serial.println(packetCount);
    lastPrint = millis();
  }
  
  // UDPパケット受信チェック
  int packetSize = udp.parsePacket();
  if (packetSize) {
    packetCount++;
    
    int len = udp.read(packetBuffer, 255);
    if (len > 0) {
      packetBuffer[len] = 0;
    }
    
    targetEEG = atof(packetBuffer);
    hasNewEEG = true;
    
    Serial.print("New EEG Value: ");
    Serial.println(targetEEG, 2);
  }
  
  // 即座にEEG値を更新
  if (hasNewEEG) {
    currentEEG = targetEEG;
    setEEGBlink(currentEEG);
    lastEEGChange = millis();
    lastBlinkChange = millis(); // 点滅タイミングもリセット
    hasNewEEG = false;
    
    Serial.print("Updated EEG Value: ");
    Serial.print(currentEEG, 2);
    Serial.print(" -> Blink interval: ");
    Serial.print(blinkInterval);
    Serial.println("ms");
  }
  
  // 点滅制御
  if (millis() - lastBlinkChange >= blinkInterval) {
    ledState = !ledState;
    if (ledState) {
      setRGB(255, 255, 255);  // 白で点滅
    } else {
      setRGB(0, 0, 0);        // 消灯
    }
    lastBlinkChange = millis();
  }
  
  delay(10);
}