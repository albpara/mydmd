#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

// Configuración de pines HUB75
#define R1_PIN 25
#define G1_PIN 26
#define B1_PIN 27
#define R2_PIN 14
#define G2_PIN 12
#define B2_PIN 13
#define A_PIN 33
#define B_PIN 32
#define C_PIN 22
#define D_PIN 17
#define CLK_PIN 16
#define LAT_PIN 4
#define OE_PIN 15

// Matríz: 2 paneles de 64x32 = 128x32
#define PANEL_WIDTH 64
#define PANEL_HEIGHT 32

MatrixPanel_I2S_DMA *dma_display = nullptr;
float colorHue = 0.0;

// Convertir HSV a RGB para transiciones suaves
uint16_t hsvToRGB(float hue) {
  float h = fmod(hue, 360.0);
  float s = 1.0;  // Saturación completa
  float v = 1.0;  // Brillo completo
  
  float c = v * s;
  float x = c * (1.0 - fabs(fmod(h / 60.0, 2.0) - 1.0));
  float m = v - c;
  
  float r, g, b;
  if (h < 60) { r = c; g = x; b = 0; }
  else if (h < 120) { r = x; g = c; b = 0; }
  else if (h < 180) { r = 0; g = c; b = x; }
  else if (h < 240) { r = 0; g = x; b = c; }
  else if (h < 300) { r = x; g = 0; b = c; }
  else { r = c; g = 0; b = x; }
  
  int red = (int)((r + m) * 255);
  int green = (int)((g + m) * 255);
  int blue = (int)((b + m) * 255);
  
  return dma_display->color565(red, green, blue);
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n\nIniciando matriz LED HUB75...");

  // Configurar HUB75
  HUB75_I2S_CFG mxconfig(
    PANEL_WIDTH,      // ancho de cada panel
    PANEL_HEIGHT,     // alto
    2                 // número de paneles encadenados
  );

  // Optimizaciones para reducir glitches y artefactos
  mxconfig.i2sspeed = HUB75_I2S_CFG::HZ_10M;  // Velocidad I2S
  mxconfig.latch_blanking = 2;                 // Anti-ghosting (1-4, 2 es buen balance)
  mxconfig.min_refresh_rate = 90;              // Tasa mínima de refresco (Hz)
  mxconfig.clkphase = false;                   // Sincronización

  // Configurar pines
  mxconfig.gpio.r1 = R1_PIN;
  mxconfig.gpio.g1 = G1_PIN;
  mxconfig.gpio.b1 = B1_PIN;
  mxconfig.gpio.r2 = R2_PIN;
  mxconfig.gpio.g2 = G2_PIN;
  mxconfig.gpio.b2 = B2_PIN;
  mxconfig.gpio.a = A_PIN;
  mxconfig.gpio.b = B_PIN;
  mxconfig.gpio.c = C_PIN;
  mxconfig.gpio.d = D_PIN;
  mxconfig.gpio.clk = CLK_PIN;
  mxconfig.gpio.lat = LAT_PIN;
  mxconfig.gpio.oe = OE_PIN;

  // Inicializar matriz
  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  dma_display->begin();
  dma_display->setTextColor(65535);  // Blanco por defecto

  // Establecer brillo al 10% (25/255)
  dma_display->fillScreen(0);
  dma_display->setBrightness(25);

  Serial.println("Matriz inicializada correctamente");
  delay(500);

  Serial.println("Sistema listo - mostrando texto con colores arcoíris");
}

void loop() {
  // Limpiar pantalla
  dma_display->fillScreen(0);

  // Setear tamaño de texto
  dma_display->setTextSize(1);
  
  // Obtener color actual del arcoíris (suave transición)
  uint16_t currentColor = hsvToRGB(colorHue);
  dma_display->setTextColor(currentColor);

  // Mostrar texto en primer panel (izquierda)
  dma_display->setCursor(10, 8);
  dma_display->println("PANEL 1");
  dma_display->setCursor(15, 20);
  dma_display->println("TEST");

  // Mostrar texto en segundo panel (derecha) con color desfasado
  uint16_t nextColor = hsvToRGB(fmod(colorHue + 120, 360.0));
  dma_display->setTextColor(nextColor);
  dma_display->setCursor(PANEL_WIDTH + 10, 8);
  dma_display->println("PANEL 2");
  dma_display->setCursor(PANEL_WIDTH + 15, 20);
  dma_display->println("DEMO");

  // Incrementar hue suavemente (1 grado cada 30ms = 360 grados en ~11 segundos)
  colorHue += 2.0;
  if (colorHue >= 360.0) colorHue = 0.0;
  
  // Actualizar cada 30ms para transiciones suaves
  delay(30);
}
