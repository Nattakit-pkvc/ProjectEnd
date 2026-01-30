#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>

// ================= ตั้งค่า WIFI =================
const char* ssid     = "TECNO POVA 6 Pro 5G";
const char* password = "14092544";

// ================= ตั้งค่า LINE Messaging API =================
const char* host = "api.line.me";
const char* accessToken = "LSmXROaNyWBDB6CXvsrQnXSE3vgX/oObcIaxGwSkrwS4V2V24IAl0Eusz2ZM3fiX0qcw5ifzMh8NfnPzviOUFc66UgdDDD+CmThQZH1kjmbH5DiQScPY2wa19CrOxgRE9sfMgeFBcCLb0G/uzjEHJgdB04t89/1O/w1cDnyilFU=LSmXROaNyWBDB6CXvsrQnXSE3vgX/oObcIaxGwSkrwS4V2V24IAl0Eusz2ZM3fiX0qcw5ifzMh8NfnPzviOUFc66UgdDDD+CmThQZH1kjmbH5DiQScPY2wa19CrOxgRE9sfMgeFBcCLb0G/uzjEHJgdB04t89/1O/w1cDnyilFU=";
String targetID = "Cf1f5aefc45f33c82d8bc303aa984fdef"; // Group ID

// ================= ตั้งค่า ขาอุปกรณ์ =================
int gasPin = A0;      // เซ็นเซอร์แก๊ส
int flamePin = D1;    // เซ็นเซอร์ไฟ
int relayPin = D2;    // <--- ขา Relay สำหรับพัดลม (เพิ่มใหม่)

int gasVal = 0;       
int flameState = HIGH; 
int gasThreshold = 500; // ค่าแจ้งเตือนแก๊ส

// ตัวแปรกันส่งไลน์รัวๆ
bool isGasAlertSent = false;
bool isFireAlertSent = false;

void setup() {
  Serial.begin(115200);
  
  pinMode(flamePin, INPUT);
  pinMode(relayPin, OUTPUT);      // กำหนดขา Relay เป็น Output
  digitalWrite(relayPin, HIGH);   // สั่งปิด Relay ไว้ก่อน (HIGH = ปิด สำหรับบอร์ด Active LOW)

  // เชื่อมต่อ WiFi
  Serial.println();
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");
  
  sendLinePush("System Ready: ระบบความปลอดภัยพร้อมพัดลมระบายอากาศ ทำงานแล้ว!");
}

void loop() {
  // 1. อ่านค่า
  gasVal = analogRead(gasPin);
  flameState = digitalRead(flamePin); 

  // 2. แสดงผล Monitor
  Serial.print("Gas: "); Serial.print(gasVal);
  Serial.print(" | Flame: "); 
  if(flameState == LOW) Serial.print("FIRE!"); else Serial.print("Safe");

  // ================= ส่วนควบคุมพัดลม (FAN CONTROL) =================
  // ถ้า (แก๊สเกินค่าที่ตั้ง) หรือ (เจอเปลวไฟ)
  if (gasVal > gasThreshold || flameState == LOW) {
    digitalWrite(relayPin, LOW); // สั่ง LOW เพื่อเปิด Relay (พัดลมหมุน)
    Serial.print(" | FAN: ON [Active]");
  } else {
    digitalWrite(relayPin, HIGH); // สั่ง HIGH เพื่อปิด Relay (พัดลมหยุด)
    Serial.print(" | FAN: OFF");
  }
  Serial.println();
  // ==============================================================

  // 3. ส่งแจ้งเตือนไลน์ (GAS)
  if (gasVal > gasThreshold) {
    if (isGasAlertSent == false) {
      sendLinePush("อันตราย! แก๊สรั่ว (ระดับ: " + String(gasVal) + ") พัดลมกำลังระบายอากาศ!");
      isGasAlertSent = true;
    }
  } else {
    isGasAlertSent = false;
  }

  // 4. ส่งแจ้งเตือนไลน์ (FIRE)
  if (flameState == LOW) {
    if (isFireAlertSent == false) {
      sendLinePush("ไฟไหม้! ตรวจพบเปลวไฟ! ระบบกำลังทำงาน");
      isFireAlertSent = true;
    }
  } else {
    isFireAlertSent = false;
  }

  delay(1000); 
}

// ฟังก์ชันส่งข้อความ
void sendLinePush(String message) {
  WiFiClientSecure client;
  client.setInsecure(); 

  if (!client.connect(host, 443)) {
    Serial.println("Connection failed");
    return;
  }

  String payload = "{\"to\":\"" + targetID + "\",\"messages\":[{\"type\":\"text\",\"text\":\"" + message + "\"}]}";

  client.println("POST /v2/bot/message/push HTTP/1.1");
  client.println("Host: " + String(host));
  client.println("Authorization: Bearer " + String(accessToken));
  client.println("Content-Type: application/json");
  client.println("Content-Length: " + String(payload.length()));
  client.println();
  client.println(payload);

  Serial.println(" -> Line Msg Sent");
}