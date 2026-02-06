#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ESP8266HTTPClient.h>
#include <TinyGPS++.h>
#include <SoftwareSerial.h>

// ================= 1. ตั้งค่า WIFI =================
const char* ssid     = "TECNO POVA 6 Pro 5G";
const char* password = "14092544";

// ================= 2. ตั้งค่า LINE & API =================
const char* lineHost = "api.line.me";
// Access Token (ยาวๆ)
const char* accessToken = "LSmXROaNyWBDB6CXvsrQnXSE3vgX/oObcIaxGwSkrwS4V2V24IAl0Eusz2ZM3fiX0qcw5ifzMh8NfnPzviOUFc66UgdDDD+CmThQZH1kjmbH5DiQScPY2wa19CrOxgRE9sfMgeFBcCLb0G/uzjEHJgdB04t89/1O/w1cDnyilFU=LSmXROaNyWBDB6CXvsrQnXSE3vgX/oObcIaxGwSkrwS4V2V24IAl0Eusz2ZM3fiX0qcw5ifzMh8NfnPzviOUFc66UgdDDD+CmThQZH1kjmbH5DiQScPY2wa19CrOxgRE9sfMgeFBcCLb0G/uzjEHJgdB04t89/1O/w1cDnyilFU=";
// User ID
String targetID = "Cf1f5aefc45f33c82d8bc303aa984fdef"; 

const char* gasApiUrl   = "https://gas-hee.vercel.app/api/gas";
const char* flameApiUrl = "https://gas-hee.vercel.app/api/flame";

// ================= 3. ตั้งค่า ขาอุปกรณ์ =================
int gasPin = A0;      
int flamePin = D1;    
int relayPin = D2;    // พัดลม
int buzzerPin = D3;   // Buzzer
int gpsRxPin = D6;    // ต่อกับ TX ของ GPS
int gpsTxPin = D7;    // ต่อกับ RX ของ GPS 

// สร้าง Object GPS
TinyGPSPlus gps;
SoftwareSerial gpsSerial(gpsRxPin, gpsTxPin);

int gasVal = 0;       
int flameState = HIGH; 
int gasThreshold = 500; 

bool isGasAlertSent = false;
bool isFireAlertSent = false;
unsigned long lastApiTime = 0;
long apiInterval = 10000; 

// ประกาศฟังก์ชันไว้ก่อน
void sendLinePush(String message);
void sendGasToAPI();
void sendFlameToAPI();
String getGoogleMapLink();

void setup() {
  Serial.begin(115200);
  gpsSerial.begin(9600); // เริ่มต้น GPS
  
  pinMode(flamePin, INPUT);
  pinMode(relayPin, OUTPUT);
  pinMode(buzzerPin, OUTPUT); 
  
  // สถานะเริ่มต้น: ปิดพัดลม, ปิดเสียง
  digitalWrite(relayPin, HIGH); 
  noTone(buzzerPin); 

  Serial.println("\nSystem Starting...");
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");
  
  // ส่งข้อความเริ่มระบบ (ถ้าส่งได้ แสดงว่าเน็ตดี)
  sendLinePush("System Ready: GPS + Continuous Alarm Active");
}

