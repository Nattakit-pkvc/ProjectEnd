#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ESP8266HTTPClient.h>
#include <TinyGPS++.h>
#include <SoftwareSerial.h>
#include <Wire.h>
#include <U8g2lib.h>

// ================= 1. ตั้งค่าจอ OLED =================
// แก้ไขจาก u8x8_pin_none เป็น U8X8_PIN_NONE
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, D5, D4);

// ================= 2. ตั้งค่า WIFI =================
const char* ssid     = "TECNO POVA 6 Pro 5G"; 
const char* password = "14092544";

// ================= 3. ตั้งค่า API & LINE =================
const char* lineHost = "api.line.me";
const char* accessToken = "JFjBPe0NShgsBQmiQmH86QCu/QPrKklOMbVnf7ClhHuSQYlkDo2PCC/c+1PnUsmYzAB+98/Vh+eswDl8xCUFp6LE47t5vIL3FGDsju7YbG1AOINHqxllreUAZS7smEt69sFR9OvoZIw16gbukXXZkwdB04t89/1O/w1cDnyilFU=";
String targetID = "Cfef216e4298635f6cbe01dfffa7bb1a6"; 

const char* gasApiUrl   = "https://my-gas.vercel.app/api/gas"; 
const char* flameApiUrl = "https://my-gas.vercel.app/api/flame";
const char* gpsApiUrl   = "https://my-gas.vercel.app/api/gps";

// ================= 4. ตั้งค่า ขาอุปกรณ์ =================
int gasPin = A0;      
int flamePin = D1;    
int relayPin = D2;    // ขาควบคุม Relay Channel 4 (ต่อเข้า IN4)
int buzzerPin = D3;   
int gpsRxPin = D6;    // ต่อกับ TX ของ GPS
int gpsTxPin = D7;    // ต่อกับ RX ของ GPS

TinyGPSPlus gps;
SoftwareSerial gpsSerial(gpsRxPin, gpsTxPin);

int gasVal = 0;       
int displayGasVal = 0; 
int flameState = HIGH; 
int gasThreshold = 500; 

// ตัวแปร Logic
bool lastGasCritical = false;
bool lastFlameCritical = false;
bool isGasAlertSent = false;
bool isFireAlertSent = false;
unsigned long lastApiTime = 0;
long apiInterval = 30000; 

// ประกาศฟังก์ชัน
void sendLinePush(String message);
void sendGasToAPI();
void sendFlameToAPI();
void sendGPSToAPI();
String getGoogleMapLink();
void updateOLED(); 

void setup() {
  Serial.begin(115200);
  gpsSerial.begin(9600); 
  
  u8g2.begin();
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(0, 15, "System Starting...");
  u8g2.drawStr(0, 35, "Connecting...");
  u8g2.sendBuffer();
  
  pinMode(flamePin, INPUT);
  pinMode(relayPin, OUTPUT);
  pinMode(buzzerPin, OUTPUT); 
  digitalWrite(relayPin, HIGH); // ปิดพัดลม (Active Low)
  noTone(buzzerPin); 

  Serial.println("\n--- Safety System Ready ---");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ WiFi Connected!");
  
  sendLinePush("🟢 [แจ้งเตือน] ระบบตรวจจับวัตถุไวไฟ เริ่มทำงานพร้อมตรวจสอบความปลอดภัยครับ");
}

void loop() {
  while (gpsSerial.available() > 0) gps.encode(gpsSerial.read());
  
  gasVal = analogRead(gasPin) * 3; 
  if(gasVal > 1024) gasVal = 1024;
  flameState = digitalRead(flamePin);

  bool currentGasCritical = (gasVal > gasThreshold);
  bool currentFlameCritical = (flameState == LOW);
  bool statusChanged = (currentGasCritical != lastGasCritical) || (currentFlameCritical != lastFlameCritical);
  bool isEmergency = (currentGasCritical || currentFlameCritical);
  
  // ควบคุม Relay และ Buzzer
  if (isEmergency) {
    digitalWrite(relayPin, LOW);  // เปิดพัดลม (Channel 4)
    tone(buzzerPin, 2000);         
  } else {
    digitalWrite(relayPin, HIGH); // ปิดพัดลม
    noTone(buzzerPin);             
  }

  // ส่งข้อมูลไป API (Next.js/Vercel)
  if (statusChanged || (millis() - lastApiTime >= apiInterval)) {
      displayGasVal = gasVal; 
      sendGasToAPI();   
      sendFlameToAPI(); 
      sendGPSToAPI();   
      lastApiTime = millis();
      lastGasCritical = currentGasCritical;
      lastFlameCritical = currentFlameCritical;
  }

  updateOLED();

  // แจ้งเตือนผ่าน LINE
  if (currentGasCritical && !isGasAlertSent) {
      String msg = "⚠️ [ฉุกเฉิน] ตรวจพบปริมาณแก๊สรั่วไหลสูงผิดปกติ!\n";
      msg += "ระดับแก๊สปัจจุบัน: " + String(gasVal) + " PPM\n";
      msg += getGoogleMapLink();
      sendLinePush(msg); 
      isGasAlertSent = true; 
  } else if (!currentGasCritical) { isGasAlertSent = false; }

  if (currentFlameCritical && !isFireAlertSent) {
      String msg = "🔥 [อันตรายสูงสุด] ตรวจพบเปลวไฟ!\n";
      msg += "ระบบพัดลมดูดอากาศกำลังทำงาน\n";
      msg += getGoogleMapLink();
      sendLinePush(msg); 
      isFireAlertSent = true; 
  } else if (!currentFlameCritical) { isFireAlertSent = false; }

  delay(10); 
}

