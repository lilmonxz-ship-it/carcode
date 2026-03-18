#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// --- กำหนด UUID สำหรับ BLE ---
#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E" 
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E" 

// --- กำหนดขามอเตอร์ ---
const int PWMA = 14; // ความเร็วมอเตอร์ซ้าย
const int AIN2 = 12; // ทิศทางมอเตอร์ซ้าย
const int AIN1 = 32; // ทิศทางมอเตอร์ซ้าย
const int STBY = 33; // เปิดการทำงานชิปมอเตอร์
const int BIN1 = 25; // ทิศทางมอเตอร์ขวา
const int BIN2 = 26; // ทิศทางมอเตอร์ขวา
const int PWMB = 27; // ความเร็วมอเตอร์ขวา

// --- กำหนดขาสำหรับเซนเซอร์ TCRT5000 Array (ยังไม่กำหนดพินจริง) ---
// *** ให้เปลี่ยนเลข 0 เป็นหมายเลขพินที่คุณต่อสายไว้เมื่อพร้อมนะครับ ***
const int IR_OUT2 = 0; // เซนเซอร์ Output 2 (ซ้าย)
const int IR_OUT4 = 0; // เซนเซอร์ Output 4 (ขวา)


// --- ตั้งค่าระบบควบคุม ---
bool isAutoMode = false; 
unsigned long lastPrintTime = 0; 
int motorSpeed = 180  ; // (ปรับได้ 0-255)

// --- ฟังก์ชันสั่งมอเตอร์ขับเคลื่อน ---
void moveRobot(int a1, int a2, int b1, int b2) {
  // ตรวจสอบว่าถ้าสั่งหยุด (0,0,0,0) ให้ตัดไฟ PWM ทันที
  if (a1 == 0 && a2 == 0 && b1 == 0 && b2 == 0) {
    analogWrite(PWMA, 0); 
    analogWrite(PWMB, 0);
  } else {
    // จ่ายไฟแบบลดความเร็ว (Soft Start)
    analogWrite(PWMA, motorSpeed); 
    analogWrite(PWMB, motorSpeed); 
  }
  
  digitalWrite(AIN1, a1); 
  digitalWrite(AIN2, a2); 
  digitalWrite(BIN1, b1); 
  digitalWrite(BIN2, b2); 
}

// --- คลาสสำหรับรับค่าจากแอปมือถือ ---
class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      String rxValue = pCharacteristic->getValue();

      if (rxValue.length() >= 4 && rxValue[0] == '!' && rxValue[1] == 'B') {
        char button = rxValue[2]; 
        char state  = rxValue[3]; 

        // --- เมื่อ "กด" ปุ่ม ---
        if (state == '1') {
          if (button == '1') {
            isAutoMode = true; 
            Serial.println("=============================");
            Serial.println(">>> AUTO MODE : ON <<<");
            Serial.println("=============================");
          }      
          else if (button == '2') {
            isAutoMode = false; 
            moveRobot(0, 0, 0, 0); 
            Serial.println("=============================");
            Serial.println(">>> AUTO MODE : OFF (STOP) <<<");
            Serial.println("=============================");
          }        

          // โหมดบังคับมือ
          else if (!isAutoMode) {
            if (button == '5') { Serial.println("Manual: FORWARD"); moveRobot(1, 0, 1, 0); }      
            else if (button == '6') { Serial.println("Manual: BACKWARD"); moveRobot(0, 1, 0, 1); } 
            else if (button == '7') { Serial.println("Manual: LEFT"); moveRobot(0, 1, 1, 0); } 
            else if (button == '8') { Serial.println("Manual: RIGHT"); moveRobot(1, 0, 0, 1); } 
          }
        } 
        // --- เมื่อ "ปล่อย" นิ้วจากปุ่ม ---
        else if (state == '0') {
          if (!isAutoMode && button >= '5' && button <= '8') {
            Serial.println("Manual: STOP (Key Released)");
            moveRobot(0, 0, 0, 0); 
          }
        }
      }
    }
};

void setup() {
  Serial.begin(115200);

  // 1. ตั้งค่าพินล้อ
  pinMode(PWMA, OUTPUT); pinMode(PWMB, OUTPUT);
  pinMode(AIN1, OUTPUT); pinMode(AIN2, OUTPUT);
  pinMode(BIN1, OUTPUT); pinMode(BIN2, OUTPUT);
  pinMode(STBY, OUTPUT); 
  
  digitalWrite(STBY, HIGH); 

  // 2. ตั้งค่าพินเซนเซอร์ (ถ้าพร้อมใส่พินแล้ว อย่าลืมมาตรวจสอบตรงนี้นะครับ)
  pinMode(IR_OUT2, INPUT); 
  pinMode(IR_OUT4, INPUT);
  pinMode(IR_OUT3, INPUT);

  // 3. เริ่มต้นระบบ BLE
  Serial.println("System Booting...");
  BLEDevice::init("MyRobot_BLE"); 
  BLEServer *pServer = BLEDevice::createServer();
  BLEService *pService = pServer->createService(SERVICE_UUID);
  
  BLECharacteristic *pRxCharacteristic = pService->createCharacteristic(
                         CHARACTERISTIC_UUID_RX,
                         BLECharacteristic::PROPERTY_WRITE
                       );
  pRxCharacteristic->setCallbacks(new MyCallbacks());

  BLECharacteristic *pTxCharacteristic = pService->createCharacteristic(
                         CHARACTERISTIC_UUID_TX,
                         BLECharacteristic::PROPERTY_NOTIFY
                       );
  pTxCharacteristic->addDescriptor(new BLE2902());

  pService->start();
  pServer->getAdvertising()->start();
  Serial.println("System Ready! Waiting for Bluetooth Connection...");
}

void loop() {
   // ลอจิกการเดินตามเส้นอัตโนมัติแบบ 2 เซนเซอร์
  if (isAutoMode) {
    int out2 = digitalRead(IR_OUT2); // อ่านค่าซ้าย
    int out4 = digitalRead(IR_OUT4); // อ่านค่าขวา
    //int out3 = digitalRead(IR_OUT3);
    String action = ""; 

  
    if (out2 == 0 && out4 == 0) {
    
      moveRobot(1, 0, 1, 0); 
      action = "FORWARD";
    } 
    else if (out2 == 1 && out4 == 0) {
     
      moveRobot(0, 1, 1, 0); 
      action = "TURN LEFT";
    } 
    else if (out2 == 0 && out4 == 1) {
      
      moveRobot(1, 0, 0, 1); 
      action = "TURN RIGHT";
    } 
    else if (out2 == 1 && out4 == 1) {
      
      moveRobot(0, 0, 0, 0); 
      action = "STOP (Intersection)";
    }

   
    if (millis() - lastPrintTime >= 200) {
      lastPrintTime = millis();
      Serial.print("Sensors [OUT2 OUT4]: [");
      Serial.print(out2); Serial.print(" ");
      Serial.print(out4); Serial.print("] -> ");
      Serial.println(action); 
    }
  }
} 
