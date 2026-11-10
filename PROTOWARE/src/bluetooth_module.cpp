#include "bluetooth_module.h"
#include <NimBLEDevice.h>
#include <NimBLEHIDDevice.h>
#include <NimBLEServer.h>
#include <NimBLEAdvertising.h>
#include <SD.h>
#include <vector>
#include <GyverButton.h>

#include "CONFIG.h"
#include "pindefs.h"
#include "display.h"
#include "interface/interface.h"
#include "menu/bluetooth.h"

// External hardware instances from main
extern DisplayType display;
extern GButton buttonUp;
extern GButton buttonDown;
extern GButton buttonOK;
extern GButton buttonBack;
extern GButton buttonCenter;

extern byte submenu;
extern bool inMenu;
extern byte currentMenu;
extern byte bluetoothMenuIndex;
extern SPIClass sdSPI;
extern bool sdCardReady;

extern void returnToMainMenu();
extern void resetActivityTimer();

// =============================================================================
// COMPOSITE HID REPORT DESCRIPTOR (Keyboard Report 1 + Mouse Report 2)
// =============================================================================
static const uint8_t compositeHidReportDescriptor[] = {
    // Keyboard (Report ID 1)
    0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)
    0x09, 0x06,                    // USAGE (Keyboard)
    0xA1, 0x01,                    // COLLECTION (Application)
    0x85, 0x01,                    //   REPORT_ID (1)
    0x05, 0x07,                    //   USAGE_PAGE (Keyboard)
    0x19, 0xE0,                    //   USAGE_MINIMUM (Keyboard LeftControl)
    0x29, 0xE7,                    //   USAGE_MAXIMUM (Keyboard Right GUI)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x25, 0x01,                    //   LOGICAL_MAXIMUM (1)
    0x75, 0x01,                    //   REPORT_SIZE (1)
    0x95, 0x08,                    //   REPORT_COUNT (8)
    0x81, 0x02,                    //   INPUT (Data,Var,Abs)
    0x95, 0x01,                    //   REPORT_COUNT (1)
    0x75, 0x08,                    //   REPORT_SIZE (8)
    0x81, 0x01,                    //   INPUT (Cnst,Ary,Abs)
    0x95, 0x05,                    //   REPORT_COUNT (5)
    0x75, 0x01,                    //   REPORT_SIZE (1)
    0x05, 0x08,                    //   USAGE_PAGE (LEDs)
    0x19, 0x01,                    //   USAGE_MINIMUM (Num Lock)
    0x29, 0x05,                    //   USAGE_MAXIMUM (Kana)
    0x91, 0x02,                    //   OUTPUT (Data,Var,Abs)
    0x95, 0x01,                    //   REPORT_COUNT (1)
    0x75, 0x03,                    //   REPORT_SIZE (3)
    0x91, 0x01,                    //   OUTPUT (Cnst,Ary,Abs)
    0x95, 0x06,                    //   REPORT_COUNT (6)
    0x75, 0x08,                    //   REPORT_SIZE (8)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x25, 0x65,                    //   LOGICAL_MAXIMUM (101)
    0x05, 0x07,                    //   USAGE_PAGE (Keyboard)
    0x19, 0x00,                    //   USAGE_MINIMUM (Reserved)
    0x29, 0x65,                    //   USAGE_MAXIMUM (Keyboard Application)
    0x81, 0x00,                    //   INPUT (Data,Ary,Abs)
    0xC0,                          // END_COLLECTION

    // Mouse (Report ID 2)
    0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)
    0x09, 0x02,                    // USAGE (Mouse)
    0xA1, 0x01,                    // COLLECTION (Application)
    0x85, 0x02,                    //   REPORT_ID (2)
    0x09, 0x01,                    //   USAGE (Pointer)
    0xA1, 0x00,                    //   COLLECTION (Physical)
    0x05, 0x09,                    //     USAGE_PAGE (Button)
    0x19, 0x01,                    //     USAGE_MINIMUM (Button 1)
    0x29, 0x03,                    //     USAGE_MAXIMUM (Button 3)
    0x15, 0x00,                    //     LOGICAL_MINIMUM (0)
    0x25, 0x01,                    //     LOGICAL_MAXIMUM (1)
    0x95, 0x03,                    //     REPORT_COUNT (3)
    0x75, 0x01,                    //     REPORT_SIZE (1)
    0x81, 0x02,                    //     INPUT (Data,Var,Abs)
    0x95, 0x01,                    //     REPORT_COUNT (1)
    0x75, 0x05,                    //     REPORT_SIZE (5)
    0x81, 0x01,                    //     INPUT (Cnst,Ary,Abs)
    0x05, 0x01,                    //     USAGE_PAGE (Generic Desktop)
    0x09, 0x30,                    //     USAGE (X)
    0x09, 0x31,                    //     USAGE (Y)
    0x09, 0x38,                    //     USAGE (Wheel)
    0x15, 0x81,                    //     LOGICAL_MINIMUM (-127)
    0x25, 0x7F,                    //     LOGICAL_MAXIMUM (127)
    0x75, 0x08,                    //     REPORT_SIZE (8)
    0x95, 0x03,                    //     REPORT_COUNT (3)
    0x81, 0x06,                    //     INPUT (Data,Var,Rel)
    0xC0,                          //   END_COLLECTION
    0xC0                           // END_COLLECTION
};

struct MouseReport {
    uint8_t buttons;
    int8_t x;
    int8_t y;
    int8_t wheel;
} __attribute__((packed));

// =============================================================================
// SUBMODULE STATES
// =============================================================================
enum BleState : byte {
    BLE_STATE_MENU = 0,
    BLE_STATE_SCANNING,
    BLE_STATE_SCAN_LIST,
    BLE_STATE_SCAN_DETAILS,
    BLE_STATE_MOUSE,
    BLE_STATE_KEYBOARD,
    BLE_STATE_HID_LIST,
    BLE_STATE_HID_RUNNING
};

static BleState currentBleState = BLE_STATE_MENU;

// -----------------------------------------------------------------------------
// BLE Scan Data
// -----------------------------------------------------------------------------
struct BleDeviceEntry {
    String name;
    String address;
    int rssi;
    String addrType;
};

static const int MAX_BLE_SCAN = 30;
static BleDeviceEntry bleScanList[MAX_BLE_SCAN];
static int totalBleScanned = 0;
static int bleScanSelectedIdx = 0;