// ================= ฟังก์ชันจัดการลิงก์ GPS =================
String getGoogleMapLink() {
  if (gps.location.isValid()) {
    // กรณีระบุพิกัดได้จริง
    return "📍 พิกัด: https://www.google.com/maps?q=" + String(gps.location.lat(), 6) + "," + String(gps.location.lng(), 6);
  } else {
    // กรณีหาพิกัดไม่เจอ ให้ส่งลิงก์จำลองตามที่คุณต้องการ
    return "📍 พิกัด: http://maps.google.com/?q=7.870490,98.392528";
  }
}

// ================= ฟังก์ชันส่งข้อมูล API =================
void sendGasToAPI() {
  if (WiFi.status() != WL_CONNECTED) return;
  WiFiClientSecure client; client.setInsecure();
  HTTPClient http;
  if (http.begin(client, gasApiUrl)) {
    http.addHeader("Content-Type", "application/json");
    String gasState = (displayGasVal > gasThreshold) ? "DANGER" : "SAFE";
    String payload = "{\"gas_val\":" + String(displayGasVal) + ",\"gas_state\":\"" + gasState + "\"}";
    http.POST(payload);
    http.end();
  }
}

void sendFlameToAPI() {
  if (WiFi.status() != WL_CONNECTED) return;
  WiFiClientSecure client; client.setInsecure();
  HTTPClient http;
  if (http.begin(client, flameApiUrl)) {
    http.addHeader("Content-Type", "application/json");
    String fStatus = (flameState == LOW) ? "FIRE DETECTED" : "NORMAL";
    String payload = "{\"flame_status\":\"" + fStatus + "\"}";
    http.POST(payload);
    http.end();
  }
}

void sendGPSToAPI() {
  if (WiFi.status() != WL_CONNECTED || !gps.location.isValid()) return;
  WiFiClientSecure client; client.setInsecure();
  HTTPClient http;
  if (http.begin(client, gpsApiUrl)) {
    http.addHeader("Content-Type", "application/json");
    String payload = "{\"Lat\":" + String(gps.location.lat(), 6) + ",\"Long\":" + String(gps.location.lng(), 6) + "}";
    http.POST(payload);
    http.end();
  }
}

// ================= ฟังก์ชันแสดงผลจอ OLED =================
void updateOLED() {
  u8g2.clearBuffer();          
  u8g2.setFont(u8g2_font_6x10_tf); 
  u8g2.drawStr(0, 10, "SAFETY MONITOR");
  u8g2.drawLine(0, 12, 128, 12); 
  u8g2.setCursor(0, 25); u8g2.print("Gas: "); u8g2.print(displayGasVal);
  u8g2.setCursor(0, 37); u8g2.print("Fire: ");
  if(flameState == LOW) u8g2.print("DETECTED!"); else u8g2.print("Safe");
  u8g2.setCursor(0, 49); u8g2.print("Sats: "); u8g2.print(gps.satellites.value());
  u8g2.setCursor(0, 61);
  if(gps.location.isValid()) {
    u8g2.print(gps.location.lat(), 4); u8g2.print(","); u8g2.print(gps.location.lng(), 4);
  } else {
    u8g2.print("Searching GPS...");
  }
  u8g2.sendBuffer(); 
}

// ================= ฟังก์ชันส่ง LINE =================
void sendLinePush(String message) {
  WiFiClientSecure client; client.setInsecure(); client.setTimeout(5000);
  if (!client.connect(lineHost, 443)) return;
  message.replace("\n", "\\n");
  String payload = "{\"to\":\"" + targetID + "\",\"messages\":[{\"type\":\"text\",\"text\":\"" + message + "\"}]}";
  client.println("POST /v2/bot/message/push HTTP/1.1");
  client.println("Host: " + String(lineHost));
  client.println("Authorization: Bearer " + String(accessToken));
  client.println("Content-Type: application/json");
  client.println("Content-Length: " + String(payload.length()));
  client.println(); 
  client.print(payload);
}