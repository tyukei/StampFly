#include <WiFi.h>

// WiFi設定
const char* ssid = "OPPORenon";
const char* password = "bxyk6037";

void setup() {
  Serial.begin(115200);
  delay(3000);
  
  Serial.println("ESP32-S3 WiFi Test Starting...");
  Serial.print("Chip Model: ");
  Serial.println(ESP.getChipModel());
  Serial.print("Free Heap: ");
  Serial.println(ESP.getFreeHeap());
  
  // WiFi初期化
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  
  Serial.println("Scanning WiFi networks...");
  int n = WiFi.scanNetworks();
  Serial.print("Found ");
  Serial.print(n);
  Serial.println(" networks");
  
  for (int i = 0; i < n; ++i) {
    Serial.print(i + 1);
    Serial.print(": ");
    Serial.print(WiFi.SSID(i));
    Serial.print(" (");
    Serial.print(WiFi.RSSI(i));
    Serial.print(")");
    Serial.println();
  }
  
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(1000);
    Serial.print(".");
    Serial.print(WiFi.status());
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("");
    Serial.println("WiFi Connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("MAC Address: ");
    Serial.println(WiFi.macAddress());
  } else {
    Serial.println("");
    Serial.print("WiFi Failed! Status: ");
    Serial.println(WiFi.status());
  }
}

void loop() {
  Serial.print("WiFi Status: ");
  Serial.print(WiFi.status());
  Serial.print(", Free Heap: ");
  Serial.println(ESP.getFreeHeap());
  delay(5000);
}