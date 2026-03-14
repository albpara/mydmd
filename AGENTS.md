# RetroPixelLED — ESP32 HUB75 LED Matrix Controller

## Project Overview

**RetroPixelLED** is an ESP32-based firmware that drives two daisy-chained 64×32 HUB75 RGB LED panels (128×32 total) with WiFi connectivity, a captive portal for configuration, MQTT integration with Home Assistant, SD card GIF playback, and multiple configurable display modes.

The project is built with **PlatformIO + Arduino framework** and written in **C++**. Comments, serial logs, and UI text are in **Spanish**.

## Hardware

- **MCU**: ESP32 Dev Module
- **Display**: 2× HUB75 RGB LED panels (64×32 each), chained as 128×32
- **Storage**: SD card via SPI (for GIF files)
- **Power**: USB-C (brightness limited to ~10% / value 16 for safe USB power)

### Pin Mapping (HUB75)

| Signal | GPIO |
|--------|------|
| R1 | 25 |
| G1 | 26 |
| B1 | 27 |
| R2 | 14 |
| G2 | 12 |
| B2 | 13 |
| A | 33 |
| B | 32 |
| C | 22 |
| D | 17 |
| CLK | 16 |
| LAT | 4 |
| OE | 15 |

### SD Card SPI Pins

| Signal | GPIO |
|--------|------|
| CS | 5 |
| CLK | 18 |
| MOSI | 23 |
| MISO | 19 |

## Build System

- **PlatformIO** with `esp32dev` board
- Upload speed: 921600, Monitor speed: 115200
- Build flag: `-DMQTT_MAX_PACKET_SIZE=512`

### Dependencies (`platformio.ini`)

| Library | Purpose |
|---------|---------|
| Adafruit GFX @^1.11.8 | Graphics primitives |
| ESP32-HUB75-MatrixPanel-DMA (mrcodetastic) | HUB75 DMA display driver |
| ESPAsyncWebServer @^3.6.0 | Non-blocking HTTP server |
| AsyncTCP @^1.1.1 | Async TCP support |
| PubSubClient @^2.8.0 | MQTT client |
| AnimatedGIF @^2.1.1 | GIF decoding |

### Build & Flash Commands

```bash
pio run -e esp32dev           # Compile
pio run -t upload -e esp32dev # Flash
pio device monitor -b 115200  # Serial monitor
```

## Architecture

The codebase is organized as a set of header-based modules in `src/`, all orchestrated by `main.cpp`:

```
src/
├── main.cpp           # Entry point: setup(), loop(), mode cycling, boot flow
├── config.h           # Pin definitions, display constants, GIF config
├── display_manager.h  # HSV colors, scroll text, clock display, service text
├── wifi_manager.h     # WiFi STA/AP management, NTP sync, credential persistence
├── web_server.h       # HTTP REST API routes (AsyncWebServer)
├── portal_html.h      # Captive portal HTML/CSS/JS (PROGMEM string)
├── mqtt_manager.h     # MQTT client, Home Assistant auto-discovery, callbacks
├── sd_manager.h       # SD card init, file listing
├── gif_manager.h      # GIF inventory, playback, splash screen, service GIFs
data/
└── splash.gif         # Boot splash animation displayed during WiFi/NTP sync
```

All modules use `extern` forward declarations to share global state. There is no class-based abstraction — everything is function-based with global variables.

## Display Modes

The firmware cycles between three configurable display modes:

| Mode | ID | Description |
|------|----|-------------|
| Clock | 0 | `HH:MM:SS` with blinking colons, magenta color (255,100,255), textSize 2. Requires WiFi + NTP sync. |
| Text | 1 | Rainbow-colored horizontally scrolling text, textSize 2. Text is configurable via web portal or MQTT. |
| GIF | 2 | Plays random GIF files from the SD card `/gifs/` directory, listed in `/lista.txt`. |

- Each mode has an independent configurable duration (1–300 seconds).
- Modes can be individually enabled/disabled.
- If clock mode is enabled but WiFi is unavailable, it falls back to text mode.
- **Service text/GIF** (triggered via MQTT) takes priority over all normal modes.

### Boot Sequence

1. Initialize HUB75 matrix display
2. Initialize SD card and GIF manager
3. Play `splash.gif` during boot (loops until WiFi/NTP ready)
4. Start WiFi (STA with saved credentials, always start Soft AP for config)
5. Start async web server and DNS spoofing (for captive portal)
6. Initialize MQTT
7. Once WiFi connects → sync NTP (non-blocking), connect MQTT, start mDNS (`dmd.local`)
8. Exit boot phase when NTP syncs or WiFi times out (30s)

## WiFi & Captive Portal

- **Soft AP SSID**: `RetroPixelLED` (no password by default in AP mode)
- **Soft AP IP**: `192.168.4.1`
- Credentials stored in NVS Flash via `Preferences` library (namespace: `"wifi"`)
- On boot: attempts STA connection with saved credentials; always starts Soft AP in parallel
- DNS spoofing redirects all domains to `192.168.4.1` when in AP mode
- Auto-reconnect every 30 seconds if WiFi drops
- mDNS: device accessible at `http://dmd.local` when connected to network

### Web Portal

Single-page responsive HTML/CSS/JS portal (stored as PROGMEM C string in `portal_html.h`) with sections for:

- **WiFi Configuration**: scan networks, select/enter SSID, connect
- **Display Text**: set scrolling text (max 50 chars)
- **Display Modes**: enable/disable clock and text modes, set durations
- **MQTT/Home Assistant**: configure broker, port, credentials, client name, topic prefix

