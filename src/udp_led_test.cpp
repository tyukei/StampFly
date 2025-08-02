#include <WiFi.h>
#include <WiFiUdp.h>

// WiFi設定
const char* ssid = "OPPORenon";
const char* password = "bxyk6037";

// UDP設定
WiFiUDP udp;
const int UDP_PORT = 4210;
char packetBuffer[255];

// LED設定（内蔵LED）
const int LED_PIN = 48;  // ESP32-S3 DevKit-C-1の内蔵RGB LED（WS2812）
// const int LED_PIN = 2;  // 別のピンも試してみる

void setup() {
  Serial.begin(115200);
  
  // LED設定
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  
  // 起動時にLED点滅で動作確認
  for(int i = 0; i < 5; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(200);
    digitalWrite(LED_PIN, LOW);
    delay(200);
  }
  
  // WiFi接続
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(LED_PIN, HIGH);
    delay(100);
    digitalWrite(LED_PIN, LOW);
    delay(100);
  }
  
  // 接続成功時に長点滅
  digitalWrite(LED_PIN, HIGH);
  delay(1000);
  digitalWrite(LED_PIN, LOW);
  
  // UDP開始
  udp.begin(UDP_PORT);
}

void loop() {
  // UDPパケット受信チェック
  int packetSize = udp.parsePacket();
  if (packetSize) {
    // パケット読み取り
    int len = udp.read(packetBuffer, 255);
    if (len > 0) {
      packetBuffer[len] = 0;
    }
    
    // 受信時にLED点灯（受信確認）
    digitalWrite(LED_PIN, HIGH);
    delay(100);
    digitalWrite(LED_PIN, LOW);
    
    // EEG値によって点滅パターンを変える
    float eegValue = atof(packetBuffer);
    int blinks = (int)(eegValue / 1.0) + 1;  // 1.0毎に点滅回数増加
    
    for(int i = 0; i < blinks && i < 5; i++) {
      delay(200);
      digitalWrite(LED_PIN, HIGH);
      delay(100);
      digitalWrite(LED_PIN, LOW);
    }
  }
  
  delay(10);
}