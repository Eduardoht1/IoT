// Motor DC con PID + publicacion MQTT a InfluxDB/Grafana
// Universidad Panamericana - IoT Semana 2

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// WiFi
const char* WIFI_SSID     = "LaptopEdu";       
const char* WIFI_PASSWORD = "G65n72R98a02E05";    

// MQTT (IP de la laptop con Docker)
const char* MQTT_BROKER = "192.168.137.1";       
const int MQTT_PORT = 1883;

const char* TOPIC_TELEMETRIA = "iot/motor/telemetria";
const char* TOPIC_SETPOINT   = "iot/motor/setpoint";
const char* TOPIC_DIRECCION  = "iot/motor/direccion"; // <-- Nuevo tópico

// Gpios
#define AIN2 13 // Usado como PWM/Dirección A
#define AIN1 26 // Usado como PWM/Dirección B 
#define Amarillo 27 // Encoder Canal A
#define Azul 4      // Encoder Canal B

// Relación de transmisión y encoder
const float PPR = 11.0; 
const float GEAR_RATIO = 35.0;
const float REAL_PPR = PPR * GEAR_RATIO; 

// Constantes del Controlador PI
float Kp = 1.5;   
float Ki = 12.5;    

// Variables del PID
float setpoint = 70.0;
float rpm = 0.0;
float integral = 0.0;
float pwmSalida = 0.0;
int direccion = 1; // 1 = CW, 0 = CCW <-- Variable de estado direccional

// Variables del sensor Hall
volatile long encoderPulses = 0;
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

// Temporizadores
const int interval = 20; // 20 ms (50 Hz)
const float dt = interval / 1000.0;
unsigned long previousMillis = 0;

#define T_MQTT_MS 500  
uint32_t tMQTT = 0;

// MQTT
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
bool mqttListo = false;

// ISR: cuenta pulsos
void IRAM_ATTR countPulses() {
  portENTER_CRITICAL_ISR(&mux);
  if (digitalRead(Azul) > 0) {
    encoderPulses++;
  } else {
    encoderPulses--;
  }
  portEXIT_CRITICAL_ISR(&mux);
}

// Lógica de control unificada
void ejecutarCicloPID() {
  long currentPulses;
  portENTER_CRITICAL(&mux);
  currentPulses = encoderPulses;
  encoderPulses = 0; 
  portEXIT_CRITICAL(&mux);

  rpm = abs(((float)currentPulses / REAL_PPR) * (60.0 / dt));
  float error = setpoint - rpm;

  integral += error * dt;
  float max_integral = 255.0 / Ki;
  if (integral > max_integral) integral = max_integral;
  if (integral < -max_integral) integral = -max_integral;

  float control_signal = (Kp * error) + (Ki * integral);
  int pwm_output = (int)control_signal;
  
  if (pwm_output > 255) pwm_output = 255;
  if (pwm_output < 0) pwm_output = 0; 

  // Lógica de puente H bidireccional
  if (direccion == 1) {
    analogWrite(AIN1, 0);          // Apaga canal contrario
    analogWrite(AIN2, pwm_output); // Inyecta PWM
  } else {
    analogWrite(AIN2, 0);          
    analogWrite(AIN1, pwm_output); 
  }
  
  pwmSalida = (float)pwm_output; 
}

void conectarWiFi() {
  Serial.printf("Conectando a WiFi '%s'...", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int n = 0;
  while (WiFi.status() != WL_CONNECTED && n++ < 40) {
    delay(500); Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\n OK IP: %s\n", WiFi.localIP().toString().c_str());
  }
}

// Callback MQTT: recibe setpoint o dirección
void mqttCallback(char* topic, byte* payload, unsigned int len) {
  String msg = "";
  for (unsigned int i = 0; i < len; i++) msg += (char)payload[i];
  
  if (String(topic) == TOPIC_SETPOINT) {
    float sp = msg.toFloat();
    if (sp >= 0 && sp <= 200) { // Limitado al rango de la gráfica
      setpoint = sp;
      integral = 0; // Reset windup
      Serial.printf("<-- Nuevo setpoint: %.1f RPM\n", setpoint);
    }
  } 
  else if (String(topic) == TOPIC_DIRECCION) {
    int dir = msg.toInt();
    if (dir == 0 || dir == 1) {
      direccion = dir;
      integral = 0; // Reset por cambio brusco de fase
      Serial.printf("<-- Nueva dirección: %s\n", direccion == 1 ? "CW" : "CCW");
    }
  }
}

void conectarMQTT() {
  if (WiFi.status() != WL_CONNECTED) return;
  int n = 0;
  while (!mqttClient.connected() && n++ < 5) {
    Serial.print("Conectando MQTT...");
    if (mqttClient.connect("ESP32-Motor-PID")) {
      mqttClient.subscribe(TOPIC_SETPOINT);
      mqttClient.subscribe(TOPIC_DIRECCION); // Suscripción a dirección
      mqttListo = true;
      Serial.println(" OK");
    } else {
      delay(1000);
    }
  }
}

void publicarTelemetria() {
  if (!mqttListo || !mqttClient.connected()) return;
  
  StaticJsonDocument<256> doc;
  float error_val = setpoint - rpm;
  
  doc["rpm"] = round(rpm * 10.0f) / 10.0f;
  doc["setpoint"] = setpoint;
  doc["error"] = round(error_val * 10.0f) / 10.0f;
  doc["pwm"] = round(pwmSalida);
  doc["pwm_pct"] = round((pwmSalida / 255.0) * 100.0); // % de Duty Cycle
  
  // % Error relativo (Manejo de div / 0)
  doc["error_pct"] = setpoint > 0 ? round((abs(error_val) / setpoint) * 100.0) : 0; 
  doc["direccion"] = direccion;
  doc["nodo"] = "ESP32-Motor";
  
  char buf[256];
  serializeJson(doc, buf);
  mqttClient.publish(TOPIC_TELEMETRIA, buf);
}

void setup() {
  Serial.begin(115200);
  
  pinMode(AIN1, OUTPUT);
  pinMode(AIN2, OUTPUT);
  digitalWrite(AIN1, LOW);
  digitalWrite(AIN2, LOW);
  
  pinMode(Amarillo, INPUT_PULLUP);
  pinMode(Azul, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(Amarillo), countPulses, RISING);
  
  conectarWiFi();
  if (WiFi.status() == WL_CONNECTED) {
    mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
    mqttClient.setCallback(mqttCallback);
    conectarMQTT();
  }
  
  previousMillis = millis();
  tMQTT = millis();
}

void loop() {
  if (mqttListo) mqttClient.loop();
  
  unsigned long currentMillis = millis();
  
  // Loop PI de control en tiempo discreto (20ms)
  if (currentMillis - previousMillis >= interval) {
    ejecutarCicloPID();
    previousMillis = currentMillis;
  }
  
  if ((currentMillis - tMQTT) >= T_MQTT_MS) {
    publicarTelemetria();
    tMQTT = currentMillis;
    if (mqttListo && !mqttClient.connected()) conectarMQTT();
  }
}