// -----------------------------------------------------------------------------
// BLE Server & HID Runtime Objects
// -----------------------------------------------------------------------------
static bool bleInited = false;
static bool bleIsConnected = false;
static NimBLEServer* pBleServer = nullptr;
static NimBLEHIDDevice* pBleHid = nullptr;
static NimBLECharacteristic* pBleKbChar = nullptr;
static NimBLECharacteristic* pBleMouseChar = nullptr;
static NimBLEAdvertising* pBleAdv = nullptr;
static String currentBleDevName = "PROTOWARE";

class ProtowareServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override {
        bleIsConnected = true;
        pServer->updateConnParams(connInfo.getConnHandle(), 6, 6, 0, 30);
        Serial.printf("[BLE] Connected handle=%u\n", connInfo.getConnHandle());
    }
    void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override {
        bleIsConnected = false;
        Serial.printf("[BLE] Disconnected handle=%u, reason=%d\n", connInfo.getConnHandle(), reason);
        if (pBleAdv && (currentBleState == BLE_STATE_MOUSE || currentBleState == BLE_STATE_KEYBOARD || currentBleState == BLE_STATE_HID_LIST || currentBleState == BLE_STATE_HID_RUNNING)) {
            pBleAdv->start();
        }
    }
};

// -----------------------------------------------------------------------------
// HID Transmission Helpers
// -----------------------------------------------------------------------------
static void sendHidKeyboardReport(uint8_t mod, uint8_t key) {
    if (!pBleKbChar || !bleIsConnected) return;
    uint8_t report[8] = {mod, 0, key, 0, 0, 0, 0, 0};
    pBleKbChar->setValue(report, 8);
    pBleKbChar->notify();
    delay(10);
}

static void releaseHidKeyboard() {
    if (!pBleKbChar || !bleIsConnected) return;
    uint8_t report[8] = {0};
    pBleKbChar->setValue(report, 8);
    pBleKbChar->notify();
    delay(10);
}

static void sendHidMouseReport(int8_t x, int8_t y, uint8_t buttons) {
    if (!pBleMouseChar || !bleIsConnected) return;
    MouseReport report = {buttons, x, y, 0};
    pBleMouseChar->setValue(reinterpret_cast<uint8_t*>(&report), sizeof(report));
    pBleMouseChar->notify();
    delay(8);
}

// -----------------------------------------------------------------------------
// BLE Server Lifecycle
// -----------------------------------------------------------------------------
static void stopBLEService() {
    if (!bleInited) return;

    if (pBleAdv && pBleAdv->isAdvertising()) {
        pBleAdv->stop();
    }
    if (pBleServer) {
        for (int i = pBleServer->getConnectedCount() - 1; i >= 0; --i) {
            pBleServer->disconnect(pBleServer->getPeerInfo((uint8_t)i).getConnHandle());
        }
    }
    delay(80);
    if (NimBLEDevice::isInitialized()) {
        NimBLEDevice::deinit(true);
    }
    pBleServer = nullptr;
    pBleHid = nullptr;
    pBleKbChar = nullptr;
    pBleMouseChar = nullptr;
    pBleAdv = nullptr;
    bleInited = false;
    bleIsConnected = false;
    Serial.println("[BLE] Service stopped & deinitialized.");
}

static void startBleHidService(const char* deviceName, uint16_t appearance) {
    stopBLEService();

    currentBleDevName = deviceName;
    Serial.printf("[BLE] Starting HID Service: %s\n", deviceName);

    NimBLEDevice::init(deviceName);
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
    NimBLEDevice::setSecurityAuth(true, false, true);

    pBleServer = NimBLEDevice::createServer();
    static ProtowareServerCallbacks sCallbacks;
    pBleServer->setCallbacks(&sCallbacks);

    pBleHid = new NimBLEHIDDevice(pBleServer);
    pBleKbChar = pBleHid->getInputReport(1);
    pBleMouseChar = pBleHid->getInputReport(2);

    pBleHid->setManufacturer("PROTOWARE");
    pBleHid->setPnp(0x02, 0x045E, 0x0040, 0x0100);
    pBleHid->setHidInfo(0x00, 0x01);
    pBleHid->setReportMap((uint8_t*)compositeHidReportDescriptor, sizeof(compositeHidReportDescriptor));
    pBleHid->setBatteryLevel(100);

    pBleAdv = NimBLEDevice::getAdvertising();
    NimBLEAdvertisementData advData;
    NimBLEAdvertisementData scanData;

    advData.setFlags(0x06);
    advData.setAppearance(appearance);
    advData.addServiceUUID(pBleHid->getHidService()->getUUID());
    scanData.setName(deviceName);

    pBleAdv->setAdvertisementData(advData);
    pBleAdv->setScanResponseData(scanData);
    pBleAdv->setMinInterval(32);
    pBleAdv->setMaxInterval(64);
    pBleAdv->enableScanResponse(true);

    pBleAdv->start();
    bleInited = true;
    bleIsConnected = false;
    Serial.println("[BLE] Advertising started.");
}

// =============================================================================
// 1. BLE SCAN IMPLEMENTATION
// =============================================================================
static bool performBleScan() {
    stopBLEService();

    display.clearDisplay();
    display.setTextColor(SH110X_WHITE);
    display.setTextSize(1);
    display.setCursor(10, 18);
    display.print("Scanning BLE...");
    display.drawRoundRect(10, 36, 108, 12, 3, SH110X_WHITE);
    display.fillRect(12, 38, 40, 8, SH110X_WHITE);
    display.display();

    NimBLEDevice::init("");
    NimBLEScan* pScan = NimBLEDevice::getScan();
    pScan->setActiveScan(true);
    pScan->setInterval(45);
    pScan->setWindow(15);

    totalBleScanned = 0;
    bleScanSelectedIdx = 0;

    pScan->start(4, false);
    NimBLEScanResults results = pScan->getResults();
    int count = results.getCount();
    if (count > MAX_BLE_SCAN) count = MAX_BLE_SCAN;

    for (int i = 0; i < count; i++) {
        const NimBLEAdvertisedDevice* dev = results.getDevice(i);
        if (!dev) continue;

        String name = dev->getName().c_str();
        if (name.length() == 0) {
            name = "[Unknown Device]";
        }
        bleScanList[totalBleScanned].name = name;
        bleScanList[totalBleScanned].address = dev->getAddress().toString().c_str();
        bleScanList[totalBleScanned].rssi = dev->getRSSI();
        bleScanList[totalBleScanned].addrType = dev->getAddress().isPublic() ? "Public" : "Random";
        totalBleScanned++;
    }

    NimBLEDevice::deinit(true);
    return totalBleScanned > 0;
}

