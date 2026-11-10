#ifndef WIFI_MENU_H
#define WIFI_MENU_H

#include "display.h"
#include "CONFIG.h"
#include "interface/interface.h"

#define WIFI_MENU_ITEM_COUNT 4
static const char* wifiMenuItemsOriginal[] = {"WiFi Scan", "OTA Web", "Connect", "Logs"};
static const char* wifiMenuItemsList[] = {"WiFi Scan", "OTA Web Portal", "Connect WiFi", "Saved Logs"};
static const SubmenuItems wifiSubmenuItems = {
  wifiMenuItemsOriginal, wifiMenuItemsList, WIFI_MENU_ITEM_COUNT
};

inline void displayWiFiMenu(DisplayType &display, byte menuIndex, int previousIndex = -1) {
  displaySubmenu(display, wifiSubmenuItems, menuIndex, previousIndex);
}

#endif
