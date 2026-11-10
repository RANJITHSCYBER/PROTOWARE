#ifndef FIRMWARE_MENU_H
#define FIRMWARE_MENU_H

#include "display.h"
#include "CONFIG.h"
#include "interface/interface.h"

#define FIRMWARE_MENU_ITEM_COUNT 5
static const char* firmwareMenuItems[] = {"System Info", "Flash Stats", "OTA Update", "Reboot", "Version"};

inline void displayFirmwareMenu(DisplayType &display, byte menuIndex, int previousIndex = -1) {
  displaySubmenu(display, firmwareMenuItems, FIRMWARE_MENU_ITEM_COUNT, menuIndex, previousIndex);
}

#endif

