#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ESP8266HTTPClient.h>
#include <TinyGPS++.h>
#include <SoftwareSerial.h>
#include <Wire.h>
#include <U8g2lib.h>

// ================= 1. ตั้งค่าจอ OLED =================
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, D5, D4);

// ================= 2. ตั้งค่า WIFI =================
const char* ssid     = "TECNO POVA 6 Pro 5G"; 
const char* password = "14092544";

// ================= 3. ตั้งค่า API & LINE =================
const char* lineHost = "api.line.me";
const char* accessToken = "hGSKIJ3AfMGX7lOe2p8eJuULp35Oi4DXJ9CmzpkcXtt0NCwdtpTvFjra/K3nCijtMP4MNNZcN2xCR8nU7pea6NgGVRZiw+ajuGI+c9R64rNfcrHW4GVOt5O/UI41Be+CgDTr69aGhZsk4TR3oZykvgdB04t89/1O/w1cDnyilFU=";
String targetID = "C1e58407e5988269ef75b53cbafec6651"; 

const char* gasApiUrl   = "https://my-gas.vercel.app/api/gas"; 
const char* flameApiUrl = "https://my-gas.vercel.app/api/flame";
const char* gpsApiUrl   = "https://my-gas.vercel.app/api/gps";

// ================= 4. ตั้งค่า ขาอุปกรณ์ และพิกัดสำรอง =================
int gasPin = A0;      
int flamePin = D1;    
int relayPin = D2;    
int buzzerPin = D3;   
int gpsRxPin = D6;    
int gpsTxPin = D7;    

// พิกัดสำรอง (กรณี GPS หาสัญญาณไม่เจอ)
float fallbackLat = 7.870490;
float fallbackLng = 98.392528;

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
  digitalWrite(relayPin, HIGH); 
  noTone(buzzerPin); 

  Serial.println("\n\n===============================");
  Serial.println("--- Safety System Ready ---");
  Serial.println("===============================");
  
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ WiFi Connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  
  sendLinePush("🟢 [แจ้งเตือน] ระบบตรวจจับวัตถุไวไฟ เริ่มทำงานพร้อมตรวจสอบความปลอดภัยครับ");
  Serial.println("✅ Sent LINE Startup Notification");
}

void loop() {
  while (gpsSerial.available() > 0) gps.encode(gpsSerial.read());
  
  gasVal = analogRead(gasPin); 
  flameState = digitalRead(flamePin);

  bool currentGasCritical = (gasVal > gasThreshold); 
  bool currentFlameCritical = (flameState == LOW);
  bool statusChanged = (currentGasCritical != lastGasCritical) || (currentFlameCritical != lastFlameCritical);
  bool isEmergency = (currentGasCritical || currentFlameCritical);
  
  // ================= 1. ควบคุม Hardware ทันที =================
  if (isEmergency) {
    digitalWrite(relayPin, LOW);  
    tone(buzzerPin, 2000);        
  } else {
    digitalWrite(relayPin, HIGH); 
    noTone(buzzerPin);            
  }

  // ================= 2. แจ้งเตือนผ่าน LINE ทันที =================
  if (currentGasCritical && !isGasAlertSent) {
      Serial.println("\n🚨 >>>>> [TRIGGER] ตรวจพบแก๊สรั่ว! ส่งแจ้งเตือน LINE ทันที <<<<< 🚨");
      String msg = "⚠️ [ฉุกเฉิน] ตรวจพบปริมาณแก๊สรั่วไหลสูงผิดปกติ!\n";
      msg += "ระดับแก๊สปัจจุบัน: " + String(gasVal) + " PPM\n";
      msg += getGoogleMapLink();
      sendLinePush(msg); 
      isGasAlertSent = true; 
  } else if (!currentGasCritical) { 
      isGasAlertSent = false; 
  }

  if (currentFlameCritical && !isFireAlertSent) {
      Serial.println("\n🔥 >>>>> [TRIGGER] ตรวจพบเปลวไฟ! ส่งแจ้งเตือน LINE ทันที <<<<< 🔥");
      String msg = "🔥 [อันตรายสูงสุด] ตรวจพบเปลวไฟ!\n";
      msg += "ระบบพัดลมดูดอากาศกำลังทำงาน\n";
      msg += getGoogleMapLink();
      sendLinePush(msg); 
      isFireAlertSent = true; 
  } else if (!currentFlameCritical) { 
      isFireAlertSent = false; 
  }

  // ================= 3. อัปเดตหน้าจอ OLED =================
  updateOLED();

  // ================= 4. ส่งข้อมูล API และ Print ลง Serial Monitor =================
  if (statusChanged || (millis() - lastApiTime >= apiInterval)) {
      displayGasVal = gasVal; 
      
      // Print ข้อมูลลง Serial Monitor เพื่อมอนิเตอร์ดู
      Serial.println("\n--- [System Status Update] ---");
      Serial.print("Gas Level: "); Serial.print(displayGasVal); Serial.println(" PPM");
      Serial.print("Flame Status: "); Serial.println(flameState == LOW ? "🔥 DETECTED!" : "✅ Safe");
      Serial.print("GPS Satellites: "); Serial.println(gps.satellites.value());
      
      if(gps.location.isValid()) {
        Serial.print("Location (Real): "); Serial.print(gps.location.lat(), 6);
        Serial.print(", "); Serial.println(gps.location.lng(), 6);
      } else {
        Serial.print("Location (Fallback): "); Serial.print(fallbackLat, 6);
        Serial.print(", "); Serial.println(fallbackLng, 6);
      }
      
      Serial.println("Sending Data to Vercel API...");

      sendGasToAPI();   
      sendFlameToAPI(); 
      sendGPSToAPI();   
      
      Serial.println("✅ API Update Complete!");
      Serial.println("------------------------------");

      lastApiTime = millis();
      lastGasCritical = currentGasCritical;
      lastFlameCritical = currentFlameCritical;
  }

  delay(10); 
}

