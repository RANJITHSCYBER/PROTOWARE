#ifndef GPIO_MENU_H
#define GPIO_MENU_H

#include "display.h"
#include "CONFIG.h"
#include "interface/interface.h"

#define GPIO_MENU_ITEM_COUNT 10

static const char* gpioMenuItemsOriginal[] = {
  "Servo",
  "Sine Wave",
  "Square Wave",
  "Triangle",
  "Blink LED",
  "Flash LED",
  "Motor",
  "Relay",
  "NeoPixel",
  "Button"
};

static const char* gpioMenuItemsList[] = {
  "Servo",
  "Sine wave",
  "Square wave",
  "Triangle wave",
  "Blink led",
  "Flash led",
  "Motor driver",
  "Relay",
  "Neo pixel",
  "Push button"
};

static const SubmenuItems gpioSubmenuItems = {
  gpioMenuItemsOriginal, gpioMenuItemsList, GPIO_MENU_ITEM_COUNT
};

inline void displayGPIOMenu(DisplayType &display, byte menuIndex, int previousIndex = -1) {
  displaySubmenu(display, gpioSubmenuItems, menuIndex, previousIndex);
}

#endif // GPIO_MENU_H
