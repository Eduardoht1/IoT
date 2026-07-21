// Motor DC con PID + publicacion MQTT a InfluxDB/Grafana
// Universidad Panamericana - IoT Semana 2

//docker exec iot-mosquitto mosquitto_pub -h localhost -p 1883 -t "iot/motor/setpoint" -m "10"

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

// Gpios (Alineados con tu lógica de control)
#define AIN2 13 // clockwise (usado como PWM)
#define AIN1 26 // counterclockwise 
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
float setpoint = 70.0; // Velocidad objetivo inicial
float rpm = 0.0;
float integral = 0.0;
float pwmSalida = 0.0;

// Variables del sensor Hall
volatile long encoderPulses = 0;
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

// Temporizadores
const int interval = 20; // Tiempo de muestreo del PID (20 ms)
const float dt = interval / 1000.0; // dt en segundos (0.02s)
unsigned long previousMillis = 0;

#define T_MQTT_MS 500  // Publicar a MQTT cada 500 ms
uint32_t tMQTT = 0;

// MQTT
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
bool mqttListo = false;

// ISR: cuenta pulsos del sensor usando tu lógica direccional
void IRAM_ATTR countPulses() {
  portENTER_CRITICAL_ISR(&mux);
  if (digitalRead(Azul) > 0) {
    encoderPulses++;
  } else {
    encoderPulses--;
  }
  portEXIT_CRITICAL_ISR(&mux);
}

// Lógica de control unificada (Lectura RPM, Error, Anti-Windup y PWM)
void ejecutarCicloPID() {
  // 1. Leer las RPM actuales
  long currentPulses;
  portENTER_CRITICAL(&mux);
  currentPulses = encoderPulses;
  encoderPulses = 0; 
  portEXIT_CRITICAL(&mux);

  // Calculamos las RPM absolutas
  rpm = abs(((float)currentPulses / REAL_PPR) * (60.0 / dt));

  // 2. Calcular el error absoluto
  float error = setpoint - rpm;

  // 3. Calcular la parte Integral con Anti-Windup
  integral += error * dt;
  
  float max_integral = 255.0 / Ki;
  if (integral > max_integral) integral = max_integral;
  if (integral < -max_integral) integral = -max_integral;

  // 4. Ecuación del Controlador PI
  float control_signal = (Kp * error) + (Ki * integral);

  // 5. Saturación (Limitar la señal matemática al mundo real 0-255)
  int pwm_output = (int)control_signal;
  
  if (pwm_output > 255) pwm_output = 255;
  if (pwm_output < 0) pwm_output = 0; 

  // 6. Aplicar la potencia al driver del motor
  digitalWrite(AIN1, LOW);
  analogWrite(AIN2, pwm_output);
  
  // Guardar la variable para que se publique en Grafana
  pwmSalida = (float)pwm_output; 
}

// Conectar WiFi
void conectarWiFi() {
  Serial.printf("Conectando a WiFi '%s'...", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int n = 0;
  while (WiFi.status() != WL_CONNECTED && n++ < 40) {
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\n OK IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("\n FALLO WiFi - continuando sin MQTT");
  }
}

// Callback MQTT: recibir nuevo setpoint desde Grafana
void mqttCallback(char* topic, byte* payload, unsigned int len) {
  String msg = "";
  for (unsigned int i = 0; i < len; i++) msg += (char)payload[i];
  
  if (String(topic) == TOPIC_SETPOINT) {
    float sp = msg.toFloat();
    if (sp >= 0 && sp <= 700) {
      setpoint = sp;
      // Reset integral al cambiar setpoint
      integral = 0;
      Serial.printf("<-- Nuevo setpoint: %.1f RPM\n", setpoint);
    }
  }
}

// Conectar al broker MQTT
void conectarMQTT() {
  if (WiFi.status() != WL_CONNECTED) return;
  int n = 0;
  while (!mqttClient.connected() && n++ < 5) {
    Serial.print("Conectando MQTT...");
    if (mqttClient.connect("ESP32-Motor-PID")) {
      mqttClient.subscribe(TOPIC_SETPOINT);
      mqttListo = true;
      Serial.println(" OK");
    } else {
      Serial.printf(" fallo (rc=%d)\n", mqttClient.state());
      delay(1000);
    }
  }
}

// Publicar telemetria a MQTT (-> Telegraf -> InfluxDB)
void publicarTelemetria() {
  if (!mqttListo || !mqttClient.connected()) return;
  
  // Construir el JSON que Telegraf leera
  StaticJsonDocument<200> doc;
  doc["rpm"] = round(rpm * 10.0f) / 10.0f;
  doc["setpoint"] = setpoint;
  doc["error"] = round((setpoint - rpm) * 10.0f) / 10.0f;
  doc["pwm"] = round(pwmSalida);
  doc["nodo"] = "ESP32-Motor";
  
  char buf[200];
  serializeJson(doc, buf);
  mqttClient.publish(TOPIC_TELEMETRIA, buf);
}

// ====================================================================

void setup() {
  Serial.begin(115200);
  
  // Pines del motor
  pinMode(AIN1, OUTPUT);
  pinMode(AIN2, OUTPUT);
  digitalWrite(AIN1, LOW);
  digitalWrite(AIN2, LOW);
  
  // Sensor Hall
  pinMode(Amarillo, INPUT_PULLUP);
  pinMode(Azul, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(Amarillo), countPulses, RISING);
  
  // WiFi y MQTT (el PID arranca aunque falle la conexion)
  conectarWiFi();
  if (WiFi.status() == WL_CONNECTED) {
    mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
    mqttClient.setCallback(mqttCallback);
    mqttClient.setBufferSize(512);
    conectarMQTT();
  }
  
  previousMillis = millis();
  tMQTT = millis();
  
  Serial.println("Sistema listo");
  Serial.printf("Setpoint inicial: %.1f RPM\n", setpoint);
}

void loop() {
  // Mantener conexion MQTT activa
  if (mqttListo) mqttClient.loop();
  
  unsigned long currentMillis = millis();
  
  // 1. Loop de Control PID (cada 20 ms estrictos)
  if (currentMillis - previousMillis >= interval) {
    ejecutarCicloPID();
    previousMillis = currentMillis;
  }
  
  // 2. Loop de Publicación a MQTT (cada 500 ms)
  if ((currentMillis - tMQTT) >= T_MQTT_MS) {
    publicarTelemetria();
    tMQTT = currentMillis;
    
    // Reconectar MQTT si se cayo
    if (mqttListo && !mqttClient.connected()) {
      conectarMQTT();
    }
  }
}