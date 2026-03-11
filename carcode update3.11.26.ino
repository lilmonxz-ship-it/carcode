#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// --- กำหนด UUID สำหรับ BLE ---
#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E" 
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E" 

// --- กำหนดขามอเตอร์ ---
const int AIN1 = 25, AIN2 = 26; // ซ้าย
const int BIN1 = 27, BIN2 = 14; // ขวา
const int STBY = 22; 

// --- กำหนดขาสำหรับเซนเซอร์ TCRT5000 Array (5 ช่อง) ---
const int IR_L2 = 34; // เซนเซอร์ซ้ายสุด
const int IR_L1 = 35; // เซนเซอร์ซ้ายใน
const int IR_C  = 32; // เซนเซอร์ตรงกลาง
const int IR_R1 = 33; // เซนเซอร์ขวาใน
const int IR_R2 = 21; // เซนเซอร์ขวาสุด

bool isAutoMode = false; 
unsigned long lastPrintTime = 0; // ตัวแปรสำหรับตั้งเวลาโชว์ข้อความ

// ฟังก์ชันสั่งมอเตอร์ขับเคลื่อน
void moveRobot(int a1, int a2, int b1, int b2) {
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

          // โหมดบังคับมือ (ลูกศร)
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
  pinMode(AIN1, OUTPUT); pinMode(AIN2, OUTPUT);
  pinMode(BIN1, OUTPUT); pinMode(BIN2, OUTPUT);
  pinMode(STBY, OUTPUT); digitalWrite(STBY, HIGH); 

  // 2. ตั้งค่าพินเซนเซอร์
  pinMode(IR_L2, INPUT); pinMode(IR_L1, INPUT);
  pinMode(IR_C,  INPUT); pinMode(IR_R1, INPUT);
  pinMode(IR_R2, INPUT);

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
  // --- ลอจิกการเดินตามเส้นอัตโนมัติ ---
  if (isAutoMode) {
    int l2 = digitalRead(IR_L2);
    int l1 = digitalRead(IR_L1);
    int c  = digitalRead(IR_C);
    int r1 = digitalRead(IR_R1);
    int r2 = digitalRead(IR_R2);

    String action = ""; // ตัวแปรเก็บข้อความสถานะการขับ

    // ตัดสินใจเลี้ยว
    if (c == 1) {
      moveRobot(1, 0, 1, 0); 
      action = "FORWARD";
    } 
    else if (l1 == 1 || l2 == 1) {
      moveRobot(0, 1, 1, 0); 
      action = "TURN LEFT";
    } 
    else if (r1 == 1 || r2 == 1) {
      moveRobot(1, 0, 0, 1); 
      action = "TURN RIGHT";
    } 
    else {
      moveRobot(0, 0, 0, 0); 
      action = "STOP (Line Lost)";
    }

    // --- ส่วนแสดงผลบน Serial Monitor (ปริ้นทุกๆ 200 มิลลิวินาที) ---
    if (millis() - lastPrintTime >= 200) {
      lastPrintTime = millis();
      // ปริ้นค่าเซนเซอร์ออกมาดู ว่าตัวไหนเจอเส้นดำบ้าง (1 = เจอสีดำ, 0 = สีขาว)
      Serial.print("Sensors [L2 L1 C R1 R2]: [");
      Serial.print(l2); Serial.print(" ");
      Serial.print(l1); Serial.print(" ");
      Serial.print(c);  Serial.print(" ");
      Serial.print(r1); Serial.print(" ");
      Serial.print(r2); Serial.print("] -> ");
      Serial.println(action); // ปริ้นทิศทางที่รถตัดสินใจไป
    }
  }
}
