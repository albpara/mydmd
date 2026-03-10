# 🚀 Guía Rápida - Portal Cautivo WiFi

## ⚡ 5 Pasos Rápidos

### 1️⃣ Compilar
```bash
pio run -e esp32dev
```

### 2️⃣ Subir
```bash
pio run -t upload -e esp32dev
```

### 3️⃣ Verificar Serial
```bash
pio device monitor -e esp32dev -b 115200
```

Deberías ver:
```
=== Iniciando RetroPixelLED ===
✓ Matriz LED inicializada

=== Iniciando Portal Cautivo WiFi ===
Iniciando Modo Punto de Acceso (Soft AP)...
SSID: RetroPixelLED
IP: 192.168.4.1
```

### 4️⃣ Conectarse al Dispositivo
- Abre configuración de WiFi
- Busca: **RetroPixelLED**
- Contraseña: **12345678**
- Conecta

### 5️⃣ Acceder al Portal
- Abre navegador
- Ve a: `192.168.4.1`
- ¡Portal se carga automáticamente!

## 📋 Checklista de Funcionalidad

### Soft AP
- [ ] Red visible como "RetroPixelLED"
- [ ] Se conecta sin problemas
- [ ] IP: 192.168.4.1 funciona

### Portal Web
- [ ] Portal HTML se carga
- [ ] Muestra lista de redes
- [ ] Puedes seleccionar red
- [ ] Campo contraseña funciona

### Conexión WiFi
- [ ] Selecciona red y contraseña
- [ ] Botón "Conectar" responde
- [ ] Muestra mensaje de éxito
- [ ] Dispositivo se conecta

### Reconexión
- [ ] Reinicia dispositivo
- [ ] Conecta automáticamente a red guardada
- [ ] Serial muestra "Conectado a WiFi"

### Matriz LED
- [ ] Texto se desplaza con colores arcoíris
- [ ] No hay conflictos con WiFi
- [ ] Animación fluida (50ms)

## 🔧 Configuración Personalizada

### Cambiar SSID del Soft AP
En `src/main.cpp` línea ~10:
```cpp
const char* WIFI_SSID = "TU_NOMBRE";
```

### Cambiar Contraseña
En `src/main.cpp` línea ~11:
```cpp
const char* WIFI_PASSWORD = "tu_contraseña";
```

### Cambiar Texto de la Matriz
En `src/main.cpp` línea ~476:
```cpp
const char* scrollText = "TU TEXTO";
```

## 🐛 Diagnóstico Rápido

### El portal no aparece
```
1. Verifica estar conectado a "RetroPixelLED"
2. Abre navegador: 192.168.4.1
3. Si aún no va, refresca (F5) varias veces
```

### Los logs seriales muestran error
```
1. Verifica velocidad serial: 115200 baud
2. Mira los mensajes de error exactos
3. Recompila: pio run -e esp32dev
```

### El dispositivo no guarda credenciales
```
1. Verifica que se ve "success: true" en respuesta
2. Revisa Serial para "Guardando credenciales"
3. Limpia NVS: preferences.clear() en setup()
```

## 📁 Archivos que Cambió

```
e:\development\myWorkingDmd\
├── platformio.ini              (actualizado - librerías)
├── src\main.cpp                (reescrito - portal cautivo)
├── CAPTIVE_PORTAL_GUIDE.md     (nuevo - guía completa)
├── TECHNICAL_SUMMARY.md        (nuevo - detalles técnicos)
└── QUICKSTART.md               (este archivo)
```

## 🔌 Hardware Verificado

- ✅ ESP32 Dev Board
- ✅ Matriz HUB75 (2 paneles 64x32)
- ✅ Pines HUB75 configurados
- ✅ Brillo matriz al 10%

## 📊 Especificaciones

| Característica | Valor |
|---|---|
| Soft AP SSID | RetroPixelLED |
| Soft AP Password | 12345678 |
| Soft AP IP | 192.168.4.1 |
| Puerto Web | 80 (HTTP) |
| Puerto DNS | 53 |
| Máximo de redes a listar | 20 |
| Timeout reconexión | 30 segundos |
| Velocidad Serial | 115200 baud |

## 🎨 Personalización CSS

Modifica colores en `getPortalHTML()`:

```cpp
background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
```

Colores populares:
- Rojo-Rosa: `#FF006E → #FB5607`
- Verde-Azul: `#00D9FF → #0096C7`
- Naranja-Rojo: `#F72585 → #FF006E`

## ✅ Cambios pre-subida

Antes de subir el firmware:

1. [ ] Revisa pines HUB75 correctos
2. [ ] Cambia SSID y contraseña si quieres
3. [ ] Compila sin errores: `pio run -e esp32dev`
4. [ ] Sube el firmware: `pio run -t upload -e esp32dev`
5. [ ] Monitorea logs: `pio device monitor`

## 🎯 Próximos Pasos

- [ ] Testar conexión exitosamente
- [ ] Verificar reconexión automática
- [ ] Validar no hay conflictos LED
- [ ] Personalizar SSID y contraseña
- [ ] ¡Desplegar a producción!

## 📞 Archivos de Referencia

1. **CAPTIVE_PORTAL_GUIDE.md** - Documentación completa
2. **TECHNICAL_SUMMARY.md** - Detalles técnicos y arquitectura
3. **src/main.cpp** - Código fuente con comentarios

## 🚀 Ejemplo de Flujo Completo

```
[ Usuario inicia dispositivo ]
          ↓
[ ESP32 intenta conectar con credenciales guardadas ]
          ↓
[ ¿Conectó? SÍ → Modo STA, sin portal ]
          ↓
[ ¿Conectó? NO → Soft AP, portal disponible ]
          ↓
[ Usuario abre WiFi, selecciona "RetroPixelLED" ]
          ↓
[ Usuario abre navegador, ve portal ]
          ↓
[ Lista de redes aparece automáticamente ]
          ↓
[ Usuario selecciona su red e ingresa contraseña ]
          ↓
[ Hace click en "Conectar" ]
          ↓
[ ESP32 guarda credenciales en Preferences ]
          ↓
[ ESP32 se conecta a la red ]
          ↓
[ Portal detecta conexión exitosa ]
          ↓
[ Detiene Soft AP y servidor web ]
          ↓
[ ¡Listo! Dispositivo conectado 🎉 ]
```

---

**¡Feliz codificación! 🚀**
