# Portal Cautivo WiFi - RetroPixelLED

## 📖 Descripción

Este proyecto implementa un **portal cautivo web** que permite configurar la conexión WiFi del dispositivo ESP32 de forma fácil e intuitiva.

## 🎯 Características Principales

✅ **Portal Cautivo Automático** - Se muestra automáticamente al conectarse al Soft AP  
✅ **Escaneo de Redes** - Lista todas las redes WiFi disponibles  
✅ **Interfaz Moderna** - Diseño responsive con gradientes y animaciones  
✅ **Almacenamiento Persistente** - Guarda las credenciales en memoria interna (Preferences)  
✅ **Reconexión Automática** - Intenta reconectarse con las credenciales guardadas al reiniciar  
✅ **Sistema Híbrido** - Combina matriz LED con WiFi Manager  

## 🔧 Cómo Funciona

### Modo 1: Primera Conexión (Sin Credenciales Guardadas)

1. El ESP32 inicia en modo **Soft AP (Punto de Acceso)**
2. Se activar el servidor DNS y web
3. El usuario ve la red WiFi: **"RetroPixelLED"** (contraseña: **12345678**)
4. Al conectarse, el portal cautivo se abre automáticamente
5. El usuario selecciona su red WiFi y proporciona la contraseña
6. El dispositivo se conecta y guarda las credenciales

### Modo 2: Conexiones Posteriores (Con Credenciales Guardadas)

1. El dispositivo intenta conectar con las credenciales guardadas
2. Si se conecta exitosamente:
   - Cambia a modo **STA (Cliente)**
   - Detiene el servidor DNS y web
   - La matriz LED muestra información de conexión
3. Si falla la conexión:
   - Vuelve a modo Soft AP
   - Portal cautivo disponible nuevamente

## 📋 Configuración

### SSID del Soft AP
```cpp
const char* WIFI_SSID = "RetroPixelLED";
```

### Contraseña del Soft AP
```cpp
const char* WIFI_PASSWORD = "12345678";
```

### IP del Portal
```
192.168.4.1
```

## 🌐 Acceso al Portal

### Cuando está en Modo Soft AP:

1. En tu teléfono o computadora, busca la red: **"RetroPixelLED"**
2. Conectate con la contraseña: **"12345678"**
3. Abre tu navegador e ingresa: **192.168.4.1** o cualquier dirección web
4. Se redirigirá automáticamente al portal cautivo

### Interfaz del Portal

El portal proporciona:

- **Lista de Redes WiFi**: Escanea y muestra todas las redes disponibles con su intensidad de señal
- **Entrada Manual**: Permite ingresar manualmente un SSID
- **Campo de Contraseña**: Para la contraseña de la red WiFi
- **Botón Conectar**: Intenta conectar con las credenciales ingresadas
- **Indicador de Estado**: Muestra si la conexión fue exitosa o falló

## 📡 APIs REST Disponibles

### 1. GET `/api/scan-wifi`

Retorna lista de redes WiFi disponibles:

```json
{
  "networks": [
    {"ssid": "MiRed1", "rssi": -45},
    {"ssid": "MiRed2", "rssi": -60},
    {"ssid": "MiRed3", "rssi": -75}
  ]
}
```

### 2. POST `/api/connect-wifi`

Conecta a una red WiFi:

**Request:**
```json
{
  "ssid": "MiRed",
  "password": "micontrasena"
}
```

**Response (Exitoso):**
```json
{
  "success": true,
  "message": "Conectando a WiFi..."
}
```

**Response (Error):**
```json
{
  "success": false,
  "message": "Parámetros inválidos"
}
```

## 🔌 Pines Utilizados (HUB75)

| Pin | Función |
|-----|---------|
| 25 | R1 |
| 26 | G1 |
| 27 | B1 |
| 14 | R2 |
| 12 | G2 |
| 13 | B2 |
| 33 | A |
| 32 | B |
| 22 | C |
| 17 | D |
| 16 | CLK |
| 4 | LAT |
| 15 | OE |

## 📚 Librerías Requeridas

Las siguientes librerías se instalan automáticamente vía PlatformIO:

