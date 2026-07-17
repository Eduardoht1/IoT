import serial
import csv

# Cambia 'COM3' por el puerto donde está conectado tu ESP32 
# (En Linux/Mac suele ser '/dev/ttyUSB0' o '/dev/cu.usbserial')
puerto = 'COM5'
baudios = 115200
archivo_csv = 'datos_motor.csv'

try:
    ser = serial.Serial(puerto, baudios)
    print(f"Conectado a {puerto}. Esperando datos del ESP32...")
    
    with open(archivo_csv, mode='w', newline='') as file:
        writer = csv.writer(file)
        
        while True:
            # Leer línea del puerto serial
            line = ser.readline().decode('utf-8').strip()
            print(line)
            
            # Escribir la línea en el archivo CSV
            writer.writerow(line.split(','))
            
            # Si el ESP32 deja de enviar datos (o llega a los 2000 ms), puedes detener el script manualmente con Ctrl+C
            
except Exception as e:
    print(f"Error: {e}")