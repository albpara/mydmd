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

  // Llenar primer panel con ROJO (izquierdo)
  dma_display->fillRect(0, 0, PANEL_WIDTH, PANEL_HEIGHT, dma_display->color565(255, 0, 0));
  delay(500);

  // Llenar segundo panel con VERDE (derecho)
  dma_display->fillRect(PANEL_WIDTH, 0, PANEL_WIDTH, PANEL_HEIGHT, dma_display->color565(0, 255, 0));

  Serial.println("Paneles configurados - Rojo a la izquierda, Verde a la derecha");
}

void loop() {
  delay(1000);
  // Por ahora sin cambios en el loop - solo mantener los colores
}
