#ifndef BLUETOOTH_MENU_H
#define BLUETOOTH_MENU_H

#include "display.h"
#include "CONFIG.h"
#include "interface/interface.h"

#define BLUETOOTH_MENU_ITEM_COUNT 4
static const char* bluetoothMenuItemsOriginal[] = {"BLE Scan", "BLE Mouse", "Keyboard", "HID"};
static const char* bluetoothMenuItemsList[] = {"BLE Scanner", "BLE Mouse", "BLE Keyboard", "HID Scripts"};
static const SubmenuItems bluetoothSubmenuItems = {
  bluetoothMenuItemsOriginal, bluetoothMenuItemsList, BLUETOOTH_MENU_ITEM_COUNT
};

inline void displayBluetoothMenu(DisplayType &display, byte menuIndex, int previousIndex = -1) {
  displaySubmenu(display, bluetoothSubmenuItems, menuIndex, previousIndex);
}

#endif
