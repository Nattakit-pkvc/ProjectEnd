#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ESP8266HTTPClient.h>
#include <TinyGPS++.h>
#include <SoftwareSerial.h>
#include <Wire.h>
#include <U8g2lib.h>

// ================= 1. ตั้งค่าจอ OLED (SH1106) =================
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE, /* clock=*/ D5, /* data=*/ D4);

// ================= 2. ตั้งค่า WIFI =================
const char* ssid     = "TECNO POVA 6 Pro 5G"; 
const char* password = "14092544";

// ================= 3. ตั้งค่า API & LINE =================
const char* lineHost = "api.line.me";
const char* accessToken = "LSmXROaNyWBDB6CXvsrQnXSE3vgX/oObcIaxGwSkrwS4V2V24IAl0Eusz2ZM3fiX0qcw5ifzMh8NfnPzviOUFc66UgdDDD+CmThQZH1kjmbH5DiQScPY2wa19CrOxgRE9sfMgeFBcCLb0G/uzjEHJgdB04t89/1O/w1cDnyilFU=LSmXROaNyWBDB6CXvsrQnXSE3vgX/oObcIaxGwSkrwS4V2V24IAl0Eusz2ZM3fiX0qcw5ifzMh8NfnPzviOUFc66UgdDDD+CmThQZH1kjmbH5DiQScPY2wa19CrOxgRE9sfMgeFBcCLb0G/uzjEHJgdB04t89/1O/w1cDnyilFU=";
String targetID = "Cf1f5aefc45f33c82d8bc303aa984fdef"; 

// URL เว็บใหม่ของคุณ
const char* gasApiUrl   = "https://my-gas.vercel.app/api/gas"; 
const char* flameApiUrl = "https://my-gas.vercel.app/api/flame";

// ================= 4. ตั้งค่า ขาอุปกรณ์ =================
int gasPin = A0;      
int flamePin = D1;    
int relayPin = D2;    
int buzzerPin = D3;   
int gpsRxPin = D6;    
int gpsTxPin = D7;    

TinyGPSPlus gps;
SoftwareSerial gpsSerial(gpsRxPin, gpsTxPin);

int gasVal = 0;       
int flameState = HIGH; 
int gasThreshold = 500; 

// ตัวแปร Logic
bool lastGasCritical = false;
bool lastFlameCritical = false;
bool isGasAlertSent = false;
bool isFireAlertSent = false;
unsigned long lastApiTime = 0;
long apiInterval = 30000; // ✅ ตั้งค่าเริ่มต้นเป็น 30 วินาที

// ประกาศฟังก์ชัน
void sendLinePush(String message);
void sendGasToAPI();
void sendFlameToAPI();
String getGoogleMapLink();
void updateOLED(); 

void setup() {
  Serial.begin(115200);
  gpsSerial.begin(9600); 
  
  u8g2.begin();
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(0, 15, "System Starting...");
  u8g2.drawStr(0, 35, "Interval: 30s"); // แจ้งบนจอว่าส่งทุก 30 วิ
  u8g2.sendBuffer();
  
  pinMode(flamePin, INPUT);
  pinMode(relayPin, OUTPUT);
  pinMode(buzzerPin, OUTPUT); 
  
  digitalWrite(relayPin, HIGH); 
  noTone(buzzerPin); 

  Serial.println("\n--- Starting System (30s Interval) ---");
  Serial.println("Connecting WiFi...");
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("\n✅ WiFi Connected!");
  
  // ส่งครั้งแรกทันทีที่เปิดเครื่อง
  sendGasToAPI();
  sendFlameToAPI();
}

void loop() {
  while (gpsSerial.available() > 0) gps.encode(gpsSerial.read());
  
  gasVal = analogRead(gasPin);
  flameState = digitalRead(flamePin);

  bool currentGasCritical = (gasVal > gasThreshold);
  bool currentFlameCritical = (flameState == LOW);

  // --- FAST ALERT SYSTEM ---
  bool statusChanged = (currentGasCritical != lastGasCritical) || (currentFlameCritical != lastFlameCritical);
  bool isEmergency = (currentGasCritical || currentFlameCritical);
  
  // ✅ ถ้าฉุกเฉินส่งทุก 2 วิ, ถ้าปกติส่งทุก 30 วิ
  apiInterval = isEmergency ? 2000 : 30000; 

  // เงื่อนไข: ถ้าสถานะเปลี่ยน (เช่น จู่ๆ ไฟไหม้) ให้ส่งทันที หรือ ถ้าครบเวลา (30วิ) ก็ส่ง
  if (statusChanged || (millis() - lastApiTime >= apiInterval)) {
      sendGasToAPI();
      sendFlameToAPI();
      lastApiTime = millis();
      lastGasCritical = currentGasCritical;
      lastFlameCritical = currentFlameCritical;
  }

  // --- Update OLED ---
  updateOLED();

  // --- Hardware Control ---
  if (isEmergency) {
    digitalWrite(relayPin, LOW);   
    tone(buzzerPin, 2000);         
  } else {
    digitalWrite(relayPin, HIGH);  
    noTone(buzzerPin);             
  }

  // --- LINE Notify ---
  if (currentGasCritical) {
    if (!isGasAlertSent) { 
      String mapLink = getGoogleMapLink();
      sendLinePush("⚠️ อันตราย! แก๊สรั่ว (" + String(gasVal) + ") " + mapLink); 
      isGasAlertSent = true; 
    }
  } else { isGasAlertSent = false; }

  if (currentFlameCritical) {
    if (!isFireAlertSent) { 
      String mapLink = getGoogleMapLink();
      sendLinePush("🔥 ไฟไหม้! ตรวจพบเปลวไฟ! " + mapLink); 
      isFireAlertSent = true; 
    }
  } else { isFireAlertSent = false; }

  delay(10); 
}