## REST API Endpoints

| Method | Path | Description |
|--------|------|-------------|
| GET | `/` | Serve captive portal HTML |
| GET | `/api/scan-wifi` | Scan and return available WiFi networks (JSON) |
| POST | `/api/connect-wifi` | Connect to WiFi (params: `ssid`, `password`) |
| GET | `/api/wifi-status` | Return current WiFi connection status |
| GET | `/api/get-text` | Get current scroll text |
| POST | `/api/update-text` | Update scroll text (param: `text`, max 50 chars) |
| GET | `/api/get-modes` | Get display mode configuration |
| POST | `/api/update-modes` | Update display modes (params: `clock`, `text`, `clockDuration`, `textDuration`) |
| POST | `/api/reboot` | Reboot the ESP32 |
| GET | `/api/mqtt-config` | Get MQTT configuration |
| POST | `/api/configure-mqtt` | Update MQTT settings (params: `broker`, `port`, `username`, `password`, `clientname`, `prefix`) |

## MQTT & Home Assistant Integration

### Connection

- MQTT client: PubSubClient
- Broker/port/credentials configurable via web portal or NVS
- Default topic prefix: `retropixel`
- Default client name: `RetroPixelLED`
- Device ID derived from ESP32 MAC address
- Reconnect interval: 15 seconds
- State publish interval: 30 seconds
- Diagnostics publish interval: 60 seconds

### Home Assistant Auto-Discovery

The firmware publishes MQTT Discovery messages to create entities automatically in Home Assistant:

| Entity Type | Name | Description |
|-------------|------|-------------|
| Light | Display | Power on/off the display |
| Switch | Mode Clock | Enable/disable clock mode |
| Switch | Mode Text | Enable/disable text mode |
| Switch | Mode GIF | Enable/disable GIF mode |
| Number | Duration Clock | Clock mode duration (1–300s) |
| Number | Duration Text | Text mode duration (1–300s) |
| Number | Duration GIF | GIF mode duration (1–300s) |
| Sensor | IP Address | Device IP (diagnostic) |
| Sensor | Uptime | Seconds since boot (diagnostic) |
| Sensor | Link Quality | WiFi RSSI in dBm (diagnostic) |
| Button | Reboot | Remote reboot |

### MQTT Topics (default prefix `retropixel`)

| Topic | Direction | Payload |
|-------|-----------|---------|
| `retropixel/display/power/set` | Subscribe | `ON` / `OFF` |
| `retropixel/display/state` | Publish | `ON` / `OFF` |
| `retropixel/modes/{clock,text,gif}/set` | Subscribe | `ON` / `OFF` |
| `retropixel/modes/{clock,text,gif}/state` | Publish | `ON` / `OFF` |
| `retropixel/modes/{clockDuration,textDuration,gifDuration}/set` | Subscribe | Integer (1–300) |
| `retropixel/modes/{clockDuration,textDuration,gifDuration}/state` | Publish | Integer |
| `retropixel/text/set` | Subscribe | JSON `{"text": "...", "duration": N}` |
| `retropixel/system/reboot` | Subscribe | `true` |
| `retropixel/system/ip/state` | Publish | IP string |
| `retropixel/system/uptime/state` | Publish | Seconds |
| `retropixel/system/link_quality/state` | Publish | dBm |

The `text/set` topic also supports GIF paths: if the `text` value starts with `/` and ends with `.gif`, it triggers a service GIF playback from the SD card instead of text display.

## SD Card & GIF Playback

- SD card is mounted via SPI at boot
- GIF files are stored anywhere on the SD card
- A file `/lista.txt` on the SD root lists all GIF paths (one per line)
- GIFs are selected randomly for mode 2; the index is looked up from `lista.txt`
- `splash.gif` in `data/` is uploaded to the SD card root and plays during boot
- Max single GIF play time: 30 seconds (`GIF_MAX_DURATION`)
- Service GIFs (MQTT-triggered) loop for their specified duration

## Persistent Storage (NVS Preferences)

| Namespace | Key | Type | Description |
|-----------|-----|------|-------------|
| `wifi` | `ssid` | String | Saved WiFi SSID |
| `wifi` | `password` | String | Saved WiFi password |
| `wifi` | `scrollText` | String | Display scroll text |
| `wifi` | `modeClock` | Bool | Clock mode enabled |
| `wifi` | `modeText` | Bool | Text mode enabled |
| `wifi` | `modeGif` | Bool | GIF mode enabled |
| `wifi` | `clockDur` | Int | Clock duration (seconds) |
| `wifi` | `textDur` | Int | Text duration (seconds) |
| `wifi` | `gifDur` | Int | GIF duration (seconds) |
| `mqtt` | `broker` | String | MQTT broker address |
| `mqtt` | `port` | UInt | MQTT broker port |
| `mqtt` | `username` | String | MQTT username |
| `mqtt` | `password` | String | MQTT password |
| `mqtt` | `clientname` | String | MQTT client name |
| `mqtt` | `prefix` | String | MQTT topic prefix |

## Key Conventions

- **Language**: Spanish in all UI text, serial logs, comments, and documentation
- **Code style**: Function-based (no classes), global state shared via `extern`
- **Display math**: `textSize 1` = 6px/char width, `textSize 2` = 12px/char width. Always use `max(0, ...)` for centering to avoid negative coords.
- **Non-blocking design**: WiFi connection, NTP sync, GIF frames, and MQTT are all non-blocking in `loop()`
- **Loop delay**: 50ms (`LOOP_DELAY_MS`)
