#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>

// =====================================================
// ส่วนที่ 1: ตั้งค่า WiFi (แก้ไขตรงนี้)
// =====================================================
const char* ssid = "NJ_2.4G";      // ห้ามใช้ WiFi 5GHz นะครับ
const char* password = "14092544";

// =====================================================
// ส่วนที่ 2: ตั้งค่า Supabase (แก้ไขตรงนี้)
// =====================================================
// URL ต้องมี /rest/v1/sensor_data ต่อท้ายเสมอ (sensor_data คือชื่อตาราง)
String supabase_url = "https://rnwqilqaddbbnvmmlsbs.supabase.co/rest/v1/sensor_data"; 

// เอา Key ยาวๆ (anon key) มาใส่ตรงนี้
String supabase_key = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6InJud3FpbHFhZGRiYm52bW1sc2JzIiwicm9sZSI6ImFub24iLCJpYXQiOjE3Njk3NTE4MzYsImV4cCI6MjA4NTMyNzgzNn0.luviv1aDtZkvshnAzFyysxxMdS5BnXsj-lVqPvz502o"; 
// =====================================================

void setup() {
  Serial.begin(115200); // ตั้งค่าความเร็ว Serial
  WiFi.begin(ssid, password);
  
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClientSecure client;
    client.setInsecure(); // สำคัญมาก! สั่งให้ไม่ต้องเช็ค SSL (ไม่งั้นยิงไม่เข้า)

    HTTPClient http;
    
    // เริ่มการเชื่อมต่อ
    http.begin(client, supabase_url);
    
    // ตั้งค่า Header
    http.addHeader("Content-Type", "application/json");
    http.addHeader("apikey", supabase_key);
    http.addHeader("Authorization", "Bearer " + supabase_key);
    
    // จำลองค่าตัวเลข (เช่น อุณหภูมิ)
    float fakeValue = random(2000, 3500) / 100.0; // สุ่มเลข 20.00 - 35.00
    
    // สร้างข้อความ JSON ที่จะส่ง
    // รูปแบบ: {"value": 25.5, "device_name": "ESP8266-A1"}
    String jsonPayload = "{\"value\": " + String(fakeValue) + ", \"device_name\": \"ESP8266-A1\"}";
    
    Serial.print("Sending payload: ");
    Serial.println(jsonPayload);

    // ยิงข้อมูลออกไป (POST)
    int httpCode = http.POST(jsonPayload);

    if (httpCode > 0) {
      Serial.println("Success! HTTP Code: " + String(httpCode)); // ถ้าได้ 201 แปลว่าสำเร็จ
    } else {
      Serial.println("Error! HTTP Code: " + String(httpCode));
      Serial.println("Error details: " + http.errorToString(httpCode));
    }

    http.end();
  } else {
    Serial.println("WiFi Disconnected");
  }
  
  // รอ 10 วินาที แล้วส่งใหม่
  delay(10000); 
}