static void renderBleScanList(int selected) {
    display.clearDisplay();
    display.setTextColor(SH110X_WHITE);
    display.setTextSize(1);

    // Header
    display.fillRect(0, 0, 128, 11, SH110X_WHITE);
    display.setTextColor(SH110X_BLACK);
    display.setCursor(2, 2);
    display.print("BLE Scan Results (");
    display.print(totalBleScanned);
    display.print(")");

    if (totalBleScanned == 0) {
        display.setTextColor(SH110X_WHITE);
        display.setCursor(14, 28);
        display.print("No BLE devices found");
        display.setCursor(14, 44);
        display.print("< Back to return");
        display.display();
        return;
    }

    int visibleRows = 4;
    int startIdx = 0;
    if (selected >= visibleRows) {
        startIdx = selected - visibleRows + 1;
    }

    int y = 14;
    for (int i = startIdx; i < startIdx + visibleRows && i < totalBleScanned; i++) {
        bool isSel = (i == selected);
        if (isSel) {
            display.fillRect(0, y - 1, 128, 12, SH110X_WHITE);
            display.setTextColor(SH110X_BLACK);
        } else {
            display.setTextColor(SH110X_WHITE);
        }

        String displayName = bleScanList[i].name;
        if (displayName.length() > 12) {
            displayName = displayName.substring(0, 11) + "..";
        }
        display.setCursor(4, y + 1);
        display.print(displayName);

        // RSSI
        display.setCursor(96, y + 1);
        display.print(bleScanList[i].rssi);
        display.print("dB");

        y += 12;
    }

    display.display();
}

static void renderBleDetails(int idx) {
    if (idx < 0 || idx >= totalBleScanned) return;

    display.clearDisplay();
    display.setTextColor(SH110X_WHITE);
    display.setTextSize(1);

    // Title bar
    display.fillRect(0, 0, 128, 11, SH110X_WHITE);
    display.setTextColor(SH110X_BLACK);
    display.setCursor(2, 2);
    display.print("BLE Device Details");

    display.setTextColor(SH110X_WHITE);
    display.setCursor(2, 14);
    display.print("Name: ");
    String s = bleScanList[idx].name;
    if (s.length() > 14) s = s.substring(0, 13) + "..";
    display.print(s);

    display.setCursor(2, 26);
    display.print("MAC:  ");
    display.print(bleScanList[idx].address);

    display.setCursor(2, 38);
    display.print("RSSI: ");
    display.print(bleScanList[idx].rssi);
    display.print(" dBm  (");
    int pct = constrain(2 * (bleScanList[idx].rssi + 100), 0, 100);
    display.print(pct);
    display.print("%)");

    display.setCursor(2, 50);
    display.print("Type: ");
    display.print(bleScanList[idx].addrType);

    // Signal bar
    int barW = map(pct, 0, 100, 0, 36);
    display.drawRect(88, 50, 38, 9, SH110X_WHITE);
    if (barW > 0) {
        display.fillRect(89, 51, barW, 7, SH110X_WHITE);
    }

    display.display();
}

// =============================================================================
// 2. BLE MOUSE IMPLEMENTATION
// =============================================================================
static unsigned long mouseHoldStart = 0;
static unsigned long lastMouseMoveTick = 0;

static void renderBleMouseScreen() {
    display.clearDisplay();
    display.setTextColor(SH110X_WHITE);
    display.setTextSize(1);

    // Header
    display.fillRect(0, 0, 128, 11, SH110X_WHITE);
    display.setTextColor(SH110X_BLACK);
    display.setCursor(2, 2);
    display.print("BLE Mouse");

    // Connection badge
    if (bleIsConnected) {
        display.fillRect(66, 1, 60, 9, SH110X_BLACK);
        display.setTextColor(SH110X_WHITE);
        display.setCursor(68, 2);
        display.print("CONNECTED");
    } else {
        display.setTextColor(SH110X_BLACK);
        display.setCursor(76, 2);
        display.print("PAIRING");
    }

    display.setTextColor(SH110X_WHITE);

    // D-Pad diagram & Buttons visualizer
    bool up = (digitalRead(BUTTON_PIN_UP) == LOW);
    bool down = (digitalRead(BUTTON_PIN_DOWN) == LOW);
    bool left = (digitalRead(BUTTON_PIN_LEFT) == LOW);
    bool right = (digitalRead(BUTTON_PIN_RIGHT) == LOW);
    bool center = (digitalRead(BUTTON_PIN_CENTER) == LOW);

    // Large Visual D-Pad Center
    int cx = 64;
    int cy = 37;

    // Up Box (54, 13, 20, 13)
    if (up) {
        display.fillRoundRect(cx - 10, 13, 20, 13, 3, SH110X_WHITE);
        display.fillTriangle(cx, 15, cx - 6, 22, cx + 6, 22, SH110X_BLACK);
    } else {
        display.drawRoundRect(cx - 10, 13, 20, 13, 3, SH110X_WHITE);
        display.fillTriangle(cx, 15, cx - 6, 22, cx + 6, 22, SH110X_WHITE);
    }

    // Down Box (54, 48, 20, 13)
    if (down) {
        display.fillRoundRect(cx - 10, 48, 20, 13, 3, SH110X_WHITE);
        display.fillTriangle(cx, 58, cx - 6, 51, cx + 6, 51, SH110X_BLACK);
    } else {
        display.drawRoundRect(cx - 10, 48, 20, 13, 3, SH110X_WHITE);
        display.fillTriangle(cx, 58, cx - 6, 51, cx + 6, 51, SH110X_WHITE);
    }

    // Left Box (32, 27, 20, 20)
    if (left) {
        display.fillRoundRect(cx - 32, 27, 20, 20, 3, SH110X_WHITE);
        display.fillTriangle(cx - 29, 37, cx - 17, 31, cx - 17, 43, SH110X_BLACK);
    } else {
        display.drawRoundRect(cx - 32, 27, 20, 20, 3, SH110X_WHITE);
        display.fillTriangle(cx - 29, 37, cx - 17, 31, cx - 17, 43, SH110X_WHITE);
    }

    // Right Box (76, 27, 20, 20)
    if (right) {
        display.fillRoundRect(cx + 12, 27, 20, 20, 3, SH110X_WHITE);
        display.fillTriangle(cx + 29, 37, cx + 17, 31, cx + 17, 43, SH110X_BLACK);
    } else {
        display.drawRoundRect(cx + 12, 27, 20, 20, 3, SH110X_WHITE);
        display.fillTriangle(cx + 29, 37, cx + 17, 31, cx + 17, 43, SH110X_WHITE);
    }

    // Center Click Box (54, 27, 20, 20)
    if (center) {
        display.fillRoundRect(cx - 10, 27, 20, 20, 3, SH110X_WHITE);
        display.fillCircle(cx, 37, 5, SH110X_BLACK);
    } else {
        display.drawRoundRect(cx - 10, 27, 20, 20, 3, SH110X_WHITE);
        display.fillCircle(cx, 37, 4, SH110X_WHITE);
    }

    display.display();
}

