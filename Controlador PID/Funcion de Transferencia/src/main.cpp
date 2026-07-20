#include <Arduino.h>

// Gpios
#define AIN2 13 // clockwise 
#define AIN1 26 // counterclockwise 
#define Amarillo 27 // Encoder Canal A
#define Azul 4      // Encoder Canal B

const float PPR = 11.0; 

volatile long encoderPulses = 0;
unsigned long previousMillis = 0;
const int interval = 20; 

bool testRunning = false;
unsigned long startTime = 0;
const int testDuration = 20000; // Aumentado a 20 segundos para permitir varios pasos de 4s

// Variables para el comportamiento aleatorio
unsigned long previousStepMillis = 0;
int stepInterval = 4000; // Tiempo inicial antes del primer cambio
int currentPwm = 0; 

void IRAM_ATTR countPulses() {
  if (digitalRead(Azul) > 0) {
    encoderPulses++;
  } else {
    encoderPulses--;
  }
}

// Función que genera los cambios bruscos solo en magnitud
void changeMotorCommand() {
  // Limita el PWM entre 77 y 128 (el 129 es exclusivo)
  currentPwm = random(77, 129);

  // Aplica la potencia únicamente en la dirección de AIN2 (Adelante)
  digitalWrite(AIN1, LOW);
  analogWrite(AIN2, currentPwm);
  
  // Asigna un tiempo aleatorio de por lo menos 4 segundos (4000ms a 5000ms)
  stepInterval = random(4000, 5001);
}

void setup() {
  Serial.begin(115200);
  pinMode(AIN1, OUTPUT);
  pinMode(AIN2, OUTPUT);
  pinMode(Amarillo, INPUT_PULLUP);
  pinMode(Azul, INPUT_PULLUP);
  
  attachInterrupt(digitalPinToInterrupt(Amarillo), countPulses, RISING);

  digitalWrite(AIN1, LOW);
  digitalWrite(AIN2, LOW);

  // Alimentar la semilla del generador aleatorio usando ruido eléctrico
  randomSeed(analogRead(34));

  delay(3000);
  
  // Actualizamos el encabezado
  Serial.println("Tiempo(ms),RPM");
  
  startTime = millis();
  previousMillis = startTime;
  previousStepMillis = startTime;
  testRunning = true;
  
  // Lanzar el primer escalón aleatorio
  changeMotorCommand();
}

void loop() {
  if (testRunning) {
    unsigned long currentMillis = millis();
    
    // 1. ¿Es hora de cambiar la velocidad?
    if (currentMillis - previousStepMillis >= stepInterval) {
       changeMotorCommand();
       previousStepMillis = currentMillis;
    }

    // 2. ¿Es hora de tomar una muestra de RPM? (Cada 20 ms)
    if (currentMillis - previousMillis >= interval) {
      unsigned long elapsedTime = currentMillis - startTime;
      
      noInterrupts(); 
      long currentPulses = encoderPulses;
      encoderPulses = 0; 
      interrupts();

      // Cálculo de RPM forzado a ser siempre positivo usando abs()
      float rpm = abs(((float)currentPulses / PPR) * (60000.0 / interval) / 49.0);

      // Imprime solo el tiempo y las RPM
      Serial.print(elapsedTime);
      Serial.print(",");
      Serial.print(currentPwm);
      Serial.print(",");
      Serial.println(rpm); 

      previousMillis = currentMillis;

      // 3. ¿Se acabó el tiempo de prueba?
      if (elapsedTime >= testDuration) {
        digitalWrite(AIN1, LOW);
        analogWrite(AIN2, 0); 
        testRunning = false;
      }
    }
  }
}