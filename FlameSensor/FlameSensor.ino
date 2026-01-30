int gasPin = A0;      // ขา Gas Sensor (อ่านค่าละเอียด)
int flamePin = D1;    // ขา Flame Sensor (อ่านค่า เจอ/ไม่เจอ)

int gasVal = 0;       // ตัวแปรเก็บค่าแก๊ส
int flameState = HIGH; // ตัวแปรเก็บสถานะไฟ (HIGH=ไม่เจอ, LOW=เจอ)
int gasThreshold = 500; // ค่าแจ้งเตือนแก๊ส (ปรับตามความเหมาะสมจากรอบที่แล้ว)

void setup() {
  pinMode(flamePin, INPUT); // กำหนดขา Flame เป็น Input รับค่า
  Serial.begin(9600);
}

void loop() {
  // 1. อ่านค่าจากทั้ง 2 เซ็นเซอร์
  gasVal = analogRead(gasPin);
  flameState = digitalRead(flamePin); 

  // 2. แสดงผล Gas
  Serial.print("Gas: ");
  Serial.print(gasVal);
  
  if (gasVal > gasThreshold) {
    Serial.print(" [DANGER: Gas Leak!]");
  } else {
    Serial.print(" [Normal]");
  }

  // 3. แสดงผล Flame (เว้นวรรคให้ดูง่าย)
  Serial.print("  |  Flame Sensor: ");
  
  // เช็คเงื่อนไข Flame (0 คือเจอไฟ, 1 คือไม่เจอ)
  if (flameState == LOW) { 
    Serial.println("FIRE DETECTED!!! (ไฟไหม้)");
  } else {
    Serial.println("Safe (ปลอดภัย)");
  }

  delay(800); // หน่วงเวลา
}