// ================= ฟังก์ชันอัปเดตจอ (U8g2) =================
void updateOLED() {
  u8g2.clearBuffer();          
  u8g2.setFont(u8g2_font_6x10_tf); 

  u8g2.drawStr(0, 10, "SAFETY MONITOR");
  u8g2.drawLine(0, 12, 128, 12); 

  u8g2.setCursor(95, 10);
  if(WiFi.status() == WL_CONNECTED) u8g2.print("(WF)"); else u8g2.print("(--)");

  u8g2.setCursor(0, 25);
  u8g2.print("Gas: "); u8g2.print(gasVal);
  if(gasVal > gasThreshold) u8g2.print(" [!]");

  u8g2.setCursor(0, 37);
  u8g2.print("Fire: ");
  if(flameState == LOW) u8g2.print("DETECTED!"); else u8g2.print("Safe");

  u8g2.setCursor(0, 49);
  u8g2.print("Sats: "); u8g2.print(gps.satellites.value());
  
  u8g2.setCursor(0, 61);
  if(gps.location.isValid()) {
    u8g2.print(gps.location.lat(), 5);
    u8g2.print(",");
    u8g2.print(gps.location.lng(), 5);
  } else {
    u8g2.print("Searching GPS...");
  }

  u8g2.sendBuffer(); 
}

// ================= ฟังก์ชันส่ง API =================
void sendGasToAPI() {
  if (WiFi.status() != WL_CONNECTED) return;
  WiFiClientSecure client; client.setInsecure(); client.setTimeout(5000);
  HTTPClient http;
  
  if (http.begin(client, gasApiUrl)) {
    http.addHeader("Content-Type", "application/json");
    String gasState = (gasVal > gasThreshold) ? "DANGER" : "SAFE";
    String payload = "{\"gas_val\":" + String(gasVal) + ",\"gas_state\":\"" + gasState + "\"}";
    
    int httpCode = http.POST(payload);
    if (httpCode > 0) {
      Serial.printf("✅ Gas API: %d\n", httpCode);
    } else {
      Serial.printf("❌ Gas Error: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
  }
}

void sendFlameToAPI() {
  if (WiFi.status() != WL_CONNECTED) return;
  WiFiClientSecure client; client.setInsecure(); client.setTimeout(5000);
  HTTPClient http;
  
  if (http.begin(client, flameApiUrl)) {
    http.addHeader("Content-Type", "application/json");
    String fStatus = (flameState == LOW) ? "FIRE DETECTED" : "NORMAL";
    String payload = "{\"flame_status\":\"" + fStatus + "\"}";
    
    int httpCode = http.POST(payload);
    if (httpCode > 0) {
      Serial.printf("✅ Flame API: %d\n", httpCode);
    } else {
      Serial.printf("❌ Flame Error: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
  }
}

// ================= ฟังก์ชันเสริม =================
String getGoogleMapLink() {
  if (gps.location.isValid()) {
    return "Map: https://maps.google.com/?q=" + String(gps.location.lat(), 6) + "," + String(gps.location.lng(), 6);
  } else {
    return "Map: (Searching GPS...)";
  }
}

void sendLinePush(String message) {
  WiFiClientSecure client; 
  client.setInsecure(); 
  if (!client.connect(lineHost, 443)) return;
  String payload = "{\"to\":\"" + targetID + "\",\"messages\":[{\"type\":\"text\",\"text\":\"" + message + "\"}]}";
  client.println("POST /v2/bot/message/push HTTP/1.1");
  client.println("Host: " + String(lineHost));
  client.println("Authorization: Bearer " + String(accessToken));
  client.println("Content-Type: application/json");
  client.println("Content-Length: " + String(payload.length()));
  client.println(); 
  client.print(payload);
}