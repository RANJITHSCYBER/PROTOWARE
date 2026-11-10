#ifndef FIRMWARE_MENU_H
#define FIRMWARE_MENU_H

#include "display.h"
#include "CONFIG.h"
#include "interface/interface.h"

#define FIRMWARE_MENU_ITEM_COUNT 5
static const char* firmwareMenuItemsOriginal[] = {"Flash SD", "Sys Info", "OTA Update", "Reboot", "Version"};
static const char* firmwareMenuItemsList[] = {"Flash (.bin)", "System Info", "OTA Update", "Reboot ESP32", "Version"};
static const SubmenuItems firmwareSubmenuItems = {
  firmwareMenuItemsOriginal, firmwareMenuItemsList, FIRMWARE_MENU_ITEM_COUNT
};

inline void displayFirmwareMenu(DisplayType &display, byte menuIndex, int previousIndex = -1) {
  displaySubmenu(display, firmwareSubmenuItems, menuIndex, previousIndex);
}

#endif

