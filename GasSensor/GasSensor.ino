int analogPin = A0; 
int val = 0;
int threshold = 500; // **เพิ่มตัวแปรนี้: กำหนดค่าระดับที่จะให้แจ้งเตือน (ปรับเลขนี้ได้)**

void setup() {
  Serial.begin(9600); 
}

void loop() {
  val = analogRead(analogPin);
  
  Serial.print("Gas Level = "); 
  Serial.print(val);          // พิมพ์ตัวเลข (ใช้ print เฉยๆ เพื่อให้อยู่บรรทัดเดียวกับข้อความหลัง)
  
  // ตรวจสอบค่า: ถ้ามากกว่า 500 ให้เตือน
  if (val > threshold) {
    Serial.println("  <-- DANGER! Gas Detected! (อันตราย แก๊สรั่ว)"); 
  } else {
    Serial.println("  (Normal)"); // ถ้าปกติ ก็ขึ้นว่า Normal
  }
  
  delay(800); 
}