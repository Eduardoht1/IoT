// =============================================================================
// simulador_motor.cpp — Simula datos del motor DC y los publica por MQTT
// Universidad Panamericana · IoT · Semana 2
//
// Este código simula un motor DC con controlador PID para que puedan
// configurar el pipeline completo (MQTT → InfluxDB → Grafana) ANTES
// de conectar el hardware real.
//
// Qué simula:
//   - RPM que responden a un setpoint con dinámica de primer orden (tau = 1.5s)
//   - Ruido de sensor Hall realista
//   - Cambios automáticos de setpoint cada 15 segundos
//   - Publicación por MQTT en formato JSON cada 500ms
//
// ANTES DE FLASHEAR: cambiar WIFI_SSID, WIFI_PASSWORD y MQTT_BROKER
// =============================================================================

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// ── Credenciales WiFi ─────────────────────────────────────────────────────────
const char* WIFI_SSID     = "LaptopEdu";      // <-- CAMBIAR
const char* WIFI_PASSWORD = "G65n72R98a02E05";   // <-- CAMBIAR

// ── Dirección del broker MQTT ─────────────────────────────────────────────────
// IP de la laptop que corre Docker
// Windows: ipconfig | Mac/Linux: ifconfig
const char* MQTT_BROKER = "192.168.137.1";   // <-- CAMBIAR
const int   MQTT_PORT   = 1883;

// ── Topics MQTT ───────────────────────────────────────────────────────────────
const char* TOPIC_TELEMETRIA = "iot/motor/telemetria";
const char* TOPIC_SETPOINT   = "iot/motor/setpoint";
const char* TOPIC_HEARTBEAT  = "iot/sistema/heartbeat";

// ── Parámetros de la simulación ───────────────────────────────────────────────
#define TAU_MOTOR_S      1.5f   // Constante de tiempo (segundos)
#define GANANCIA_MOTOR   5.5f   // RPM por porcentaje de PWM
#define RUIDO_RPM        3.0f   // Ruido del sensor Hall (RPM)
#define INTERVALO_PUB_MS 500    // Publicar cada 500 ms

// ── Secuencia automática de setpoints ────────────────────────────────────────
const float SETPOINTS[]             = {200.0, 400.0, 600.0, 300.0, 500.0, 100.0};
const int   N_SETPOINTS             = sizeof(SETPOINTS) / sizeof(SETPOINTS[0]);
const uint32_t DURACION_SETPOINT_MS = 15000;   // Cambiar cada 15 s

// ── Variables de estado ───────────────────────────────────────────────────────
float    rpmActual   = 0.0f;
float    setpoint    = 300.0f;
float    pwmActual   = 0.0f;
int      idxSetpoint = 0;
uint32_t tUltimoCambio        = 0;
uint32_t tUltimaPublicacion   = 0;
uint32_t tInicio              = 0;
uint32_t contadorPaquetes     = 0;

WiFiClient   wifiClient;
PubSubClient mqttClient(wifiClient);

// ── Ruido gaussiano (Box-Muller simplificado) ─────────────────────────────────
float ruidoGaussiano(float amplitud) {
  float u1 = (float)random(1, 10000) / 10000.0f;
  float u2 = (float)random(1, 10000) / 10000.0f;
  return sqrt(-2.0f * log(u1)) * cos(2.0f * PI * u2) * amplitud;
}

// ── Modelo de motor: primer orden discreto ───────────────────────────────────
// y[k] = alpha*y[k-1] + (1-alpha)*K*u[k]
float simularMotor(float sp_rpm, float dt_s) {
  float pwm   = constrain(sp_rpm / GANANCIA_MOTOR, 0.0f, 100.0f);
  pwmActual   = pwm;
  float alpha = exp(-dt_s / TAU_MOTOR_S);
  float rpm   = alpha * rpmActual + (1.0f - alpha) * GANANCIA_MOTOR * pwm;
  return max(0.0f, rpm + ruidoGaussiano(RUIDO_RPM));
}

