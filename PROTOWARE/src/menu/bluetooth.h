#ifndef BLUETOOTH_MENU_H
#define BLUETOOTH_MENU_H

#include "display.h"
#include "CONFIG.h"
#include "interface/interface.h"

#define BLUETOOTH_MENU_ITEM_COUNT 4
static const char* bluetoothMenuItems[] = {"BLE Scan", "BLE Mouse", "BLE Keyboard", "HID"};
static const SubmenuItems bluetoothSubmenuItems = {
  bluetoothMenuItems, bluetoothMenuItems, BLUETOOTH_MENU_ITEM_COUNT
};

inline void displayBluetoothMenu(DisplayType &display, byte menuIndex, int previousIndex = -1) {
  displaySubmenu(display, bluetoothSubmenuItems, menuIndex, previousIndex);
}

#endif