void loop() {
  // --- 1. อ่านค่า GPS (ต้องทำตลอดเวลา) ---
  while (gpsSerial.available() > 0) {
    gps.encode(gpsSerial.read());
  }

  // --- 2. อ่านค่าเซ็นเซอร์ ---
  gasVal = analogRead(gasPin);
  flameState = digitalRead(flamePin);

  // --- 3. Logic ควบคุม (เสียง Buzzer ยาว) ---
  if (gasVal > gasThreshold || flameState == LOW) {
    // === เหตุฉุกเฉิน ===
    digitalWrite(relayPin, LOW);   // เปิดพัดลม
    tone(buzzerPin, 2000);         // เสียงดังยาว (Continuous)
  } else {
    // === ปกติ ===
    digitalWrite(relayPin, HIGH);  // ปิดพัดลม
    noTone(buzzerPin);             // ปิดเสียง
  }

  // --- 4. ส่ง LINE Notify (แก้เรื่อง \n แล้ว) ---
  if (gasVal > gasThreshold) {
    if (!isGasAlertSent) { 
      String mapLink = getGoogleMapLink();
      // ใช้เว้นวรรคแทนการขึ้นบรรทัดใหม่ เพื่อป้องกัน JSON Error
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

  // --- 5. ส่ง API ---
  bool isEmergency = (gasVal > gasThreshold || flameState == LOW);
  apiInterval = isEmergency ? 1000 : 10000; 

  unsigned long currentMillis = millis();
  if (currentMillis - lastApiTime >= apiInterval) {
    lastApiTime = currentMillis;
    sendGasToAPI();   
    sendFlameToAPI(); 
  }
  
  delay(10); // หน่วงนิดเดียวพอ ให้ GPS ทำงานทัน
}

// ================= ฟังก์ชันสร้างลิ้งค์แผนที่ =================
String getGoogleMapLink() {
  if (gps.location.isValid()) {
    String lat = String(gps.location.lat(), 6);
    String lng = String(gps.location.lng(), 6);
    // ลิ้งค์แบบนี้เปิดแอป Maps ได้เลย
    return "Map: https://maps.google.com/?q=" + lat + "," + lng;
  } else {
    return "Map: (Searching GPS...)";
  }
}

// ================= ฟังก์ชันส่ง LINE (แก้ Bug Reset Loop) =================
void sendLinePush(String message) {
  WiFiClientSecure client; 
  client.setInsecure(); 
  
  // [สำคัญ] ลดขนาด Buffer เพื่อป้องกัน Memory เต็มจนเครื่องรีสตาร์ท
  client.setBufferSizes(512, 512); 
  
  if (!client.connect(lineHost, 443)) {
    Serial.println("❌ LINE Connect Failed");
    return;
  }
  
  // JSON Payload (ห้ามมี \n ข้างใน message เด็ดขาด)
  String payload = "{\"to\":\"" + targetID + "\",\"messages\":[{\"type\":\"text\",\"text\":\"" + message + "\"}]}";
  
  client.println("POST /v2/bot/message/push HTTP/1.1");
  client.println("Host: " + String(lineHost));
  client.println("Authorization: Bearer " + String(accessToken));
  client.println("Content-Type: application/json");
  client.println("Content-Length: " + String(payload.length()));
  client.println(); 
  client.print(payload);
  
  Serial.println("✅ LINE Sent: " + message);
}

// ================= ฟังก์ชันส่ง Gas API =================
void sendGasToAPI() {
  if (WiFi.status() != WL_CONNECTED) return;
  WiFiClientSecure client; 
  client.setInsecure(); 
  client.setBufferSizes(512, 512); // ลด Buffer
  
  HTTPClient http;
  if (http.begin(client, gasApiUrl)) {
    http.addHeader("Content-Type", "application/json");
    String gasState = (gasVal > gasThreshold) ? "DANGER" : (gasVal > gasThreshold - 100 ? "WARNING" : "SAFE");
    String payload = "{\"gas_val\":" + String(gasVal) + ",\"gas_state\":\"" + gasState + "\"}";
    int httpCode = http.POST(payload);
    http.end();
  }
}

// ================= ฟังก์ชันส่ง Flame API =================
void sendFlameToAPI() {
  if (WiFi.status() != WL_CONNECTED) return;
  WiFiClientSecure client; 
  client.setInsecure(); 
  client.setBufferSizes(512, 512); // ลด Buffer
  
  HTTPClient http;
  if (http.begin(client, flameApiUrl)) {
    http.addHeader("Content-Type", "application/json");
    String fStatus = (flameState == LOW) ? "FIRE DETECTED" : "NORMAL";
    String payload = "{\"flame_status\":\"" + fStatus + "\"}";
    int httpCode = http.POST(payload);
    http.end();
  }
}