# ⚙️ Resumen Técnico - Portal Cautivo WiFi

## 📦 Cambios Realizados

### 1. Actualización de `platformio.ini`

Se agregaron las siguientes librerías:

```ini
lib_deps =
    adafruit/Adafruit GFX Library@^1.11.8
    https://github.com/mrcodetastic/ESP32-HUB75-MatrixPanel-DMA.git
    ESPAsyncWebServer@^1.3.3
    AsyncTCP@^1.1.1
    DNSServer@^1.1.1
```

- **ESPAsyncWebServer**: Servidor web no bloqueante
- **AsyncTCP**: Soporte TCP asincrónico
- **DNSServer**: DNS spoofing para el portal cautivo

### 2. Reescritura de `src/main.cpp`

El archivo fue completamente restructurado para incluir:

#### A. Variables Globales del Portal

```cpp
DNSServer dnsServer;                    // Servidor DNS para redirección
AsyncWebServer server(80);              // Servidor web en puerto 80
Preferences preferences;                // Almacenamiento persistente
String ssidWifi, passwordWifi;          // Credenciales almacenadas
bool wifiConnected = false;             // Estado de conexión
```

#### B. Funciones del Portal Cautivo

| Función | Propósito |
|---------|-----------|
| `getPortalHTML()` | Retorna HTML/CSS/JS del portal cautivo |
| `initWiFiManager()` | Inicializa WiFi (STA o AP) y carga credenciales |
| `setupAsyncServer()` | Configura rutas HTTP y APIs |
| `setupDNS()` | Inicia servidor DNS para redirección |

#### C. Rutas HTTP Implementadas

```
GET  /                    → Portal HTML principal
GET  /api/scan-wifi       → Escanea y retorna redes WiFi disponibles
POST /api/connect-wifi    → Conecta a una red WiFi
GET  /*                   → Redirige al portal (para captura cautiva)
```

#### D. Lógica de Conexión WiFi

**Al iniciar:**
1. Intenta conectar con credenciales guardadas (máx 10 segundos)
2. Si fallá → inicia Soft AP y portal cautivo
3. Si logra conectar → cambia a modo STA y detiene el portal

**En el loop:**
1. Procesa solicitudes DNS (si está en Soft AP)
2. Monitorea estado de conexión WiFi
3. Reintentar conexión cada 30 segundos si hay credenciales

## 🎨 Portal Web - Características

### HTML/CSS/JavaScript (incrustado en C++)

