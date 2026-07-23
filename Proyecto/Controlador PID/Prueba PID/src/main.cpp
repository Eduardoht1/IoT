#include <Arduino.h>

// Gpios
#define AIN2 13 // clockwise 
#define AIN1 26 // counterclockwise 
#define Amarillo 27 // Encoder Canal A
#define Azul 4      // Encoder Canal B

// Relación de transmisión y encoder
const float PPR = 11.0; 
const float GEAR_RATIO = 35.0;
const float REAL_PPR = PPR * GEAR_RATIO; 

volatile long encoderPulses = 0;
unsigned long previousMillis = 0;
const int interval = 20; // Tiempo de muestreo (20 ms)
const float dt = interval / 1000.0; // dt en segundos (0.02s)

// --- Constantes del Controlador PI ---
float Kp = 1.5; // 3.788
float Ki = 12.5; // 27.45

// --- Variables del PID ---
float setpoint_rpm = 75.0; // Velocidad objetivo 140
float integral = 0;

void IRAM_ATTR countPulses() {
  if (digitalRead(Azul) > 0) {
    encoderPulses++;
  } else {
    encoderPulses--;
  }
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

  // Se actualiza el encabezado para reflejar el porcentaje
  Serial.println("Target,RPM,PWM,Error(%)");
  delay(3000);
}

void loop() {
  unsigned long currentMillis = millis();
  
  // Se ejecuta exactamente cada 20 ms
  if (currentMillis - previousMillis >= interval) {
    
    // 1. Leer las RPM actuales
    noInterrupts(); 
    long currentPulses = encoderPulses;
    encoderPulses = 0; 
    interrupts();

    // Calculamos las RPM absolutas
    float rpm = abs(((float)currentPulses / REAL_PPR) * (60.0 / dt));

    // 2. Calcular el error absoluto y el error en porcentaje
    float error = setpoint_rpm - rpm;
    float error_porcentaje = 0.0;
    
    // Protección matemática contra división por cero
    if (setpoint_rpm != 0.0) {
      error_porcentaje = (error / setpoint_rpm) * 100.0;
    }

    // 3. Calcular la parte Integral con Anti-Windup (usa el error absoluto para la matemática del controlador)
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

    // 7. Enviar datos para el Serial Plotter
    Serial.print("Target:");
    Serial.print(setpoint_rpm);
    Serial.print(",");
    Serial.print("RPM:");
    Serial.print(rpm);
    Serial.print(",");
    Serial.print("PWM:");
    Serial.print(pwm_output/255.0*100.0); // Normalizamos a porcentaje
    Serial.print("%,");
    Serial.print(",");
    Serial.print("Error(%):");
    Serial.println(error_porcentaje); 

    previousMillis = currentMillis;
  }
}