static void handleBleMouseInput() {
    bool up = (digitalRead(BUTTON_PIN_UP) == LOW);
    bool down = (digitalRead(BUTTON_PIN_DOWN) == LOW);
    bool left = (digitalRead(BUTTON_PIN_LEFT) == LOW);
    bool right = (digitalRead(BUTTON_PIN_RIGHT) == LOW);

    static MenuButtonState mouseCenterState;
    bool centerPress = isMenuButtonPress(BUTTON_PIN_CENTER, mouseCenterState, 0);

    bool anyMove = up || down || left || right;

    if (!anyMove) {
        mouseHoldStart = 0;
    } else {
        if (mouseHoldStart == 0) {
            mouseHoldStart = millis();
        }

        // Smooth tick rate: 20ms
        if (millis() - lastMouseMoveTick >= 20) {
            lastMouseMoveTick = millis();

            unsigned long held = millis() - mouseHoldStart;
            int8_t speed = 3; // Initial precision speed
            if (held > 250) speed = 6;
            if (held > 600) speed = 10;
            if (held > 1200) speed = 15;

            int8_t dx = 0;
            int8_t dy = 0;
            if (left)  dx -= speed;
            if (right) dx += speed;
            if (up)    dy -= speed;
            if (down)  dy += speed;

            sendHidMouseReport(dx, dy, 0);
            renderBleMouseScreen();
        }
    }

    if (centerPress) {
        // Send Click (press and release)
        sendHidMouseReport(0, 0, 0x01);
        delay(25);
        sendHidMouseReport(0, 0, 0x00);
        renderBleMouseScreen();
    }

    // Refresh pairing status periodically
    static unsigned long lastStatusRefresh = 0;
    if (millis() - lastStatusRefresh >= 800) {
        lastStatusRefresh = millis();
        renderBleMouseScreen();
    }
}

// =============================================================================
// 3. BLE KEYBOARD IMPLEMENTATION
// =============================================================================
static int kbPage = 0; // 0 = Alpha/Numeric, 1 = Special / Function keys
static int kbRow = 0;
static int kbCol = 0;
static String kbLastSent = "None";

// Standard 4 rows
static const char* bleKbRows[4] = {
    "1234567890",
    "qwertyuiop",
    "asdfghjkl;",
    "zxcvbnm,./"
};

// Special keys table for page 1
struct SpecialKey {
    const char* label;
    uint8_t mod;
    uint8_t key;
    int w;
};

static const SpecialKey specKeysRow0[] = {
    {"ESC", 0x00, 0x29, 23}, {"TAB", 0x00, 0x2B, 23}, {"ENT", 0x00, 0x28, 23},
    {"DEL", 0x00, 0x2A, 23}, {"SPC", 0x00, 0x2C, 25}
};
static const SpecialKey specKeysRow1[] = {
    {"HOME", 0x00, 0x4A, 25}, {"END", 0x00, 0x4D, 23}, {"PGUP", 0x00, 0x4B, 26},
    {"PGDN", 0x00, 0x4E, 26}, {"WIN",  0x08, 0x00, 20}
};
static const SpecialKey specKeysRow2[] = {
    {"UP",   0x00, 0x52, 23}, {"DOWN", 0x00, 0x51, 26}, {"LEFT", 0x00, 0x50, 26},
    {"RGHT", 0x00, 0x4F, 26}, {"CTRL", 0x01, 0x00, 21}
};

static void renderBleKeyboardScreen() {
    display.clearDisplay();
    display.setTextColor(SH110X_WHITE);
    display.setTextSize(1);

    // Title bar
    display.fillRect(0, 0, 128, 11, SH110X_WHITE);
    display.setTextColor(SH110X_BLACK);
    display.setCursor(2, 2);
    display.print("BLE Keyboard");

    // Connection badge
    if (bleIsConnected) {
        display.fillRect(66, 1, 60, 9, SH110X_BLACK);
        display.setTextColor(SH110X_WHITE);
        display.setCursor(68, 2);
        display.print("CONNECTED");
    } else {
        display.setTextColor(SH110X_BLACK);
        display.setCursor(76, 2);
        display.print("PAIRING");
    }

    display.setTextColor(SH110X_WHITE);

    // Status: Last sent
    display.setCursor(2, 14);
    display.print("Sent: ");
    display.print(kbLastSent);

    if (kbPage == 0) {
        // Render Alpha/Numeric grid
        for (int r = 0; r < 4; r++) {
            int y = 24 + r * 7;
            for (int c = 0; c < 10; c++) {
                char ch = bleKbRows[r][c];
                int x = 4 + c * 12;
                bool isSel = (kbRow == r && kbCol == c);

                if (isSel) {
                    display.fillRect(x - 1, y - 1, 10, 8, SH110X_WHITE);
                    display.setTextColor(SH110X_BLACK);
                } else {
                    display.setTextColor(SH110X_WHITE);
                }
                display.setCursor(x + 1, y);
                display.print(ch);
            }
        }

        // Bottom Controls: [SPEC] [SPC] [DEL] [ENT] [EXIT]
        const char* btmLabels[5] = {"SPEC", "SPC", "DEL", "ENT", "EXIT"};
        int btmX[5] = {2, 30, 56, 80, 104};
        int btmW[5] = {26, 24, 22, 22, 22};
        int yBtm = 54;

        for (int c = 0; c < 5; c++) {
            bool isSel = (kbRow == 4 && kbCol == c);
            if (isSel) {
                display.fillRect(btmX[c], yBtm, btmW[c], 9, SH110X_WHITE);
                display.setTextColor(SH110X_BLACK);
            } else {
                display.drawRect(btmX[c], yBtm, btmW[c], 9, SH110X_WHITE);
                display.setTextColor(SH110X_WHITE);
            }
            display.setCursor(btmX[c] + 2, yBtm + 1);
            display.print(btmLabels[c]);
        }
    } else {
        // Special Keys Page
        // Row 0
        int y0 = 24;
        int curX = 2;
        for (int c = 0; c < 5; c++) {
            bool isSel = (kbRow == 0 && kbCol == c);
            if (isSel) {
                display.fillRect(curX, y0, specKeysRow0[c].w, 8, SH110X_WHITE);
                display.setTextColor(SH110X_BLACK);
            } else {
                display.drawRect(curX, y0, specKeysRow0[c].w, 8, SH110X_WHITE);
                display.setTextColor(SH110X_WHITE);
            }
            display.setCursor(curX + 2, y0);
            display.print(specKeysRow0[c].label);
            curX += specKeysRow0[c].w + 2;
        }

        // Row 1
        int y1 = 34;
        curX = 2;
        for (int c = 0; c < 5; c++) {
            bool isSel = (kbRow == 1 && kbCol == c);
            if (isSel) {
                display.fillRect(curX, y1, specKeysRow1[c].w, 8, SH110X_WHITE);
                display.setTextColor(SH110X_BLACK);
            } else {
                display.drawRect(curX, y1, specKeysRow1[c].w, 8, SH110X_WHITE);
                display.setTextColor(SH110X_WHITE);
            }
            display.setCursor(curX + 2, y1);
            display.print(specKeysRow1[c].label);
            curX += specKeysRow1[c].w + 2;
        }

        // Row 2
        int y2 = 44;
        curX = 2;
        for (int c = 0; c < 5; c++) {
            bool isSel = (kbRow == 2 && kbCol == c);
            if (isSel) {
                display.fillRect(curX, y2, specKeysRow2[c].w, 8, SH110X_WHITE);
                display.setTextColor(SH110X_BLACK);
            } else {
                display.drawRect(curX, y2, specKeysRow2[c].w, 8, SH110X_WHITE);
                display.setTextColor(SH110X_WHITE);
            }
            display.setCursor(curX + 2, y2);
            display.print(specKeysRow2[c].label);
            curX += specKeysRow2[c].w + 2;
        }

        // Row 3: [123/ABC]  [EXIT]
        int y3 = 54;
        bool selAbc = (kbRow == 3 && kbCol == 0);
        bool selExit = (kbRow == 3 && kbCol == 1);

        if (selAbc) {
            display.fillRect(6, y3, 56, 9, SH110X_WHITE);
            display.setTextColor(SH110X_BLACK);
        } else {
            display.drawRect(6, y3, 56, 9, SH110X_WHITE);
            display.setTextColor(SH110X_WHITE);
        }
        display.setCursor(10, y3 + 1);
        display.print("< 123/ABC");

        if (selExit) {
            display.fillRect(68, y3, 54, 9, SH110X_WHITE);
            display.setTextColor(SH110X_BLACK);
        } else {
            display.drawRect(68, y3, 54, 9, SH110X_WHITE);
            display.setTextColor(SH110X_WHITE);
        }
        display.setCursor(76, y3 + 1);
        display.print("EXIT");
    }

    display.display();
}

