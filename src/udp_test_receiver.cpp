#include <WiFi.h>
#include <WiFiUdp.h>

// WiFi設定
const char* ssid = "OPPORenon";
const char* password = "bxyk6037";

// UDP設定
WiFiUDP udp;
const int UDP_PORT = 4210;
char packetBuffer[255];

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("=== UDP Receiver Test ===");
  
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
  Serial.printf("UDP server listening on port %d\n", UDP_PORT);
  Serial.println("Waiting for data...");
}

void loop() {
  // UDPパケット受信チェック
  int packetSize = udp.parsePacket();
  if (packetSize) {
    Serial.printf("Packet received! Size: %d bytes\n", packetSize);
    
    // パケット読み取り
    int len = udp.read(packetBuffer, 255);
    if (len > 0) {
      packetBuffer[len] = 0;
    }
    
    // 送信者情報
    Serial.printf("From: %s:%d\n", udp.remoteIP().toString().c_str(), udp.remotePort());
    Serial.printf("Data: [%s]\n", packetBuffer);
    
    // EEG値として解析
    float eegValue = atof(packetBuffer);
    Serial.printf("Parsed as float: %.2f\n", eegValue);
    Serial.println("---");
  }
  
  delay(10);
}