- **Responsive Design**: Se adapta a móviles y escritorio
- **Gradiente de Fondo**: Púrpura (#667eea → #764ba2)
- **Escaneo de Redes**: Fetch async a `/api/scan-wifi`
- **Selección Simple**: Click para seleccionar red
- **Validaciones**: Verifica SSID y contraseña no vacíos
- **Indicadores de Estado**: Éxito/error con estilos visuales
- **Animación de Carga**: Spinner durante conexión

### Elementos Visuales

```
┌─────────────────────────────────┐
│   🎮 RetroPixelLED              │
│   Configurar conexión WiFi      │
├─────────────────────────────────┤
│                                 │
│ Red WiFi Disponibles            │
│ ┌─────────────────────────────┐ │
│ │ MiRed1        -45 dBm       │ │
│ │ MiRed2        -60 dBm       │ │
│ │ MiRed3        -75 dBm       │ │
│ └─────────────────────────────┘ │
│                                 │
│ Ingresa SSID manualmente        │
│ [_____________________]         │
│                                 │
│ Contraseña WiFi                 │
│ [•••••••••••••••••••]           │
│                                 │
│ [    Conectar     ]             │
│                                 │
└─────────────────────────────────┘
```

## 🔄 Flujo de Datos

```
Usuario se conecta al Soft AP "RetroPixelLED"
        ↓
Abre navegador (192.168.4.1 o cualquier URL)
        ↓
DNS intercepta y redirige a 192.168.4.1
        ↓
Carga portal HTML/CSS/JS
        ↓
JavaScript hace GET a /api/scan-wifi
        ↓
ESP32 ejecuta WiFi.scanNetworks() y retorna JSON
        ↓
Usuario selecciona red e ingresa contraseña
        ↓
JavaScript hace POST a /api/connect-wifi
        ↓
ESP32 valida, guarda en Preferences y conecta
        ↓
WiFi.begin(ssid, password) inicia conexión
        ↓
Loop supervisa WiFi.status() → si conectado, detiene portal
        ↓
Dispositivo listo con conexión normal
```

## 💾 Almacenamiento de Credenciales

Las credenciales se guardan en **NVS Flash** usando `Preferences`:

```cpp
preferences.begin("wifi", false);
preferences.putString("ssid", ssid);
preferences.putString("password", password);
preferences.end();
```

**Cómo se cargan:**
```cpp
preferences.begin("wifi", false);
String ssid = preferences.getString("ssid", "");
String password = preferences.getString("password", "");
preferences.end();
```

**Ubicación en memoria**: `nvs_namespace: "wifi"`

## 🔐 Consideraciones de Seguridad

### Actual (HTTP)
- ✅ Conexión local en red privada (192.168.4.0/24)
- ✅ Contraseña en Soft AP protege acceso
- ❌ Sin HTTPS (credenciales en texto plano)
- ❌ Credenciales sin encriptación en NVS

### Mejoras Futuras
1. Implementar HTTPS con certificados auto-firmados
2. Encriptar credenciales con AES en NVS
3. Agregar timeout de sesión
4. Limitar intentos de conexión fallida

## 📊 Gestión de Memoria

### RAM Estimado

| Componente | Tamaño |
|-----------|--------|
| Portal HTML/CSS/JS | ~15 KB |
| Buffers de JSON | ~2 KB |
| Strings WiFi | <1 KB |
| ESPAsyncWebServer | ~8 KB |
| DNSServer | ~2 KB |
| **Total Portal** | ~28 KB |

ESP32 tiene 256 KB SRAM → Suficiente espacio

### Flash Estimado

| Componente | Tamaño |
|-----------|--------|
| archivo main.cpp compilado | ~150 KB |
| Librerías ESP32 core | ~2.5 MB |
| **Total** | ~2.65 MB de 4 MB |

Soporta OTA updates en futuro

## 🧪 Casos de Prueba Recomendados

### 1. Primera Conexión
- [ ] Softwere inicia en Soft AP
- [ ] Red "RetroPixelLED" visible
- [ ] Conexión exitosa a Soft AP
- [ ] Portal se abre automáticamente
- [ ] Escaneo muestra redes disponibles
- [ ] Conexión a red exitosa

### 2. Reconexión
- [ ] Reiniciar dispositivo
- [ ] Conecta automáticamente a red guardada
- [ ] Portal se detiene
- [ ] Matriz muestra información

### 3. Cambio de Red
- [ ] Conectarse nuevamente al Soft AP
- [ ] Ingresar nuevas credenciales
- [ ] Credenciales anteriores sobrescritas
- [ ] Conecta a nueva red

### 4. Red Inaccesible
- [ ] Apagar red WiFi guardada
- [ ] Dispositivo falla conexión
- [ ] Reintenta conexión
- [ ] Vuelve a Soft AP después de timeout

## 🎯 Integración con RedPixelLED Existente

La matriz LED continúa funcionando:
- **Antes de WiFi**: Muestra texto al seleccionar red
- **Después de WiFi**: Continúa animación arcoíris
- **Durante conexión**: Muestra estado WiFi pequeño

Sin conflictos con el servidor web gracias a:
- Loop asincrónico (no bloqueante)
- Procesamiento DNS en background
- Delay() suficiente para actualizaciones

## 🚀 Deploy y Uso

### Compilación
```bash
cd e:\development\myWorkingDmd
pio run -e esp32dev
```

### Subida
```bash
pio run -t upload -e esp32dev
```

### Monitor
```bash
pio device monitor -e esp32dev -b 115200
```

### Acceso Usuario Final
1. Conectarte a WiFi: **RetroPixelLED** (contraseña: **12345678**)
2. Abre navegador → irá a portal automáticamente
3. Selecciona red, ingresa contraseña, conecta
4. ¡Listo! Dispositivo con WiFi configurado

---

**Última actualización**: 9 de Marzo de 2026  
**Estado**: ✅ Implementado y listo para testing
