#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ESP8266HTTPClient.h>

// ================= 1. ตั้งค่า WIFI =================
const char* ssid     = "TECNO POVA 6 Pro 5G";
const char* password = "14092544";

// ================= 2. ตั้งค่า LINE Messaging API =================
const char* lineHost = "api.line.me";
const char* accessToken = "LSmXROaNyWBDB6CXvsrQnXSE3vgX/oObcIaxGwSkrwS4V2V24IAl0Eusz2ZM3fiX0qcw5ifzMh8NfnPzviOUFc66UgdDDD+CmThQZH1kjmbH5DiQScPY2wa19CrOxgRE9sfMgeFBcCLb0G/uzjEHJgdB04t89/1O/w1cDnyilFU=LSmXROaNyWBDB6CXvsrQnXSE3vgX/oObcIaxGwSkrwS4V2V24IAl0Eusz2ZM3fiX0qcw5ifzMh8NfnPzviOUFc66UgdDDD+CmThQZH1kjmbH5DiQScPY2wa19CrOxgRE9sfMgeFBcCLb0G/uzjEHJgdB04t89/1O/w1cDnyilFU=";
String targetID = "Cf1f5aefc45f33c82d8bc303aa984fdef"; 

// ================= 3. ตั้งค่า WEB API (Vercel) =================
const char* gasApiUrl   = "https://gas-hee.vercel.app/api/gas";
const char* flameApiUrl = "https://gas-hee.vercel.app/api/flame";

// ================= 4. ตั้งค่า ขาอุปกรณ์ =================
int gasPin = A0;      
int flamePin = D1;    
int relayPin = D2;    

int gasVal = 0;       
int flameState = HIGH; 
int gasThreshold = 500; 

// ตัวแปรกันส่งไลน์รัวๆ
bool isGasAlertSent = false;
bool isFireAlertSent = false;

// ตัวแปรจับเวลาสำหรับส่ง API
unsigned long lastApiTime = 0;
// เราจะใช้ตัวแปรนี้กำหนดความถี่ในการส่ง (ไม่ต้องใส่ const เพราะค่าจะเปลี่ยนไปมาได้)
long apiInterval = 10000; 

void setup() {
  Serial.begin(115200);
  
  pinMode(flamePin, INPUT);
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, HIGH); // ปิดพัดลมก่อน (Active LOW)

  Serial.println();
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");
  
  sendLinePush("System Online: เชื่อมต่อครบวงจร (Smart Interval Mode)!");
}

void loop() {
  // --- 1. อ่านค่าเซ็นเซอร์ ---
  gasVal = analogRead(gasPin);
  flameState = digitalRead(flamePin);

  // --- 2. แสดงผล Monitor ---
  Serial.print("Gas: "); Serial.print(gasVal);
  Serial.print(" | Flame: "); 
  if(flameState == LOW) Serial.print("FIRE!"); else Serial.print("Safe");
  Serial.println();

  // --- 3. ระบบพัดลม (Fan Control) ---
  if (gasVal > gasThreshold || flameState == LOW) {
    digitalWrite(relayPin, LOW); // เปิดพัดลม
  } else {
    digitalWrite(relayPin, HIGH); // ปิดพัดลม
  }

  // --- 4. ระบบแจ้งเตือน LINE (Gas) ---
  if (gasVal > gasThreshold) {
    if (!isGasAlertSent) {
      sendLinePush("⚠️ อันตราย! แก๊สรั่ว (ระดับ: " + String(gasVal) + ")");
      isGasAlertSent = true;
    }
  } else {
    isGasAlertSent = false;
  }

  // --- 5. ระบบแจ้งเตือน LINE (Fire) ---
  if (flameState == LOW) {
    if (!isFireAlertSent) {
      sendLinePush("🔥 ไฟไหม้! ตรวจพบเปลวไฟ!");
      isFireAlertSent = true;
    }
  } else {
    isFireAlertSent = false;
  }

  // ================= 6. ระบบส่ง API (ปรับเปลี่ยนตามสถานการณ์) =================
  
  // เช็คว่ามีเหตุฉุกเฉินไหม?
  bool isEmergency = (gasVal > gasThreshold || flameState == LOW);

  if (isEmergency) {
    // [เคสฉุกเฉิน] ส่งทุกๆ 1 วินาที (เพื่อให้หน้าเว็บอัปเดตทันที)
    apiInterval = 1000; 
  } else {
    // [เคสปกติ] ส่งทุกๆ 10 วินาที (ประหยัดเน็ต/แรม)
    apiInterval = 10000; 
  }

  // ตรวจสอบเวลา
  unsigned long currentMillis = millis();
  if (currentMillis - lastApiTime >= apiInterval) {
    lastApiTime = currentMillis;
    
    // เรียกฟังก์ชันส่งข้อมูล
    Serial.print(">> Sending API (Interval: "); Serial.print(apiInterval); Serial.println("ms)");
    sendGasToAPI();   
    sendFlameToAPI(); 
  }

  // ลดเวลา delay หลักลงเหลือ 0.1 วินาที เพื่อให้ระบบตรวจจับตอบสนองไวขึ้น
  // (เพราะเราใช้ millis() คุมเวลาส่ง API แล้ว ไม่จำเป็นต้อง delay นานๆ)
  delay(100); 
}

// ================= ฟังก์ชันส่ง LINE =================
void sendLinePush(String message) {
  WiFiClientSecure client;
  client.setInsecure(); 

  if (!client.connect(lineHost, 443)) {
    Serial.println("Line Connection failed");
    return;
  }

  String payload = "{\"to\":\"" + targetID + "\",\"messages\":[{\"type\":\"text\",\"text\":\"" + message + "\"}]}";

  client.println("POST /v2/bot/message/push HTTP/1.1");
  client.println("Host: " + String(lineHost));
  client.println("Authorization: Bearer " + String(accessToken));
  client.println("Content-Type: application/json");
  client.println("Content-Length: " + String(payload.length()));
  client.println();
  client.println(payload);
}

// ================= ฟังก์ชันส่ง Gas API =================
void sendGasToAPI() {
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClientSecure client;
  client.setInsecure(); 
  client.setBufferSizes(1024, 1024);

  HTTPClient http;
  
  if (http.begin(client, gasApiUrl)) {
    http.addHeader("Content-Type", "application/json");

    String gasState = "SAFE";
    if (gasVal > gasThreshold) gasState = "DANGER";
    else if (gasVal > gasThreshold - 100) gasState = "WARNING";
    
    String payload = "{\"gas_val\":" + String(gasVal) + ",\"gas_state\":\"" + gasState + "\"}";

    int httpCode = http.POST(payload);
    if (httpCode > 0) {
      Serial.printf("✅ Gas API Sent: %d\n", httpCode);
    } else {
      Serial.printf("❌ Gas API Failed: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
  }
}

// ================= ฟังก์ชันส่ง Flame API =================
void sendFlameToAPI() {
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClientSecure client;
  client.setInsecure(); 
  client.setBufferSizes(1024, 1024);

  HTTPClient http;
  
  if (http.begin(client, flameApiUrl)) {
    http.addHeader("Content-Type", "application/json");

    String fStatus = "NORMAL";
    if (flameState == LOW) fStatus = "FIRE DETECTED";
    
    String payload = "{\"flame_status\":\"" + fStatus + "\"}";

    int httpCode = http.POST(payload);
    if (httpCode > 0) {
      Serial.printf("✅ Flame API Sent: %d\n", httpCode);
    } else {
      Serial.printf("❌ Flame API Failed: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
  }
}