// ================= ฟังก์ชันจัดการลิงก์ GPS (รองรับพิกัดสำรอง) =================
String getGoogleMapLink() {
  if (gps.location.isValid()) {
    // กรณีระบุพิกัดได้จริง
    return "📍 พิกัด: http://maps.google.com/maps?q=" + String(gps.location.lat(), 6) + "," + String(gps.location.lng(), 6);
  } else {
    // กรณีหาพิกัดไม่เจอ ให้ส่งพิกัดจำลอง
    return "📍 พิกัด: http://maps.google.com/maps?q=" + String(fallbackLat, 6) + "," + String(fallbackLng, 6);
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
  // เอาการตรวจสอบ !gps.location.isValid() ออก เพื่อให้มันส่งค่าเข้า DB เสมอ
  if (WiFi.status() != WL_CONNECTED) return; 
  WiFiClientSecure client; client.setInsecure();
  HTTPClient http;
  if (http.begin(client, gpsApiUrl)) {
    http.addHeader("Content-Type", "application/json");
    
    // ตรวจสอบว่าจะใช้พิกัดจริง หรือ พิกัดสำรอง
    float currentLat = gps.location.isValid() ? gps.location.lat() : fallbackLat;
    float currentLng = gps.location.isValid() ? gps.location.lng() : fallbackLng;
    
    String payload = "{\"Lat\":" + String(currentLat, 6) + ",\"Long\":" + String(currentLng, 6) + "}";
    http.POST(payload);
    http.end();
  }
}

// ================= ฟังก์ชันแสดงผลจอ OLED (รองรับพิกัดสำรอง) =================
void updateOLED() {
  u8g2.clearBuffer();          

  // -----------------------------------------
  // 1. ส่วนหัว (Header) - ใช้ฟอนต์หนาและจัดให้อยู่กึ่งกลางนิดๆ
  // -----------------------------------------
  u8g2.setFont(u8g2_font_helvB08_tf); // ฟอนต์หนา (Bold)
  u8g2.drawStr(12, 11, "SAFETY MONITOR");
  u8g2.drawLine(0, 14, 128, 14);      // เส้นคั่นใต้หัวข้อ

  // -----------------------------------------
  // 2. ส่วนข้อมูลเซ็นเซอร์ (Sensor Data) - จัดคอลัมน์ให้อ่านง่าย
  // -----------------------------------------
  u8g2.setFont(u8g2_font_6x12_tf);    // ฟอนต์ปกติที่อ่านง่ายและดูโปร่งขึ้น

  // แก๊ส (Gas)
  u8g2.setCursor(0, 28);
  u8g2.print("Gas Level:");
  u8g2.setCursor(70, 28);             // ตั้งระยะ X ให้ตัวเลขตรงกัน
  u8g2.print(displayGasVal);
  // u8g2.print(" ppm");              // (ใส่หน่วยเพิ่มได้ถ้าต้องการ)

  // ไฟ (Fire)
  u8g2.setCursor(0, 42);
  u8g2.print("Fire Stat:");

  if(flameState == LOW) {
    // สร้างแถบ Highlight สีขาว ตัวอักษรดำ แจ้งเตือนไฟไหม้ให้เด่นชัด
    u8g2.setDrawColor(1);             // เลือกสีขาว (หรือสีสว่างของจอ)
    u8g2.drawBox(65, 33, 63, 11);     // วาดกล่องทึบ (x, y, width, height)
    u8g2.setDrawColor(0);             // เปลี่ยนสีปากกาเป็นสีดำ
    u8g2.drawStr(68, 42, "DETECTED!");
    u8g2.setDrawColor(1);             // คืนค่าสีปากกากลับเป็นสีปกติ
  } else {
    // สถานะปลอดภัย แสดงผลปกติ
    u8g2.drawStr(70, 42, "Safe");
  }

  // -----------------------------------------
  // 3. ส่วนตำแหน่ง (GPS Footer) - ไว้ล่างสุด และใช้ฟอนต์เล็กลง
  // -----------------------------------------
  u8g2.drawLine(0, 48, 128, 48);      // เส้นคั่นก่อนแสดงพิกัด
  u8g2.setFont(u8g2_font_5x8_tf);     // ฟอนต์ขนาดเล็ก เพื่อให้พิกัดยาวๆ ไม่ล้นจอ
  u8g2.setCursor(0, 58);

  if(gps.location.isValid()) {
    u8g2.print("GPS: ");
    u8g2.print(gps.location.lat(), 4); 
    u8g2.print(", "); 
    u8g2.print(gps.location.lng(), 4);
  } else {
    u8g2.print("GPS: "); // เปลี่ยน F: เป็น LOC(F) ให้ดูเข้าใจง่ายขึ้น
    u8g2.print(fallbackLat, 4); 
    u8g2.print(", "); 
    u8g2.print(fallbackLng, 4);
  }

  u8g2.sendBuffer(); 
}

// ================= ฟังก์ชันส่ง LINE =================
void sendLinePush(String message) {
  WiFiClientSecure client; client.setInsecure(); client.setTimeout(5000);
  if (!client.connect(lineHost, 443)) {
    Serial.println("❌ ไม่สามารถเชื่อมต่อกับ LINE API ได้");
    return;
  }
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