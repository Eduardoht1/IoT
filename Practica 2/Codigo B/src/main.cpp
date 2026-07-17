#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>
#include <ArduinoJson.h>

#define LORA_SS 5
#define LORA_RST 14
#define LORA_DIO0 2
#define LORA_FREQ 433.2E6
#define SF 9
#define BW 125E3
#define CR 5

void setupLoRa() {
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(LORA_FREQ)) {
    Serial.println("ERROR: RA-02 no responde. Verifica conexiones y antena.");
    while (true);
  }
  LoRa.setSpreadingFactor(SF);
  LoRa.setSignalBandwidth(BW);
  LoRa.setCodingRate4(CR);
  Serial.println("LoRa RX listo esperando paquetes (Modo Ping-Pong)...");
}

void procesarPaquete(int tamano) {
  String jsonStr = "";
  while (LoRa.available()) {
    jsonStr += (char)LoRa.read();
  }

  StaticJsonDocument<128> doc;
  DeserializationError err = deserializeJson(doc, jsonStr);
  if (err) {
    Serial.print("Error al parsear JSON: ");
    Serial.println(err.c_str());
    return;
  }

  // Verificamos que sea un paquete de datos original (tiene 'id' y 'v')
  if (doc.containsKey("id") && doc.containsKey("v")) {
    float valor = doc["v"];
    int id = doc["id"];
    
    int rssi = LoRa.packetRssi();
    float snr = LoRa.packetSnr();

    // Imprimir datos recibidos (sin la latencia falsa)
    Serial.print("PKT ");
    Serial.print(id);
    Serial.print(" | Val: ");
    Serial.print(valor, 1);
    Serial.print(" | RSSI: ");
    Serial.print(rssi);
    Serial.print(" dBm | SNR: ");
    Serial.print(snr, 1);
    Serial.println(" dB");

    // --- ENVIAR RESPUESTA INMEDIATA (ACK) ---
    // Armamos un JSON pequeñito solo con la llave "ack"
    StaticJsonDocument<64> ackDoc;
    ackDoc["ack"] = id;
    char ackPayload[32];
    serializeJson(ackDoc, ackPayload);

    // Cambiamos a modo Transmisión un instante
    LoRa.beginPacket();
    LoRa.print(ackPayload);
    LoRa.endPacket();
    
    Serial.print("  -> ACK respondido para PKT ");
    Serial.println(id);
    
    // Al terminar endPacket(), la librería regresa a RX automáticamente
  }
}

void setup() {
  Serial.begin(115200);
  setupLoRa();
}

void loop() {
  int tamano = LoRa.parsePacket();
  if (tamano > 0) {
    procesarPaquete(tamano);
  }
}