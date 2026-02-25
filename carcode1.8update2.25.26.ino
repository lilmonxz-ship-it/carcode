#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>

// --- ตั้งค่า Wi-Fi Hotspot ---
const char* ssid = "MyRobot_ESP32";     // ชื่อ Wi-Fi //เปลี่ยนได้
const char* password = "123123123"; // รหัสผ่าน Wi-Fi //เปลี่ยนได้ตามใจ

WebServer server(80);

// --- กำหนดขามอเตอร์
const int PWMA = 25, AIN1 = 26, AIN2 = 27; //ซ้าย // ไว้แก้ pin ตอนประกอบด้วย //PWMA คือขาความเร็ว //AIN1/2 =ทิศทางในการับ (การจ่ายไฟ +,-)
const int PWMB = 12, BIN1 = 13, BIN2 = 33; //ขวา // ไว้แก้ pin ตอนประกอบด้วย //PWMB คือขาความเร็ว //BIN1/2 =ทิศทางในการับ (การจ่ายไฟ +,-)
const int STBY = 14; 

// --- กำหนดค่าสำหรับ Servo ---
Servo myServo;
Servo myServo1;

const int servoPin = 15; // ไว้แก้ pin ตอนประกอบด้วย
const int servoPin1 = 16; // ไว้แก้ pin ตอนประกอบด้วย

int servoAngle = 0; 
int targetAngle = 0;                 // องศาเป้าหมายที่อยากให้ไป
unsigned long lastServoMoveTime = 0; // นาฬิกาจับเวลา
const int servoDelay = 15; //15 ms     