static void handleBleKeyboardClick() {
    if (kbPage == 0) {
        if (kbRow < 4) {
            char ch = bleKbRows[kbRow][kbCol];
            uint8_t key = 0;
            uint8_t mod = 0;

            if (ch >= 'a' && ch <= 'z') key = 0x04 + (ch - 'a');
            else if (ch >= '1' && ch <= '9') key = 0x1E + (ch - '1');
            else if (ch == '0') key = 0x27;
            else if (ch == ';') key = 0x33;
            else if (ch == ',') key = 0x36;
            else if (ch == '.') key = 0x37;
            else if (ch == '/') key = 0x38;

            sendHidKeyboardReport(mod, key);
            releaseHidKeyboard();

            kbLastSent = String("'") + ch + "'";
        } else {
            switch (kbCol) {
                case 0: kbPage = 1; kbRow = 0; kbCol = 0; break; // SPEC
                case 1: sendHidKeyboardReport(0, 0x2C); releaseHidKeyboard(); kbLastSent = "[SPACE]"; break;
                case 2: sendHidKeyboardReport(0, 0x2A); releaseHidKeyboard(); kbLastSent = "[DEL]"; break;
                case 3: sendHidKeyboardReport(0, 0x28); releaseHidKeyboard(); kbLastSent = "[ENTER]"; break;
                case 4: // EXIT
                    stopBLEService();
                    currentBleState = BLE_STATE_MENU;
                    displayBluetoothMenu(display, bluetoothMenuIndex);
                    return;
            }
        }
    } else {
        // Special Keys page
        if (kbRow == 0) {
            sendHidKeyboardReport(specKeysRow0[kbCol].mod, specKeysRow0[kbCol].key);
            releaseHidKeyboard();
            kbLastSent = specKeysRow0[kbCol].label;
        } else if (kbRow == 1) {
            sendHidKeyboardReport(specKeysRow1[kbCol].mod, specKeysRow1[kbCol].key);
            releaseHidKeyboard();
            kbLastSent = specKeysRow1[kbCol].label;
        } else if (kbRow == 2) {
            sendHidKeyboardReport(specKeysRow2[kbCol].mod, specKeysRow2[kbCol].key);
            releaseHidKeyboard();
            kbLastSent = specKeysRow2[kbCol].label;
        } else if (kbRow == 3) {
            if (kbCol == 0) {
                kbPage = 0; kbRow = 0; kbCol = 0; // Return to ABC
            } else {
                stopBLEService();
                currentBleState = BLE_STATE_MENU;
                displayBluetoothMenu(display, bluetoothMenuIndex);
                return;
            }
        }
    }
}

// =============================================================================
// 4. HID (BADBLE / DUCKY SCRIPT RUNNER) IMPLEMENTATION
// =============================================================================
static const int MAX_HID_SCRIPTS = 25;
static String hidScriptNames[MAX_HID_SCRIPTS];
static int totalHidScripts = 0;
static int hidSelectedIdx = 0;

static void scanSdForScripts() {
    totalHidScripts = 0;
    hidSelectedIdx = 0;

    if (!sdCardReady) {
        sdSPI.begin(SPI_SCK_PIN, SPI_MISO_PIN, SPI_MOSI_PIN, SD_CS);
        sdCardReady = SD.begin(SD_CS, sdSPI);
    }
    if (!sdCardReady) return;

    // Search folders in order: /scripts, /badkb, /hid, and root /
    const char* folders[4] = {"/scripts", "/badkb", "/hid", "/"};

    for (int f = 0; f < 4 && totalHidScripts < MAX_HID_SCRIPTS; f++) {
        File dir = SD.open(folders[f]);
        if (!dir || !dir.isDirectory()) continue;

        File entry = dir.openNextFile();
        while (entry && totalHidScripts < MAX_HID_SCRIPTS) {
            if (!entry.isDirectory()) {
                String name = entry.name();
                if (name.endsWith(".txt") || name.endsWith(".TXT")) {
                    // Prepend folder if not root
                    String fullPath = (String(folders[f]) == "/") ? (String("/") + name) : (String(folders[f]) + "/" + name);

                    // Check if duplicate
                    bool dup = false;
                    for (int i = 0; i < totalHidScripts; i++) {
                        if (hidScriptNames[i] == fullPath) { dup = true; break; }
                    }
                    if (!dup) {
                        hidScriptNames[totalHidScripts++] = fullPath;
                    }
                }
            }
            entry = dir.openNextFile();
        }
        dir.close();
    }
}