// ── Conectar WiFi ─────────────────────────────────────────────────────────────
void conectarWiFi() {
  Serial.printf("Conectando a WiFi '%s'", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int n = 0;
  while (WiFi.status() != WL_CONNECTED && n++ < 30) {
    delay(500); Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED)
    Serial.printf("\n OK - IP: %s\n", WiFi.localIP().toString().c_str());
  else
    Serial.println("\n ERROR: no se pudo conectar");
}

// ── Callback: recibir setpoint externo ───────────────────────────────────────
void mqttCallback(char* topic, byte* payload, unsigned int len) {
  String msg = "";
  for (unsigned int i = 0; i < len; i++) msg += (char)payload[i];
  if (String(topic) == TOPIC_SETPOINT) {
    float sp = msg.toFloat();
    if (sp > 0 && sp <= 700) {
      setpoint = sp;
      Serial.printf("<- Setpoint externo: %.1f RPM\n", setpoint);
    }
  }
}

// ── Conectar MQTT ─────────────────────────────────────────────────────────────
void conectarMQTT() {
  Serial.printf("Conectando a MQTT %s:%d ", MQTT_BROKER, MQTT_PORT);
  int n = 0;
  while (!mqttClient.connected() && n++ < 10) {
    if (mqttClient.connect("ESP32-Simulador")) {
      Serial.println("OK");
      mqttClient.subscribe(TOPIC_SETPOINT);
    } else {
      Serial.printf("fallo (rc=%d)\n", mqttClient.state());
      delay(2000);
    }
  }
}

// ── Publicar telemetría ───────────────────────────────────────────────────────
void publicarTelemetria() {
  StaticJsonDocument<256> doc;
  doc["rpm"]      = round(rpmActual * 10) / 10.0;
  doc["setpoint"] = setpoint;
  doc["pwm"]      = round(pwmActual * 10) / 10.0;
  doc["error"]    = round((setpoint - rpmActual) * 10) / 10.0;
  doc["nodo"]     = "ESP32-A";
  doc["paquete"]  = ++contadorPaquetes;

  char buf[256];
  serializeJson(doc, buf);

  if (mqttClient.publish(TOPIC_TELEMETRIA, buf))
    Serial.printf("-> PKT#%lu  rpm:%.1f  sp:%.1f  pwm:%.1f%%\n",
                  contadorPaquetes, rpmActual, setpoint, pwmActual);
  else
    Serial.println("ERROR: publish fallido");
}

// =============================================================================
void setup() {
  Serial.begin(115200);
  randomSeed(analogRead(0));
  tInicio = millis();

  Serial.println("\n============================================");
  Serial.println("  SIMULADOR MOTOR DC");
  Serial.println("  Universidad Panamericana - IoT Semana 2");
  Serial.println("============================================\n");

  conectarWiFi();
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setBufferSize(512);
  conectarMQTT();

  tUltimoCambio      = millis();
  tUltimaPublicacion = millis();

  Serial.printf("\nSetpoint inicial: %.1f RPM\n", setpoint);
  Serial.println("Publicando en: iot/motor/telemetria\n");
}

// =============================================================================
void loop() {
  if (!mqttClient.connected()) conectarMQTT();
  mqttClient.loop();

  uint32_t ahora = millis();

  // Cambio automático de setpoint
  if ((ahora - tUltimoCambio) >= DURACION_SETPOINT_MS) {
    idxSetpoint   = (idxSetpoint + 1) % N_SETPOINTS;
    setpoint      = SETPOINTS[idxSetpoint];
    tUltimoCambio = ahora;
    Serial.printf("\n-- Nuevo setpoint: %.1f RPM --\n\n", setpoint);
  }

  // Simular motor y publicar
  if ((ahora - tUltimaPublicacion) >= INTERVALO_PUB_MS) {
    rpmActual          = simularMotor(setpoint, INTERVALO_PUB_MS / 1000.0f);
    publicarTelemetria();
    tUltimaPublicacion = ahora;
  }
}
