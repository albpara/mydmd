# Instrucciones para ESP32 LED Matrix DMD Project

## Contexto del Proyecto
- **Hardware**: ESP32 Dev Module + HUB75 LED Matrix Panel (64x32 píxeles)
- **Framework**: Arduino con PlatformIO
- **Idioma**: Español en comentarios y mensajes
- **Objetivo**: Sistema de control WiFi para matriz LED con modos clock y texto

## Stack Tecnológico
- Librerías clave:
  - ESP32-HUB75-MatrixPanel-I2S-DMA v3.0.14
  - ESPAsyncWebServer v3.6.0
  - Adafruit GFX v1.12.5
  - Standard C time.h (para NTP)

## Convenciones de Código
### TextSize y Dimensiones
- **textSize 1**: charWidth = 5 píxeles
- **textSize 2**: charWidth = 10 píxeles
- **Panel**: PANEL_WIDTH = 64, PANEL_HEIGHT = 32
- **Centrado X**: `int centerX = max(0, (PANEL_WIDTH - textWidth) / 2);`
- Para strings de longitud fija, usar: `int numChars = <length>;`

### Display
- Modo 0: Clock (HH:MM:SS, magenta 255,100,255, textSize 2)
- Modo 1: Text (rainbow scrolling, textSize 1)
- Clock solo se muestra si `wifiConnected == true`
- Fallback a text si clock seleccionado pero sin WiFi

### WiFi
- Soft AP: 192.168.4.1 (RetroPixelLED)
- STA: intenta conectarse a red guardada
- NTP: `configTime(0, 0, "pool.ntp.org", "time.nist.gov")`

### Persistencia
- Storage: NVS Preferences
- Keys usadas:
  - "ssid", "password" (WiFi credentials con bullets de ocultar)
  - "scrollText" (texto a mostrar)
  - "modeClock", "modeText", "modeInterval" (configuración de modos)

### API Endpoints
- `/` - Portal HTML
- `/api/connect` - POST WiFi connect
- `/api/text` - GET texto guardado
- `/api/save-text` - POST guardar texto
- `/api/get-modes` - GET configuración de modos
- `/api/update-modes` - POST actualizar modos
- `/api/reboot` - POST reiniciar

### HTML Portal
- Single-page minified (~3.5KB)
- 3 fieldsets: WiFi, Display Text, Display Modes
- Usa FormData + fetch para comunicación

## Pasos Antes de Compilar
1. Verificar que displayClock() usa charWidth correcto para el textSize
2. Asegurar que centerX cálculos usan `max(0, ...)` para evitar negativos
3. Verificar que todas las funciones de modo tienen validación WiFi donde aplique

## Compilación y Upload
```bash
platformio run -e esp32dev                    # Solo compilar
platformio run -e esp32dev -t upload          # Compilar y subir
platformio device monitor -e esp32dev         # Ver serial
```

## Notas Importantes
- Suponemos que hay un monitor serial activo: esperar o matar proceso antes de upload
- No cambiar PANEL_WIDTH ni PANEL_HEIGHT (hardcoded en matrix initialization)
- NTP sync toma ~10 segundos en primer conecta WiFi
- El usuario prefiere instrucciones en español