static void renderHidScriptList(int selected) {
    display.clearDisplay();
    display.setTextColor(SH110X_WHITE);
    display.setTextSize(1);

    // Header
    display.fillRect(0, 0, 128, 11, SH110X_WHITE);
    display.setTextColor(SH110X_BLACK);
    display.setCursor(2, 2);
    display.print("HID Scripts");

    // Connection badge
    if (bleIsConnected) {
        display.fillRect(70, 1, 56, 9, SH110X_BLACK);
        display.setTextColor(SH110X_WHITE);
        display.setCursor(72, 2);
        display.print("CONNECTED");
    } else {
        display.setTextColor(SH110X_BLACK);
        display.setCursor(78, 2);
        display.print("PAIRING");
    }

    if (totalHidScripts == 0) {
        display.setTextColor(SH110X_WHITE);
        display.setCursor(8, 20);
        display.print("No .txt scripts found");
        display.setCursor(8, 32);
        display.print("Insert SD with scripts");
        display.setCursor(8, 44);
        display.print("in /scripts folder");
        display.setCursor(8, 54);
        display.print("< Back to return");
        display.display();
        return;
    }

    int visibleRows = 4;
    int startIdx = 0;
    if (selected >= visibleRows) {
        startIdx = selected - visibleRows + 1;
    }

    int y = 14;
    for (int i = startIdx; i < startIdx + visibleRows && i < totalHidScripts; i++) {
        bool isSel = (i == selected);
        if (isSel) {
            display.fillRect(0, y - 1, 128, 12, SH110X_WHITE);
            display.setTextColor(SH110X_BLACK);
        } else {
            display.setTextColor(SH110X_WHITE);
        }

        String displayName = hidScriptNames[i];
        int lastSlash = displayName.lastIndexOf('/');
        if (lastSlash >= 0) displayName = displayName.substring(lastSlash + 1);

        if (displayName.length() > 18) {
            displayName = displayName.substring(0, 17) + "..";
        }
        display.setCursor(4, y + 1);
        display.print(displayName);

        y += 12;
    }

    display.display();
}

static bool asciiToHIDKey(char c, uint8_t &key, uint8_t &mod) {
    mod = 0x00;
    if (c >= 'a' && c <= 'z') { key = 0x04 + (c - 'a'); return true; }
    if (c >= 'A' && c <= 'Z') { key = 0x04 + (c - 'A'); mod = 0x02; return true; } // Shift
    if (c >= '1' && c <= '9') { key = 0x1E + (c - '1'); return true; }
    if (c == '0') { key = 0x27; return true; }
    if (c == ' ') { key = 0x2C; return true; }
    if (c == '\n' || c == '\r') { key = 0x28; return true; }
    if (c == '\t') { key = 0x2B; return true; }
    if (c == '!') { key = 0x1E; mod = 0x02; return true; }
    if (c == '@') { key = 0x1F; mod = 0x02; return true; }
    if (c == '#') { key = 0x20; mod = 0x02; return true; }
    if (c == '$') { key = 0x21; mod = 0x02; return true; }
    if (c == '%') { key = 0x22; mod = 0x02; return true; }
    if (c == '^') { key = 0x23; mod = 0x02; return true; }
    if (c == '&') { key = 0x24; mod = 0x02; return true; }
    if (c == '*') { key = 0x25; mod = 0x02; return true; }
    if (c == '(') { key = 0x26; mod = 0x02; return true; }
    if (c == ')') { key = 0x27; mod = 0x02; return true; }
    if (c == '-') { key = 0x2D; return true; }
    if (c == '_') { key = 0x2D; mod = 0x02; return true; }
    if (c == '=') { key = 0x2E; return true; }
    if (c == '+') { key = 0x2E; mod = 0x02; return true; }
    if (c == '[') { key = 0x2F; return true; }
    if (c == '{') { key = 0x2F; mod = 0x02; return true; }
    if (c == ']') { key = 0x30; return true; }
    if (c == '}') { key = 0x30; mod = 0x02; return true; }
    if (c == '\\') { key = 0x31; return true; }
    if (c == '|') { key = 0x31; mod = 0x02; return true; }
    if (c == ';') { key = 0x33; return true; }
    if (c == ':') { key = 0x33; mod = 0x02; return true; }
    if (c == '\'') { key = 0x34; return true; }
    if (c == '\"') { key = 0x34; mod = 0x02; return true; }
    if (c == '`') { key = 0x35; return true; }
    if (c == '~') { key = 0x35; mod = 0x02; return true; }
    if (c == ',') { key = 0x36; return true; }
    if (c == '<') { key = 0x36; mod = 0x02; return true; }
    if (c == '.') { key = 0x37; return true; }
    if (c == '>') { key = 0x37; mod = 0x02; return true; }
    if (c == '/') { key = 0x38; return true; }
    if (c == '?') { key = 0x38; mod = 0x02; return true; }
    return false;
}

static void sendDuckyString(const String& s) {
    for (size_t i = 0; i < s.length(); i++) {
        uint8_t key = 0, mod = 0;
        if (asciiToHIDKey(s[i], key, mod)) {
            sendHidKeyboardReport(mod, key);
            releaseHidKeyboard();
            delay(12);
        }
    }
}

static void renderDuckyProgressScreen(const String& filename, int progressPct, const String& lastCmd) {
    display.clearDisplay();
    display.setTextColor(SH110X_WHITE);
    display.setTextSize(1);

    // Header
    display.fillRect(0, 0, 128, 11, SH110X_WHITE);
    display.setTextColor(SH110X_BLACK);
    display.setCursor(2, 2);
    display.print("Executing Script");

    display.setTextColor(SH110X_WHITE);
    display.setCursor(2, 14);
    String fname = filename;
    int slash = fname.lastIndexOf('/');
    if (slash >= 0) fname = fname.substring(slash + 1);
    if (fname.length() > 18) fname = fname.substring(0, 17) + "..";
    display.print(fname);

    // Progress bar
    display.drawRect(8, 27, 112, 11, SH110X_WHITE);
    int fillW = map(progressPct, 0, 100, 0, 108);
    if (fillW > 0) {
        display.fillRect(10, 29, fillW, 7, SH110X_WHITE);
    }
    display.setCursor(54, 40);
    display.print(progressPct);
    display.print("%");

    display.setCursor(2, 52);
    String cmd = lastCmd;
    if (cmd.length() > 20) cmd = cmd.substring(0, 19) + "..";
    display.print(cmd);

    display.display();
}

