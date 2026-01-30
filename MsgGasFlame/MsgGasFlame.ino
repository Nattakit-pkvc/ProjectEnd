#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>

// ================= ตั้งค่า WIFI =================
const char* ssid     = "TECNO POVA 6 Pro 5G";
const char* password = "14092544";

// ================= ตั้งค่า LINE Messaging API =================
const char* host = "api.line.me";
// ใส่ Channel Access Token (แบบยาว) ของคุณที่นี่
const char* accessToken = "LSmXROaNyWBDB6CXvsrQnXSE3vgX/oObcIaxGwSkrwS4V2V24IAl0Eusz2ZM3fiX0qcw5ifzMh8NfnPzviOUFc66UgdDDD+CmThQZH1kjmbH5DiQScPY2wa19CrOxgRE9sfMgeFBcCLb0G/uzjEHJgdB04t89/1O/w1cDnyilFU=LSmXROaNyWBDB6CXvsrQnXSE3vgX/oObcIaxGwSkrwS4V2V24IAl0Eusz2ZM3fiX0qcw5ifzMh8NfnPzviOUFc66UgdDDD+CmThQZH1kjmbH5DiQScPY2wa19CrOxgRE9sfMgeFBcCLb0G/uzjEHJgdB04t89/1O/w1cDnyilFU=";
// ใส่ Group ID (ต้องขึ้นต้นด้วย C) หรือ User ID ที่ต้องการส่ง
String targetID = "Cf1f5aefc45f33c82d8bc303aa984fdef"; 

// ================= ตั้งค่า ขาเซ็นเซอร์ =================
int gasPin = A0;      
int flamePin = D1;    

int gasVal = 0;       
int flameState = HIGH; 
int gasThreshold = 500; // ค่าแจ้งเตือนแก๊ส

// ตัวแปรกันส่งไลน์รัวๆ (สำคัญมากสำหรับ Messaging API เพื่อไม่ให้เปลืองโควต้า)
bool isGasAlertSent = false;
bool isFireAlertSent = false;

void setup() {
  Serial.begin(115200);
  pinMode(flamePin, INPUT);

  // เชื่อมต่อ WiFi
  Serial.println();
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");
  
  // ส่งข้อความเช็คสถานะตอนเปิดเครื่อง (กินโควต้า 1 ข้อความ)
  sendLinePush("System Started: ระบบตรวจจับไฟและแก๊สพร้อมทำงาน!");
}

void loop() {
  // 1. อ่านค่า
  gasVal = analogRead(gasPin);
  flameState = digitalRead(flamePin); 

  // 2. แสดงผลบนจอก่อน
  Serial.print("Gas: "); Serial.print(gasVal);
  Serial.print(" | Flame: "); 
  if(flameState == LOW) Serial.print("FIRE!"); else Serial.print("Safe");
  Serial.println();

  // 3. เช็คเงื่อนไขและส่งไลน์ (GAS)
  if (gasVal > gasThreshold) {
    if (isGasAlertSent == false) { // ส่งแค่ครั้งแรกที่เจอ
      sendLinePush("อันตราย! ตรวจพบแก๊สรั่ว (ระดับ: " + String(gasVal) + ")");
      isGasAlertSent = true; // ล็อคไว้
    }
  } else {
    isGasAlertSent = false; // ถ้าค่าปกติแล้ว ปลดล็อค
  }

  // 4. เช็คเงื่อนไขและส่งไลน์ (FIRE)
  if (flameState == LOW) { // LOW คือเจอไฟ
    if (isFireAlertSent == false) {
      sendLinePush("ไฟไหม้! ตรวจพบเปลวไฟ!!!");
      isFireAlertSent = true;
    }
  } else {
    isFireAlertSent = false;
  }

  delay(1000); 
}

// ฟังก์ชันส่งข้อความผ่าน Messaging API (Push Message)
void sendLinePush(String message) {
  WiFiClientSecure client;
  client.setInsecure(); // สำคัญ: ต้องใส่เพื่อให้ ESP8266 ส่ง HTTPS ได้

  if (!client.connect(host, 443)) {
    Serial.println("Connection failed");
    return;
  }

  // สร้าง JSON Payload
  // รูปแบบ: {"to": "ID", "messages": [{"type": "text", "text": "ข้อความ"}]}
  String payload = "{\"to\":\"" + targetID + "\",\"messages\":[{\"type\":\"text\",\"text\":\"" + message + "\"}]}";

  // ส่ง HTTP Header
  client.println("POST /v2/bot/message/push HTTP/1.1");
  client.println("Host: " + String(host));
  client.println("Authorization: Bearer " + String(accessToken));
  client.println("Content-Type: application/json");
  client.println("Content-Length: " + String(payload.length()));
  client.println();
  client.println(payload); // ส่งเนื้อหา

  Serial.println("Line Message Sent! (Quota used)");
}