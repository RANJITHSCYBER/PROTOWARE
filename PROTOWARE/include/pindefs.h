#ifndef PINDEFS_H
#define PINDEFS_H

#include <Arduino.h>

// Navigation Buttons (Soldered on Custom PCB - PRESERVED & FIXED)
#define BUTTON_PIN_UP      26
#define BUTTON_PIN_DOWN    33
#define BUTTON_PIN_CENTER  32  // Exit / Center
#define BUTTON_PIN_LEFT    25  // Back
#define BUTTON_PIN_RIGHT   27  // Select / OK

// SPI & SD Card Pins (Soldered on Custom PCB - PRESERVED & FIXED)
#define SPI_SCK_PIN        18
#define SPI_MISO_PIN       19
#define SPI_MOSI_PIN       23
#define SD_CS              13

// I2C OLED Display
#define OLED_SDA_PIN       21
#define OLED_SCL_PIN       22
#define OLED_I2C_ADDR      0x3C

#endif // PINDEFS_H

