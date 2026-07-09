#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#define SERVICE_UUID "b04ffa78-981f-43d4-9dec-a08e06bf1b9c"
#define CHAR_RPM_UUID "6af668d0-b1bb-4df5-b2ee-fe5b23d09230"
#define CHAR_SETPOINT_UUID "e100143e-41f8-465c-81c4-3a24bd8bc4ce"

bool deviceConnected = false;
float rpm = 0;
float setpoint = 0;

BLEServer* pServer = nullptr;
BLECharacteristic* pRpmChar = nullptr;

class ServerCallbacks : public BLEServerCallbacks{
  void onConnect(BLEServer* s){
    deviceConnected = true;
    Serial.println(">>> Central conectado!");
  }
    void onDisconnect(BLEServer* s){
    deviceConnected = false;
    s->startAdvertising();
    Serial.println(">>> Central conectado!");
  }
};

class SetpointCallbacks : public BLECharacteristicCallbacks{
  void onWrite (BLECharacteristic* c) {
    String val = c->getValue();
    if (val.length() > 0) {
      setpoint = atof(val.c_str());
      Serial.printf("Setpoint: %.1f RPM\n, setpoint");
    }
  }
};

void setup() {
  Serial.begin (115200);
  //Paso1
  BLEDevice::init("Motor-ESP32-EDU");

  //Paso2
  pServer = BLEDevice::createServer();
  pServer->setCallbacks (new ServerCallbacks());

  //Paso3
  BLEService* pService = pServer->createService(SERVICE_UUID);

  //Paso4
  pRpmChar = pService->createCharacteristic(CHAR_RPM_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  pRpmChar->addDescriptor(new BLE2902());

  //Paso5
  BLECharacteristic* pSpChar = pService->createCharacteristic(CHAR_SETPOINT_UUID, BLECharacteristic::PROPERTY_WRITE);
  pSpChar->setCallbacks(new SetpointCallbacks());

  //Paso6
  pService->start();
  BLEAdvertising* pAdv = BLEDevice::getAdvertising();
  pAdv->addServiceUUID(SERVICE_UUID);
  pAdv->setScanResponse(true);
  BLEDevice::startAdvertising();
  Serial.println("BLE listo - esperando conexion...");
}

void loop(){
  if(deviceConnected){
    rpm+=5.0;
    if(rpm>600.0) rpm = 0;
    char buf[10];
    sprintf(buf,"%.1f",rpm);
    pRpmChar->setValue(buf);
    pRpmChar->notify();
    Serial.printf("RPM: %1.f | Setpoint: %.1f\n", rpm, setpoint);
  }
  delay(500);
}