static void executeDuckyScript(const String& filepath) {
    if (!bleIsConnected) {
        display.clearDisplay();
        display.setTextColor(SH110X_WHITE);
        display.setTextSize(1);
        display.setCursor(8, 16);
        display.print("Not Connected!");
        display.setCursor(8, 30);
        display.print("Connect victim phone");
        display.setCursor(8, 42);
        display.print("or PC to PROTOWARE");
        display.display();
        delay(2000);
        return;
    }

    File f = SD.open(filepath, FILE_READ);
    if (!f) {
        display.clearDisplay();
        display.setTextColor(SH110X_WHITE);
        display.setTextSize(1);
        display.setCursor(8, 24);
        display.print("Failed to open file!");
        display.display();
        delay(1500);
        return;
    }

    size_t fileSize = f.size();
    if (fileSize == 0) fileSize = 1;

    renderDuckyProgressScreen(filepath, 0, "Starting...");
    delay(400);

    while (f.available()) {
        buttonBack.tick();
        if (buttonBack.isClick()) { // Abort requested
            releaseHidKeyboard();
            break;
        }

        String line = f.readStringUntil('\n');
        line.trim();
        if (line.length() == 0 || line.startsWith("REM")) continue;

        int pct = (f.position() * 100) / fileSize;
        renderDuckyProgressScreen(filepath, pct, line);

        if (line.startsWith("STRING ")) {
            sendDuckyString(line.substring(7));
        } else if (line.startsWith("DELAY ")) {
            int ms = line.substring(6).toInt();
            unsigned long startWait = millis();
            while (millis() - startWait < (unsigned long)ms) {
                buttonBack.tick();
                if (buttonBack.isClick()) break;
                delay(10);
            }
        } else if (line.equalsIgnoreCase("ENTER")) {
            sendHidKeyboardReport(0, 0x28);
            releaseHidKeyboard();
        } else if (line.startsWith("GUI ") || line.startsWith("WINDOWS ")) {
            char keyChar = line.charAt(line.length() - 1);
            uint8_t key = 0, mod = 0;
            if (asciiToHIDKey(keyChar, key, mod)) {
                sendHidKeyboardReport(0x08, key);
                releaseHidKeyboard();
            }
        } else if (line.equalsIgnoreCase("TAB")) {
            sendHidKeyboardReport(0, 0x2B);
            releaseHidKeyboard();
        } else if (line.equalsIgnoreCase("ESCAPE") || line.equalsIgnoreCase("ESC")) {
            sendHidKeyboardReport(0, 0x29);
            releaseHidKeyboard();
        } else if (line.equalsIgnoreCase("SPACE")) {
            sendHidKeyboardReport(0, 0x2C);
            releaseHidKeyboard();
        } else if (line.equalsIgnoreCase("BACKSPACE")) {
            sendHidKeyboardReport(0, 0x2A);
            releaseHidKeyboard();
        } else if (line.equalsIgnoreCase("DOWN") || line.equalsIgnoreCase("DOWNARROW")) {
            sendHidKeyboardReport(0, 0x51);
            releaseHidKeyboard();
        } else if (line.equalsIgnoreCase("UP") || line.equalsIgnoreCase("UPARROW")) {
            sendHidKeyboardReport(0, 0x52);
            releaseHidKeyboard();
        } else if (line.equalsIgnoreCase("LEFT") || line.equalsIgnoreCase("LEFTARROW")) {
            sendHidKeyboardReport(0, 0x50);
            releaseHidKeyboard();
        } else if (line.equalsIgnoreCase("RIGHT") || line.equalsIgnoreCase("RIGHTARROW")) {
            sendHidKeyboardReport(0, 0x4F);
            releaseHidKeyboard();
        }

        delay(30);
    }

    f.close();
    releaseHidKeyboard();

    renderDuckyProgressScreen(filepath, 100, "Done!");
    delay(1200);
}

// =============================================================================
// SUBMODULE ROUTERS & INPUT HANDLER
// =============================================================================
void initBluetoothModule() {
    currentBleState = BLE_STATE_MENU;
    bleInited = false;
    bleIsConnected = false;
}

void enterBluetoothSubmenu() {
    currentBleState = BLE_STATE_MENU;
    bluetoothMenuIndex = 0;
    displayBluetoothMenu(display, bluetoothMenuIndex);
}

