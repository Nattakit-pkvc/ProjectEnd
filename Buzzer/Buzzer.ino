#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ESP8266HTTPClient.h>

// ================= 1. ตั้งค่า WIFI =================
const char* ssid     = "TECNO POVA 6 Pro 5G";
const char* password = "14092544";

// ================= 2. ตั้งค่า LINE & API =================
const char* lineHost = "api.line.me";
const char* accessToken = "LSmXROaNyWBDB6CXvsrQnXSE3vgX/oObcIaxGwSkrwS4V2V24IAl0Eusz2ZM3fiX0qcw5ifzMh8NfnPzviOUFc66UgdDDD+CmThQZH1kjmbH5DiQScPY2wa19CrOxgRE9sfMgeFBcCLb0G/uzjEHJgdB04t89/1O/w1cDnyilFU=LSmXROaNyWBDB6CXvsrQnXSE3vgX/oObcIaxGwSkrwS4V2V24IAl0Eusz2ZM3fiX0qcw5ifzMh8NfnPzviOUFc66UgdDDD+CmThQZH1kjmbH5DiQScPY2wa19CrOxgRE9sfMgeFBcCLb0G/uzjEHJgdB04t89/1O/w1cDnyilFU=";
String targetID = "Cf1f5aefc45f33c82d8bc303aa984fdef"; 
const char* gasApiUrl   = "https://gas-hee.vercel.app/api/gas";
const char* flameApiUrl = "https://gas-hee.vercel.app/api/flame";

// ================= 3. ตั้งค่า ขาอุปกรณ์ =================
int gasPin = A0;      
int flamePin = D1;    
int relayPin = D2;    // พัดลม
int buzzerPin = D3;   // Buzzer

int gasVal = 0;       
int flameState = HIGH; 
int gasThreshold = 500; 

bool isGasAlertSent = false;
bool isFireAlertSent = false;
unsigned long lastApiTime = 0;
long apiInterval = 10000; 

void setup() {
  Serial.begin(115200);
  
  pinMode(flamePin, INPUT);
  pinMode(relayPin, OUTPUT);
  pinMode(buzzerPin, OUTPUT); 
  
  // ปิดพัดลมและเสียงก่อน
  digitalWrite(relayPin, HIGH); 
  noTone(buzzerPin); // ใช้คำสั่งปิดเสียง Tone

  Serial.println("\nConnecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");
  sendLinePush("System Ready: Passive Buzzer Mode");
}

void loop() {
  gasVal = analogRead(gasPin);
  flameState = digitalRead(flamePin);

  Serial.print("Gas: "); Serial.print(gasVal);
  Serial.print(" | Flame: "); 
  if(flameState == LOW) Serial.print("FIRE!"); else Serial.print("Safe");
  Serial.println();

  // --- Logic ควบคุม (Passive Buzzer) ---
  if (gasVal > gasThreshold || flameState == LOW) {
    // === เหตุฉุกเฉิน ===
    digitalWrite(relayPin, LOW);   // เปิดพัดลม
    tone(buzzerPin, 2000);         // สั่ง Buzzer ร้องเสียงแหลม (2000Hz)
  } else {
    // === ปกติ ===
    digitalWrite(relayPin, HIGH);  // ปิดพัดลม
    noTone(buzzerPin);             // สั่งปิดเสียง
  }

  // --- ส่วนส่งไลน์และ API ---
  if (gasVal > gasThreshold) {
    if (!isGasAlertSent) { sendLinePush("⚠️ อันตราย! แก๊สรั่ว (" + String(gasVal) + ")"); isGasAlertSent = true; }
  } else { isGasAlertSent = false; }

  if (flameState == LOW) {
    if (!isFireAlertSent) { sendLinePush("🔥 ไฟไหม้!"); isFireAlertSent = true; }
  } else { isFireAlertSent = false; }

  bool isEmergency = (gasVal > gasThreshold || flameState == LOW);
  if (isEmergency) apiInterval = 1000; else apiInterval = 10000; 

  unsigned long currentMillis = millis();
  if (currentMillis - lastApiTime >= apiInterval) {
    lastApiTime = currentMillis;
    sendGasToAPI();   
    sendFlameToAPI(); 
  }
  delay(100); 
}

// ... (ฟังก์ชัน API เดิม) ...
void sendLinePush(String message) {
  WiFiClientSecure client; client.setInsecure(); 
  if (!client.connect(lineHost, 443)) return;
  String payload = "{\"to\":\"" + targetID + "\",\"messages\":[{\"type\":\"text\",\"text\":\"" + message + "\"}]}";
  client.println("POST /v2/bot/message/push HTTP/1.1");
  client.println("Host: " + String(lineHost));
  client.println("Authorization: Bearer " + String(accessToken));
  client.println("Content-Type: application/json");
  client.println("Content-Length: " + String(payload.length()));
  client.println(); client.println(payload);
}
void sendGasToAPI() {
  if (WiFi.status() != WL_CONNECTED) return;
  WiFiClientSecure client; client.setInsecure(); client.setBufferSizes(1024, 1024);
  HTTPClient http;
  if (http.begin(client, gasApiUrl)) {
    http.addHeader("Content-Type", "application/json");
    String gasState = (gasVal > gasThreshold) ? "DANGER" : (gasVal > gasThreshold - 100 ? "WARNING" : "SAFE");
    String payload = "{\"gas_val\":" + String(gasVal) + ",\"gas_state\":\"" + gasState + "\"}";
    http.POST(payload); http.end();
  }
}
void sendFlameToAPI() {
  if (WiFi.status() != WL_CONNECTED) return;
  WiFiClientSecure client; client.setInsecure(); client.setBufferSizes(1024, 1024);
  HTTPClient http;
  if (http.begin(client, flameApiUrl)) {
    http.addHeader("Content-Type", "application/json");
    String fStatus = (flameState == LOW) ? "FIRE DETECTED" : "NORMAL";
    String payload = "{\"flame_status\":\"" + fStatus + "\"}";
    http.POST(payload); http.end();
  }
}