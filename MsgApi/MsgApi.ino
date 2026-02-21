#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>

const char* ssid = "TECNO POVA 6 Pro 5G";
const char* password = "14092544";
const char* host = "api.line.me";
const int httpsPort = 443;

// ใส่ LINE Access Token ของคุณ
String accessToken = "JFjBPe0NShgsBQmiQmH86QCu/QPrKklOMbVnf7ClhHuSQYlkDo2PCC/c+1PnUsmYzAB+98/Vh+eswDl8xCUFp6LE47t5vIL3FGDsju7YbG1AOINHqxllreUAZS7smEt69sFR9OvoZIw16gbukXXZkwdB04t89/1O/w1cDnyilFU=";

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("WiFi Connected!");
}

void sendLineMessage(String message) {
  WiFiClientSecure client;
  client.setInsecure();  // ข้ามการตรวจสอบ SSL (ESP8266)

  if (!client.connect(host, httpsPort)) {
    Serial.println("Connection failed");
    return;
  }

  String url = "/v2/bot/message/push";
  String payload = "{\"to\":\"Cfef216e4298635f6cbe01dfffa7bb1a6\",\"messages\":[{\"type\":\"text\",\"text\":\"" + message + "\"}]}";
  
  
  String request = "POST " + url + " HTTP/1.1\r\n" +
                   "Host: " + String(host) + "\r\n" +
                   "Authorization: Bearer " + accessToken + "\r\n" +
                   "Content-Type: application/json\r\n" +
                   "Content-Length: " + String(payload.length()) + "\r\n\r\n" +
                   payload;
  
  client.print(request);
  Serial.println("Message Sent!");
}

void loop() {
  sendLineMessage("Hello Test 1 2 3");
  delay(30000);  // ส่งทุกๆ 60 วินาที
}
