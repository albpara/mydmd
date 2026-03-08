# Control DMD LED Panels - ESP32 HUB75

## Descripción
Ejemplo simple para controlar 2 paneles DMD LED (64x32 cada uno) conectados como 128x32 a un ESP32 usando la librería HUB75.

## Configuración

### Hardware
- 2x Paneles DMD LED RGB (64x32 cada uno)
- ESP32
- Conexión USB-C (alimentación limitada - 10% brillo)

### Conexión de pines (HUB75)

| Pin Panel | Pin ESP32 | Función |
|-----------|-----------|---------|
| R1 | GPIO 25 | Rojo Superior |
| G1 | GPIO 26 | Verde Superior |
| B1 | GPIO 27 | Azul Superior |
| R2 | GPIO 14 | Rojo Inferior |
| G2 | GPIO 12 | Verde Inferior |
| B2 | GPIO 13 | Azul Inferior |
| A | GPIO 33 | Fila A |
| B | GPIO 32 | Fila B |
| CLK | GPIO 16 | Reloj |
| LAT | GPIO 4 | Latch |
| OE | GPIO 15 | Output Enable |
| D | GPIO 17 | Fila D |
| C | GPIO 22 | Fila C |
| E | GND | GND |

## Cómo usar

### PlatformIO (recomendado)
```bash
pio run --target upload
pio device monitor
```

### Arduino IDE
1. Instalar librería: "ESP32 HUB75 LED MATRIX PANEL DMA Display" (mrcodetastic)
2. Copiar `main.cpp` a tu .ino
3. Subir a ESP32

## Funcionalidad actual
- Panel izquierdo: ROJO
- Panel derecho: VERDE
- Brillo: 10% (seguro para alimentación USB-C)

## Próximos pasos
- Animaciones
- Control por WiFi/Bluetooth
- Efectos de desplazamiento