// --- โค้ดหน้าเว็บ (HTML + CSS + JS) ---
String htmlPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>ESP32 Robot Controller</title>
  <style>
    body { text-align: center; font-family: sans-serif; margin-top: 30px; background-color: #f4f4f4; }
    .btn { padding: 20px 30px; font-size: 20px; margin: 5px; border-radius: 10px; background-color: #007BFF; color: white; border: none; user-select: none; box-shadow: 2px 2px 5px rgba(0,0,0,0.2); }
    .btn-servo { background-color: #28a745; }
    .btn:active { background-color: #0056b3; transform: scale(0.95); }
    .btn-servo:active { background-color: #1e7e34; }
    hr { margin: 30px auto; width: 80%; border: 1px solid #ccc; }
  </style>
</head>
<body>
  <h2>Robot Control</h2>
  <button class="btn" ontouchstart="fetch('/forward')" ontouchend="fetch('/stop')" onmousedown="fetch('/forward')" onmouseup="fetch('/stop')">W (หน้า)</button><br>
  <button class="btn" ontouchstart="fetch('/left')" ontouchend="fetch('/stop')" onmousedown="fetch('/left')" onmouseup="fetch('/stop')">A (ซ้าย)</button>
  <button class="btn" ontouchstart="fetch('/backward')" ontouchend="fetch('/stop')" onmousedown="fetch('/backward')" onmouseup="fetch('/stop')">S (หลัง)</button>
  <button class="btn" ontouchstart="fetch('/right')" ontouchend="fetch('/stop')" onmousedown="fetch('/right')" onmouseup="fetch('/stop')">D (ขวา)</button>
  
  <hr>
  
  <button class="btn btn-servo" onclick="fetch('/servoUp')">U (ยกขึ้น 90&deg;)</button>
  <button class="btn btn-servo" onclick="fetch('/servoDown')">B (ลง 0&deg;)</button>

  <script>
    // ดักจับคีย์บอร์ด
    document.addEventListener('keydown', function(e) {
      if(e.key.toLowerCase() === 'w') fetch('/forward');
      if(e.key.toLowerCase() === 'a') fetch('/left');
      if(e.key.toLowerCase() === 's') fetch('/backward');
      if(e.key.toLowerCase() === 'd') fetch('/right');
      if(e.key.toLowerCase() === 'u') fetch('/servoUp');
      if(e.key.toLowerCase() === 'b') fetch('/servoDown');
    });
    document.addEventListener('keyup', function(e) { 
      if(['w','a','s','d'].includes(e.key.toLowerCase())) fetch('/stop'); 
    });
  </script>
</body>
</html>
)rawliteral";

void setup() {
  Serial.begin(115200);

  // 1. เปิดโหมดสร้าง Wi-Fi (Access Point)
  Serial.println("\nStarting Access Point...");
  WiFi.softAP(ssid, password);
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP); // ปกติจะได้ 192.168.4.1

  // 2. ตั้งค่าพินล้อและเปิดบอร์ด TB6612FNG
  pinMode(PWMA, OUTPUT); pinMode(AIN1, OUTPUT); pinMode(AIN2, OUTPUT);
  pinMode(PWMB, OUTPUT); pinMode(BIN1, OUTPUT); pinMode(BIN2, OUTPUT);
  pinMode(STBY, OUTPUT); 
  digitalWrite(STBY, HIGH); 

  // 3. ตั้งค่า Servo
  myServo.setPeriodHertz(50); // ความถี่มาตรฐานสำหรับ Servo
  myServo1.setPeriodHertz(50);
  myServo.attach(servoPin, 500, 2400); 
  myServo.write(servoAngle);  // เริ่มต้นที่ 0 องศา
  myServo1.attach(servoPin1, 500, 2400); 
  myServo1.write(servoAngle);  // เริ่มต้นที่ 0 องศา

  // 4. เส้นทางคำสั่ง Web Server (ล้อ)
  server.on("/", []() { server.send(200, "text/html", htmlPage); });
  server.on("/forward", []() { moveRobot(1, 0, 1, 0); server.send(200); }); 
  server.on("/backward", []() { moveRobot(0, 1, 0, 1); server.send(200); }); 
  server.on("/left", []() { moveRobot(0, 1, 1, 0); server.send(200); });    
  server.on("/right", []() { moveRobot(1, 0, 0, 1); server.send(200); });   
  server.on("/stop", []() { moveRobot(0, 0, 0, 0); server.send(200); });    

  // 5. เส้นทางคำสั่ง Web Server (Servo แบบสมูท)
  server.on("/servoUp", []() { 
    targetAngle = 90; // จดเป้าหมายว่าอยากไป 90 องศา แล้วตอบกลับเว็บทันที //ถ้า servo ทำงานไม่ถึงเปลี่ยนตรงนี้
    server.send(200); 
  }); 

  server.on("/servoDown", []() { 
    targetAngle = 0;  // จดเป้าหมายว่าอยากไป 0 องศา แล้วตอบกลับเว็บทันที
    server.send(200); 
  }); 

  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient(); // รอรับคำสั่ง
if (millis() - lastServoMoveTime >= servoDelay) {
    lastServoMoveTime = millis(); // รีเซ็ตนาฬิกาจับเวลาใหม่

    // ถ้าองศาปัจจุบันยังน้อยกว่าเป้าหมาย ให้บวกเพิ่มไปเรื่อยๆ
    if (servoAngle < targetAngle) {
      servoAngle++;
      myServo.write(servoAngle);
      myServo1.write(servoAngle);
    } 
    // ถ้าองศาปัจจุบันยังมากกว่าเป้าหมาย ให้ลบลดลงมาเรื่อยๆ
    else if (servoAngle > targetAngle) {
      servoAngle--;
      myServo.write(servoAngle);
      myServo1.write(servoAngle);
    }
  }
}

// ฟังก์ชันสั่งมอเตอร์ขับเคลื่อน
void moveRobot(int a1, int a2, int b1, int b2) {
  digitalWrite(AIN1, a1); 
  digitalWrite(AIN2, a2); 
  analogWrite(PWMA, 200); 
  
  digitalWrite(BIN1, b1); 
  digitalWrite(BIN2, b2); 
  analogWrite(PWMB, 200); 
  
  if(a1 == 0 && a2 == 0 && b1 == 0 && b2 == 0) {
    analogWrite(PWMA, 0); 
    analogWrite(PWMB, 0);
  }
}
