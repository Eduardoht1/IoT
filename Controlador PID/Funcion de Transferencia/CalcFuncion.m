% 1. Importar los datos del CSV
datos = readtable('datos_motor.csv');

% Extraer las columnas como vectores
t = datos{:, 1} / 1000; %Segundos
u = datos{:, 2}; % Entrada (Señal de control: PWM)
y = datos{:, 3}; % Salida (Respuesta de la planta: RPM)

% 2. Crear el objeto iddata
% El tercer argumento es tu tiempo de muestreo (Ts). 
Ts = 0.02; 
motor_data = iddata(y, u, Ts);

% (Opcional) Eliminar el retraso medio o "detrending" 
% motor_data = detrend(motor_data); 

% 3. Estimar la función de transferencia
% Un motor DC clásico se modela como un sistema de primer orden 
% (1 polo, 0 ceros) para la velocidad.
num_polos = 1; 
num_ceros = 0; 
sys = tfest(motor_data, num_polos, num_ceros);

% 4. Resultados
% Imprime la función de transferencia en la consola
disp('Función de transferencia del motor:')
disp(sys)

% Genera una gráfica para comparar qué tan bien se ajusta 
% el modelo matemático a tus datos reales de la prueba
figure;
compare(motor_data, sys);
grid on;
title('Comparación de Datos Reales vs. Modelo Estimado');