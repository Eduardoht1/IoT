#include <Arduino.h>

//Gpios
#define AIN2 13 //clockwise 52
#define AIN1 26 //counterclockwise 52
#define Amarillo 27 // Encoder (ignorado por ahora)
#define Azul 4      // Encoder (ignorado por ahora)

void setup() {
  Serial.begin(115200);
  pinMode(AIN1, OUTPUT);
  pinMode(AIN2, OUTPUT);
  pinMode(Amarillo, INPUT);
  pinMode(Azul, INPUT);

  Serial.println("Iniciando prueba de PWM minimo (Reversa) en 3 segundos...");
  delay(3000);
}

void loop() {
  // Invertimos el sentido de giro: AIN1 en LOW, AIN2 recibe el PWM
  digitalWrite(AIN1, LOW);

  Serial.println("Comenzando rampa de aceleracion (Reversa)...");

  for (int pwm_value = 40; pwm_value <= 255; pwm_value++) {
    analogWrite(AIN2, pwm_value);
    
    Serial.print("Potencia PWM actual: ");
    Serial.println(pwm_value);

    // Retardo de 100ms para observar la reaccion
    delay(500); 
  }

  // Frenamos el motor apagando la señal en AIN2
  analogWrite(AIN2, 0);
  Serial.println("Prueba terminada. El motor se detiene.");
  
  delay(5000);
}