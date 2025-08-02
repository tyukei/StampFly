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

// M5Stamp S3のLED設定を複数試行
const int LED_PINS[] = {2, 8, 48, 21, 47};  // 可能性のあるピン
const int NUM_PINS = 5;

void setLED(int brightness) {
  for (int i = 0; i < NUM_PINS; i++) {
    digitalWrite(LED_PINS[i], brightness > 127 ? HIGH : LOW);
    ledcSetup(i, 5000, 8);
    ledcAttachPin(LED_PINS[i], i);
    ledcWrite(i, brightness);
  }
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("======================");
  Serial.println("EEG-LED Simple Test");
  Serial.println("======================");
  
  // 全ピンを出力として設定
  for (int i = 0; i < NUM_PINS; i++) {
    pinMode(LED_PINS[i], OUTPUT);
    Serial.print("Set pin ");
    Serial.print(LED_PINS[i]);
    Serial.println(" as OUTPUT");
  }
  
  // LED点滅テスト
  Serial.println("LED Test: Blinking all pins...");
  for (int j = 0; j < 5; j++) {
    setLED(255);  // ON
    delay(200);
    setLED(0);    // OFF
    delay(200);
    Serial.print("Blink ");
    Serial.println(j + 1);
  }
  
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
  
  Serial.println("Ready! Send EEG data...");
  setLED(128);  // 中間の明るさで待機
}

// 重複定義を削除

void loop() {
  static unsigned long lastPrint = 0;
  static unsigned long packetCount = 0;
  
  // 10秒毎に状態とLEDテスト
  if (millis() - lastPrint > 10000) {
    Serial.print("Running... Packets: ");
    Serial.println(packetCount);
    
    // 定期的なLEDテスト
    setLED(255);
    delay(100);
    setLED(0);
    delay(100);
    setLED(128);  // 元に戻す
    
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
    
    float eegValue = atof(packetBuffer);
    
    // EEG値に応じたLED明度制御
    int brightness = (int)(eegValue * 51);  // 0-5 -> 0-255
    if (brightness > 255) brightness = 255;
    
    setLED(brightness);
    
    Serial.print("EEG: ");
    Serial.print(eegValue, 2);
    Serial.print(" -> LED Brightness: ");
    Serial.println(brightness);
  }
  
  delay(10);
}