```ini
adafruit/Adafruit GFX Library@^1.11.8
https://github.com/mrcodetastic/ESP32-HUB75-MatrixPanel-DMA.git
ESPAsyncWebServer@^1.3.3
AsyncTCP@^1.1.1
DNSServer@^1.1.1
```

## 🛠️ Compilación y Subida

### Con PlatformIO (Recomendado)

```bash
# Compilar
pio run -e esp32dev

# Subir firmware
pio run -t upload -e esp32dev

# Monitor serial
pio device monitor -e esp32dev
```

## 📊 Monitoreo Serial

El dispositivo emite mensajes de depuración en el puerto serie:

```
=== Iniciando RetroPixelLED ===
✓ Matriz LED inicializada

=== Iniciando Portal Cautivo WiFi ===
Intentando conectar con credenciales guardadas...
.................
Conectado a WiFi: MiRed
IP: 192.168.1.100
=== Sistema listo ===
```

O si no hay credenciales:

```
=== Iniciando Portal Cautivo WiFi ===
Iniciando Modo Punto de Acceso (Soft AP)...

✓ Punto de acceso iniciado
SSID: RetroPixelLED
Contraseña: 12345678
IP: 192.168.4.1

Configurando servidor web...
✓ Servidor web iniciado en puerto 80
✓ Servidor DNS iniciado
=== Sistema listo ===
```

## 🔐 Seguridad

- **SSID**: Se recomienda cambiar "RetroPixelLED" por un nombre único
- **Contraseña**: Se recomienda cambiar "12345678" por una contraseña más segura
- **HTTPS**: El portal actual usa HTTP. Para producción, considera agregar certificados SSL
- **Autenticación**: Las credenciales se almacenan sin encriptación. Considera usar encriptación en NVS

## 🐛 Solución de Problemas

### El portal no aparece

1. **Verifica la conexión al Soft AP**:
   - Abre tu gestor de redes
   - Busca "RetroPixelLED"
   - Ingresa la contraseña: "12345678"

2. **Usa la IP directamente**:
   - Abre navegador: `192.168.4.1`

3. **Revisa los logs seriales**:
   - Abre el monitor serial a 115200 baud
   - Busca mensajes de error

### El dispositivo no se conecta a WiFi

1. **Verifica la contraseña**: Asegúrate de escribirla correctamente (distingue mayúsculas/minúsculas)
2. **Reinicia el dispositivo**: Desconecta y vuelve a conectar la alimentación
3. **Limpia las credenciales**: Modifica el código para no cargar credenciales guardadas:
   ```cpp
   preferences.begin("wifi", false);
   preferences.clear(); // Limpia todo
   preferences.end();
   ```

### La matriz LED no se muestra

1. Verifica la conexión de pines HUB75
2. Comprueba que el cableado no esté dañado
3. Revisa en el monitor serial que la matriz se inicialice correctamente

## 📝 Personalización

### Cambiar el SSID del Soft AP

```cpp
const char* WIFI_SSID = "MiDispositivo";
```

### Cambiar la Contraseña del Soft AP

```cpp
const char* WIFI_PASSWORD = "mi_contrasena_segura";
```

### Cambiar Colores del Portal

En la función `getPortalHTML()`, modifica los valores de `background` y `#667eea`:

```css
background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
```

### Cambiar el Texto de la Matriz

```cpp
const char* scrollText = "TU TEXTO AQUI";
```

## 🚀 Características Futuras (Roadmap)

- [ ] Panel de configuración avanzada
- [ ] Almacenamiento de múltiples redes WiFi
- [ ] Encriptación de credenciales (AES)
- [ ] Interfaz web con estadísticas de conexión
- [ ] Control remoto de la matriz LED vía web
- [ ] Soporte para WPA3
- [ ] OTA (Over-The-Air) updates

## 📞 Soporte

Para más información sobre:
- **ESPAsyncWebServer**: https://github.com/me-no-dev/ESPAsyncWebServer
- **ESP32-HUB75-MatrixPanel-DMA**: https://github.com/mrcodetastic/ESP32-HUB75-MatrixPanel-DMA
- **Arduino Preferences**: https://github.com/espressif/arduino-esp32/tree/master/libraries/Preferences

---

**Autor**: Basado en mejores prácticas de portal cautivo WiFi para ESP32  
**Versión**: 1.0  
**Licencia**: MIT
