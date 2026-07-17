#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>
#include <ArduinoJson.h>

// Pines SPI del RA-02 al ESP32
#define LORA_SS 5
#define LORA_RST 14
#define LORA_DIO0 2

// Frecuencia: 433 MHz (banda ISM libre en Mexico)
#define LORA_FREQ 433E6
// Parametros RF
#define SF 9
#define BW 125E3
#define CR 5

float valor = 20.0;
int contadorPaquetes = 0;

void setupLoRa() {
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(LORA_FREQ)) {
    Serial.println("ERROR: RA-02 no responde. Verifica conexiones y antena.");
    while (true);
  }
  LoRa.setSpreadingFactor(SF);
  LoRa.setSignalBandwidth(BW);
  LoRa.setCodingRate4(CR);
  LoRa.setTxPower(14);
  LoRa.setSyncWord(0xF4); 
  Serial.println("LoRa TX listo 433 MHz, SF9, BW125");
}

void setup() {
  Serial.begin(115200);
  setupLoRa();
}

void loop() {
  valor += 0.3;
  if (valor > 35.0) valor = 20.0;
  contadorPaquetes++;

  StaticJsonDocument<128> doc;
  doc["v"] = valor;
  doc["ts"] = millis();
  doc["id"] = contadorPaquetes;

  char payload[80];
  serializeJson(doc, payload);

  LoRa.beginPacket();
  LoRa.print(payload);
  LoRa.endPacket();

  Serial.print("TX [");
  Serial.print(contadorPaquetes);
  Serial.print("]: ");
  Serial.println(payload);

  delay(2000); 
}