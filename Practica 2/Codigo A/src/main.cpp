#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>
#include <ArduinoJson.h>

// Pines SPI del RA-02 al ESP32
#define LORA_SS 5
#define LORA_RST 14
#define LORA_DIO0 2

// Frecuencia: 433 MHz
#define LORA_FREQ 433.2E6
// Parametros RF
#define SF 12
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
  Serial.println("LoRa TX listo (Modo Ping-Pong)");
}

void setup() {
  Serial.begin(115200);
  setupLoRa();
}

void loop() {
  valor += 0.3;
  if (valor > 35.0) valor = 20.0;
  contadorPaquetes++;

  // 1. Preparar el paquete (sin marca de tiempo, solo datos e ID)
  StaticJsonDocument<128> doc;
  doc["v"] = valor;
  doc["id"] = contadorPaquetes;
  
  char payload[80];
  serializeJson(doc, payload);

  // Guardamos el tiempo exacto ANTES de mandar a volar el paquete
  unsigned long tiempoEnvio = millis();

  // 2. Enviar el paquete
  LoRa.beginPacket();
  LoRa.print(payload);
  LoRa.endPacket();

  // ¡CRÍTICO! Forzar al radio a escuchar inmediatamente después de enviar
  LoRa.receive();

  Serial.print("TX [");
  Serial.print(contadorPaquetes);
  Serial.print("]: Enviado. Esperando ACK... ");

  // 3. Ponerse a escuchar para esperar la respuesta (Timeout ajustado a 5000ms por el SF12)
  bool ackRecibido = false;
  unsigned long inicioEspera = millis();
  
  while (millis() - inicioEspera < 5000) {
    int tamano = LoRa.parsePacket();
    if (tamano > 0) {
      String jsonStr = "";
      while (LoRa.available()) {
        jsonStr += (char)LoRa.read();
      }
      
      StaticJsonDocument<128> ackDoc;
      DeserializationError err = deserializeJson(ackDoc, jsonStr);
      
      // Verificamos si es un mensaje de ACK válido
      if (!err && ackDoc.containsKey("ack")) {
        int id_recibido = ackDoc["ack"];
        
        if (id_recibido == contadorPaquetes) {
          // ¡Recibimos el Acuse de Recibo correcto! Calculamos la latencia.
          unsigned long tiempoTotal = millis() - tiempoEnvio;
          float latenciaReal = tiempoTotal / 2.0; 
          
          Serial.print("ACK OK | Latencia de vuelo: ");
          Serial.print(latenciaReal);
          Serial.println(" ms");
          
          ackRecibido = true;
          break; // Salimos del ciclo de espera
        }
      }
    }
  }
  
  // Si pasaron 5 segundos y no hubo respuesta
  if (!ackRecibido) {
    Serial.println("Timeout. Paquete perdido.");
  }

  delay(2000); 
}