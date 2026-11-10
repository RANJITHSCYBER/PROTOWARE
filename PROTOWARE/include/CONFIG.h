#ifndef PROTOWARE_CONFIG_H
#define PROTOWARE_CONFIG_H

#include "pindefs.h"

// Display Configuration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADR 0x3C
#define OLED_RESET -1
#define OLED_SCL 22
#define OLED_SDA 21

// Buttons mapped to fixed custom PCB layout
#define BUTTON_UP      BUTTON_PIN_UP      // 26 (Up)
#define BUTTON_DOWN    BUTTON_PIN_DOWN    // 33 (Down)
#define BUTTON_OK      BUTTON_PIN_RIGHT   // 27 (Select / OK)
#define BUTTON_BACK    BUTTON_PIN_LEFT    // 25 (Back)
#define BUTTON_CENTER  BUTTON_PIN_CENTER  // 32 (Exit / Center)
#define BUTTON_EXIT    BUTTON_PIN_CENTER  // 32

// SD Card mapped to hardware SPI + CS 13
#define SD_CLK   SPI_SCK_PIN   // 18
#define SD_MISO  SPI_MISO_PIN  // 19
#define SD_MOSI  SPI_MOSI_PIN  // 23

// Firmware version
static const char* FIRMWARE = "v1.0-PROTOWARE";

#endif // PROTOWARE_CONFIG_H
