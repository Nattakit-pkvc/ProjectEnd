#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ESP8266HTTPClient.h>
#include <TinyGPS++.h>
#include <SoftwareSerial.h>
#include <Wire.h>
// เรียกใช้ไลบรารี U8g2 (พระเอกของเรา)
#include <U8g2lib.h>

// ================= 1. ตั้งค่าจอ OLED (SH1106) =================
// ใช้ Driver แบบที่ 2 ที่คุณทดสอบผ่าน
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE, /* clock=*/ D5, /* data=*/ D4);

// ================= 2. ตั้งค่า WIFI =================
const char* ssid     = "TECNO POVA 6 Pro 5G";
const char* password = "14092544";

// ================= 3. ตั้งค่า LINE & API =================
const char* lineHost = "api.line.me";
const char* accessToken = "LSmXROaNyWBDB6CXvsrQnXSE3vgX/oObcIaxGwSkrwS4V2V24IAl0Eusz2ZM3fiX0qcw5ifzMh8NfnPzviOUFc66UgdDDD+CmThQZH1kjmbH5DiQScPY2wa19CrOxgRE9sfMgeFBcCLb0G/uzjEHJgdB04t89/1O/w1cDnyilFU=LSmXROaNyWBDB6CXvsrQnXSE3vgX/oObcIaxGwSkrwS4V2V24IAl0Eusz2ZM3fiX0qcw5ifzMh8NfnPzviOUFc66UgdDDD+CmThQZH1kjmbH5DiQScPY2wa19CrOxgRE9sfMgeFBcCLb0G/uzjEHJgdB04t89/1O/w1cDnyilFU=";
String targetID = "Cf1f5aefc45f33c82d8bc303aa984fdef"; 
const char* gasApiUrl   = "https://gas-hee.vercel.app/api/gas";
const char* flameApiUrl = "https://gas-hee.vercel.app/api/flame";

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
bool isGasAlertSent = false;
bool isFireAlertSent = false;
unsigned long lastApiTime = 0;
long apiInterval = 10000; 

// Prototype
void sendLinePush(String message);
void sendGasToAPI();
void sendFlameToAPI();
String getGoogleMapLink();
void updateOLED(); 

void setup() {
  Serial.begin(115200);
  gpsSerial.begin(9600); 
  
  // เริ่มต้นจอ U8g2
  u8g2.begin();
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr); // เลือกฟอนต์สวยๆ
  u8g2.drawStr(0, 15, "System Starting...");
  u8g2.drawStr(0, 35, "Driver: SH1106");
  u8g2.sendBuffer();
  
  pinMode(flamePin, INPUT);
  pinMode(relayPin, OUTPUT);
  pinMode(buzzerPin, OUTPUT); 
  digitalWrite(relayPin, HIGH); 
  noTone(buzzerPin); 

  Serial.println("\nConnecting WiFi...");
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("\nWiFi Connected!");
  // แจ้งเตือนเริ่มต้น
  sendLinePush("System Ready: SH1106 OLED + GPS Mode");
}

void loop() {
  // 1. อ่าน GPS
  while (gpsSerial.available() > 0) gps.encode(gpsSerial.read());
  
  // 2. อ่านเซ็นเซอร์
  gasVal = analogRead(gasPin);
  flameState = digitalRead(flamePin);

  // 3. อัปเดตหน้าจอ (ด้วยฟังก์ชันใหม่ของ U8g2)
  updateOLED();

  // 4. Logic แจ้งเตือน (เสียงยาว)
  if (gasVal > gasThreshold || flameState == LOW) {
    digitalWrite(relayPin, LOW);   // เปิดพัดลม
    tone(buzzerPin, 2000);         // เสียงยาว
  } else {
    digitalWrite(relayPin, HIGH);  // ปิดพัดลม
    noTone(buzzerPin);             // เงียบ
  }

  // 5. ส่ง LINE (ไม่มี \n)
  if (gasVal > gasThreshold) {
    if (!isGasAlertSent) { 
      String mapLink = getGoogleMapLink();
      sendLinePush("⚠️ อันตราย! แก๊สรั่ว (" + String(gasVal) + ") " + mapLink); 
      isGasAlertSent = true; 
    }
  } else { isGasAlertSent = false; }

  if (flameState == LOW) {
    if (!isFireAlertSent) { 
      String mapLink = getGoogleMapLink();
      sendLinePush("🔥 ไฟไหม้! ตรวจพบเปลวไฟ! " + mapLink); 
      isFireAlertSent = true; 
    }
  } else { isFireAlertSent = false; }

  // 6. ส่ง API
  bool isEmergency = (gasVal > gasThreshold || flameState == LOW);
  apiInterval = isEmergency ? 1000 : 10000; 
  unsigned long currentMillis = millis();
  if (currentMillis - lastApiTime >= apiInterval) {
    lastApiTime = currentMillis;
    sendGasToAPI();   
    sendFlameToAPI(); 
  }
  delay(10); 
}