void handleBluetoothMenu() {
    static MenuButtonState upState;
    static MenuButtonState downState;
    static MenuButtonState leftState;
    static MenuButtonState rightState;
    static MenuButtonState centerState;

    unsigned long repeatDelay = getMenuSubmenuRepeatDelay(submenu == 1);
    bool upPress = isMenuButtonPress(BUTTON_PIN_UP, upState, repeatDelay);
    bool downPress = isMenuButtonPress(BUTTON_PIN_DOWN, downState, repeatDelay);
    bool clickLeft = buttonBack.isClick();
    bool clickRight = buttonOK.isClick();
    bool clickCenter = buttonCenter.isClick();
    bool okClick = clickRight || clickCenter;
    bool backClick = clickLeft;

    if (upPress || downPress || okClick || backClick) {
        resetActivityTimer();
    }

    switch (currentBleState) {
        // ---------------------------------------------------------------------
        // 0. SUBMENU ROOT: BLE Scan, BLE Mouse, BLE Keyboard, HID
        // ---------------------------------------------------------------------
        case BLE_STATE_MENU: {
            if (upPress) {
                byte prev = bluetoothMenuIndex;
                bluetoothMenuIndex = (bluetoothMenuIndex + BLUETOOTH_MENU_ITEM_COUNT - 1) % BLUETOOTH_MENU_ITEM_COUNT;
                displayBluetoothMenu(display, bluetoothMenuIndex, prev);
            } else if (downPress) {
                byte prev = bluetoothMenuIndex;
                bluetoothMenuIndex = (bluetoothMenuIndex + 1) % BLUETOOTH_MENU_ITEM_COUNT;
                displayBluetoothMenu(display, bluetoothMenuIndex, prev);
            } else if (backClick) {
                stopBLEService();
                returnToMainMenu();
            } else if (okClick) {
                if (bluetoothMenuIndex == 0) { // BLE Scan
                    performBleScan();
                    currentBleState = BLE_STATE_SCAN_LIST;
                    renderBleScanList(bleScanSelectedIdx);
                } else if (bluetoothMenuIndex == 1) { // BLE Mouse
                    startBleHidService("BLE Mouse", 0x03C2);
                    currentBleState = BLE_STATE_MOUSE;
                    renderBleMouseScreen();
                } else if (bluetoothMenuIndex == 2) { // BLE Keyboard
                    startBleHidService("BLE Keyboard", 0x03C1);
                    currentBleState = BLE_STATE_KEYBOARD;
                    kbPage = 0;
                    kbRow = 0;
                    kbCol = 0;
                    kbLastSent = "None";
                    renderBleKeyboardScreen();
                } else if (bluetoothMenuIndex == 3) { // HID
                    startBleHidService("PROTOWARE HID", 0x03C1);
                    scanSdForScripts();
                    currentBleState = BLE_STATE_HID_LIST;
                    renderHidScriptList(hidSelectedIdx);
                }
            }
            break;
        }

        // ---------------------------------------------------------------------
        // 1. BLE SCAN: List & Details
        // ---------------------------------------------------------------------
        case BLE_STATE_SCAN_LIST: {
            if (totalBleScanned > 0) {
                if (upPress) {
                    bleScanSelectedIdx = (bleScanSelectedIdx + totalBleScanned - 1) % totalBleScanned;
                    renderBleScanList(bleScanSelectedIdx);
                } else if (downPress) {
                    bleScanSelectedIdx = (bleScanSelectedIdx + 1) % totalBleScanned;
                    renderBleScanList(bleScanSelectedIdx);
                } else if (okClick) {
                    currentBleState = BLE_STATE_SCAN_DETAILS;
                    renderBleDetails(bleScanSelectedIdx);
                }
            }
            if (backClick) {
                currentBleState = BLE_STATE_MENU;
                displayBluetoothMenu(display, bluetoothMenuIndex);
            }
            break;
        }

        case BLE_STATE_SCAN_DETAILS: {
            if (backClick || okClick) {
                currentBleState = BLE_STATE_SCAN_LIST;
                renderBleScanList(bleScanSelectedIdx);
            }
            break;
        }

        // ---------------------------------------------------------------------
        // 2. BLE MOUSE
        // ---------------------------------------------------------------------
        case BLE_STATE_MOUSE: {
            handleBleMouseInput();
            if (buttonBack.isClick()) {
                stopBLEService();
                currentBleState = BLE_STATE_MENU;
                displayBluetoothMenu(display, bluetoothMenuIndex);
            }
            break;
        }

        // ---------------------------------------------------------------------
        // 3. BLE KEYBOARD
        // ---------------------------------------------------------------------
        case BLE_STATE_KEYBOARD: {
            static MenuButtonState kbUpState;
            static MenuButtonState kbDownState;
            static MenuButtonState kbLeftState;
            static MenuButtonState kbRightState;
            static MenuButtonState kbCenterState;

            bool kbUp = isMenuButtonPress(BUTTON_PIN_UP, kbUpState, 160);
            bool kbDown = isMenuButtonPress(BUTTON_PIN_DOWN, kbDownState, 160);
            bool kbLeft = isMenuButtonPress(BUTTON_PIN_LEFT, kbLeftState, 160);
            bool kbRight = isMenuButtonPress(BUTTON_PIN_RIGHT, kbRightState, 160);
            bool kbSelect = isMenuButtonPress(BUTTON_PIN_CENTER, kbCenterState, 0); // Single-shot debounced

            bool moved = false;
            int maxR = (kbPage == 0) ? 5 : 4;

            if (kbUp) {
                kbRow = (kbRow + maxR - 1) % maxR;
                moved = true;
            } else if (kbDown) {
                kbRow = (kbRow + 1) % maxR;
                moved = true;
            } else if (kbLeft) {
                int maxC = (kbPage == 0) ? ((kbRow == 4) ? 5 : 10) : ((kbRow == 3) ? 2 : 5);
                kbCol = (kbCol + maxC - 1) % maxC;
                moved = true;
            } else if (kbRight) {
                int maxC = (kbPage == 0) ? ((kbRow == 4) ? 5 : 10) : ((kbRow == 3) ? 2 : 5);
                kbCol = (kbCol + 1) % maxC;
                moved = true;
            }

            // Adjust column boundary if switching rows
            int maxColsOnRow = (kbPage == 0) ? ((kbRow == 4) ? 5 : 10) : ((kbRow == 3) ? 2 : 5);
            if (kbCol >= maxColsOnRow) kbCol = maxColsOnRow - 1;

            if (kbSelect) {
                handleBleKeyboardClick();
                moved = true;
            }

            if (currentBleState == BLE_STATE_KEYBOARD && moved) {
                renderBleKeyboardScreen();
            }

            // Periodic connection refresh
            static unsigned long lastKbRefresh = 0;
            if (millis() - lastKbRefresh >= 1000) {
                lastKbRefresh = millis();
                renderBleKeyboardScreen();
            }

            if (buttonBack.isClick()) {
                stopBLEService();
                currentBleState = BLE_STATE_MENU;
                displayBluetoothMenu(display, bluetoothMenuIndex);
            }
            break;
        }

        // ---------------------------------------------------------------------
        // 4. HID SCRIPT LIST & RUNNER
        // ---------------------------------------------------------------------
        case BLE_STATE_HID_LIST: {
            if (totalHidScripts > 0) {
                if (upPress) {
                    hidSelectedIdx = (hidSelectedIdx + totalHidScripts - 1) % totalHidScripts;
                    renderHidScriptList(hidSelectedIdx);
                } else if (downPress) {
                    hidSelectedIdx = (hidSelectedIdx + 1) % totalHidScripts;
                    renderHidScriptList(hidSelectedIdx);
                } else if (okClick) {
                    currentBleState = BLE_STATE_HID_RUNNING;
                    executeDuckyScript(hidScriptNames[hidSelectedIdx]);
                    currentBleState = BLE_STATE_HID_LIST;
                    renderHidScriptList(hidSelectedIdx);
                }
            }
            if (backClick) {
                stopBLEService();
                currentBleState = BLE_STATE_MENU;
                displayBluetoothMenu(display, bluetoothMenuIndex);
            }
            break;
        }

        case BLE_STATE_HID_RUNNING: {
            // Handled synchronously in executeDuckyScript
            break;
        }
    }
}
