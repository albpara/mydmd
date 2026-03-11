#ifndef CONFIG_H
#define CONFIG_H

// Matrix Panel Configuration
#define PANEL_WIDTH 64
#define PANEL_HEIGHT 32

// Pin Definitions
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

// Display Settings
#define BRIGHTNESS 16
#define TEXT_SIZE 2
#define DISPLAY_Y_OFFSET 8
#define SCROLL_START_X 128
#define SCROLL_SPEED 1

// Total display width (2 panels)
#define DISPLAY_WIDTH (PANEL_WIDTH * 2)

// Loop Configuration
#define LOOP_DELAY_MS 50

#endif // CONFIG_H