// ================= ฟังก์ชันอัปเดตจอ (U8g2) =================
void updateOLED() {
  u8g2.clearBuffer();          // ล้างเมมโมรี่จอ
  u8g2.setFont(u8g2_font_6x10_tf); // ฟอนต์ขนาดเล็กอ่านง่าย

  // หัวข้อ
  u8g2.drawStr(0, 10, "SAFETY MONITOR");
  u8g2.drawLine(0, 12, 128, 12); // ขีดเส้นใต้

  // WiFi Status (มุมขวาบน)
  u8g2.setCursor(95, 10);
  if(WiFi.status() == WL_CONNECTED) u8g2.print("(WF)"); else u8g2.print("(--)");

  // ค่า Gas
  u8g2.setCursor(0, 25);
  u8g2.print("Gas: "); u8g2.print(gasVal);
  if(gasVal > gasThreshold) u8g2.print(" [!]");

  // ค่า Fire
  u8g2.setCursor(0, 37);
  u8g2.print("Fire: ");
  if(flameState == LOW) u8g2.print("DETECTED!"); else u8g2.print("Safe");

  // GPS Info
  u8g2.setCursor(0, 49);
  u8g2.print("Sats: "); u8g2.print(gps.satellites.value());
  
  // พิกัด
  u8g2.setCursor(0, 61);
  if(gps.location.isValid()) {
    u8g2.print(gps.location.lat(), 4);
    u8g2.print(",");
    u8g2.print(gps.location.lng(), 4);
  } else {
    u8g2.print("Searching GPS...");
  }

  u8g2.sendBuffer(); // สั่งให้ภาพขึ้นจอ
}

// ================= ฟังก์ชันเสริม (เหมือนเดิม) =================
String getGoogleMapLink() {
  if (gps.location.isValid()) {
    String lat = String(gps.location.lat(), 6);
    String lng = String(gps.location.lng(), 6);
    return "Map: https://maps.google.com/?q=" + lat + "," + lng;
  } else {
    return "Map: (Searching GPS...)";
  }
}

void sendLinePush(String message) {
  WiFiClientSecure client; 
  client.setInsecure(); 
  client.setBufferSizes(512, 512); 
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

void sendGasToAPI() {
  if (WiFi.status() != WL_CONNECTED) return;
  WiFiClientSecure client; client.setInsecure(); client.setBufferSizes(512, 512);
  HTTPClient http;
  if (http.begin(client, gasApiUrl)) {
    http.addHeader("Content-Type", "application/json");
    String gasState = (gasVal > gasThreshold) ? "DANGER" : "SAFE";
    String payload = "{\"gas_val\":" + String(gasVal) + ",\"gas_state\":\"" + gasState + "\"}";
    http.POST(payload); http.end();
  }
}

void sendFlameToAPI() {
  if (WiFi.status() != WL_CONNECTED) return;
  WiFiClientSecure client; client.setInsecure(); client.setBufferSizes(512, 512);
  HTTPClient http;
  if (http.begin(client, flameApiUrl)) {
    http.addHeader("Content-Type", "application/json");
    String fStatus = (flameState == LOW) ? "FIRE DETECTED" : "NORMAL";
    String payload = "{\"flame_status\":\"" + fStatus + "\"}";
    http.POST(payload); http.end();
  }
}