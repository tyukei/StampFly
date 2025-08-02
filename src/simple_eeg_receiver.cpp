#include <WiFi.h>
#include <WiFiUdp.h>
#include "led.hpp"

// WiFi設定
const char* ssid = "OPPORenon";
const char* password = "bxyk6037";

// UDP設定
WiFiUDP udp;
const int UDP_PORT = 4210;
char packetBuffer[255];

void setup() {
  Serial.begin(115200);
  
  // LED初期化
  led_init();
  
  // WiFi接続
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Connected! IP: ");
  Serial.println(WiFi.localIP());
  
  // UDP開始
  udp.begin(UDP_PORT);
  Serial.printf("UDP server started on port %d\n", UDP_PORT);
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
    
    // EEG値を解析
    float eegValue = atof(packetBuffer);
    
    // LED色更新
    update_eeg_led(eegValue);
    
    Serial.printf("Received EEG: %.2f -> LED Color: 0x%06X\n", 
                  eegValue, get_eeg_color(eegValue));
  }
  
  delay(10);
}