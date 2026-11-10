#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <GyverButton.h>

#include "CONFIG.h"
#include "display.h"
#include "interface/interface.h"

#include "menu/wifi.h"
#include "menu/bluetooth.h"
#include "menu/subghz.h"
#include "menu/infrared.h"
#include "menu/gpio.h"
#include "menu/games.h"
#include "menu/settings.h"
#include "menu/firmware.h"
#include "settings.h"
#include "wifi_module.h"
#include "bluetooth_module.h"
#include "firmware_module.h"

// =============================================================================
// GLOBAL INTERFACE DEFINITIONS
// =============================================================================
byte menu = 1;      // 0: Pages, 1: List (Animated), 2: Wii, 3: DSi
byte submenu = 1;   // 0: Roll, 1: List
const char* menuItems[] = {"WiFi", "Bluetooth", "Transceiver", "Infrared", "GPIO", "Games", "Settings", "Firmware"};

byte currentMenu = 0;
bool inMenu = true;

// Submenu indices
byte wifiMenuIndex = 0;
byte bluetoothMenuIndex = 0;
byte subghzMenuIndex = 0;
byte irMenuIndex = 0;
byte gpioMenuIndex = 0;
byte gamesMenuIndex = 0;
byte settingsMenuIndex = 0;
byte firmwareMenuIndex = 0;

// Submenu item counts
static const byte submenuCounts[MENU_ITEM_COUNT] = {
    WIFI_MENU_ITEM_COUNT,       // 5
    BLUETOOTH_MENU_ITEM_COUNT,  // 3
    SUBGHZ_MENU_ITEM_COUNT,     // 6 (Transceiver)
    4,                          // Infrared (4 items)
    GPIO_MENU_ITEM_COUNT,       // 3
    GAMES_MENU_ITEM_COUNT,      // 5
    SETTINGS_MENU_ITEM_COUNT,   // 5
    FIRMWARE_MENU_ITEM_COUNT    // 5
};

// =============================================================================
// HARDWARE INSTANCES
// =============================================================================
#if DISPLAY_TYPE == DISPLAY_SH1106
DisplayType display = DisplayType(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
#elif DISPLAY_TYPE == DISPLAY_SSD1306
DisplayType display = DisplayType(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
#endif

GButton buttonUp(BUTTON_PIN_UP, HIGH_PULL, NORM_OPEN);
GButton buttonDown(BUTTON_PIN_DOWN, HIGH_PULL, NORM_OPEN);
GButton buttonOK(BUTTON_PIN_RIGHT, HIGH_PULL, NORM_OPEN);      // Select / OK
GButton buttonBack(BUTTON_PIN_LEFT, HIGH_PULL, NORM_OPEN);     // Back
GButton buttonCenter(BUTTON_PIN_CENTER, HIGH_PULL, NORM_OPEN); // Center / Exit

// SD Card SPI bus
SPIClass sdSPI(VSPI);
bool sdCardReady = false;

// =============================================================================
// FUNCTION DECLARATIONS
// =============================================================================
void returnToMainMenu();
bool ensureSDReadyInteractive(bool allowSkip);
void displayCurrentSubmenu(int previousIndex = -1);
void handleSubmenuNavigation();

// =============================================================================
// INPUT HELPERS
// =============================================================================
static bool isMainMenuButtonPress(uint8_t pin, MenuButtonState &state) {
    const bool pressed = (digitalRead(pin) == LOW);
    const unsigned long now = millis();

    if (!pressed) {
        if (state.wasPressed) {
            if (now - state.lastPressTime >= BUTTON_DEBOUNCE_MS) {
                state.wasPressed = false;
                state.lastReleaseTime = now;
                state.nextRepeatAt = 0;
            }
        }
        return false;
    }

    if (!state.wasPressed) {
        if (now - state.lastReleaseTime < BUTTON_DEBOUNCE_MS) {
            return false;
        }
        state.wasPressed = true;
        state.lastPressTime = now;
        state.nextRepeatAt = now + getListMenuInitialRepeatDelay();
        return true;
    }

    if (now >= state.nextRepeatAt) {
        state.nextRepeatAt = now + getListMenuRepeatDelay();
        return true;
    }

    return false;
}

void returnToMainMenu() {
    inMenu = true;
    resetMainMenuAnimation(currentMenu);
    OLED_printMenu(display, currentMenu);
}

bool ensureSDReadyInteractive(bool allowSkip) {
    (void)allowSkip;
    return sdCardReady;
}

// =============================================================================
// SUBMENU DISPLAY ROUTER
// =============================================================================
void displayCurrentSubmenu(int previousIndex) {
    switch (currentMenu) {
        case 0: enterWiFiSubmenu(); break;
        case 1: enterBluetoothSubmenu(); break;
        case 2: OLED_printSubGHzMenu(display, subghzMenuIndex, previousIndex); break;
        case 3: displayIRMenu(display, irMenuIndex, previousIndex); break;
        case 4: displayGPIOMenu(display, gpioMenuIndex, previousIndex); break;
        case 5: displayGamesMenu(display, gamesMenuIndex, previousIndex); break;
        case 6: enterSettingsSubmenu(); break;
        case 7: enterFirmwareSubmenu(); break;
    }
}

static byte* getCurrentSubmenuIndexPtr() {
    switch (currentMenu) {
        case 0: return &wifiMenuIndex;
        case 1: return &bluetoothMenuIndex;
        case 2: return &subghzMenuIndex;
        case 3: return &irMenuIndex;
        case 4: return &gpioMenuIndex;
        case 5: return &gamesMenuIndex;
        case 6: return &settingsMenuIndex;
        case 7: return &firmwareMenuIndex;
        default: return &wifiMenuIndex;
    }
}

// =============================================================================
// SUBMENU HANDLER
// =============================================================================
void handleSubmenuNavigation() {
    if (currentMenu == 0) {
        handleWiFiMenu();
        return;
    }
    if (currentMenu == 1) {
        handleBluetoothMenu();
        return;
    }
    if (currentMenu == 6) {
        handleSettingsMenu();
        return;
    }
    if (currentMenu == 7) {
        handleFirmwareMenu();
        return;
    }

    byte* idxPtr = getCurrentSubmenuIndexPtr();
    byte totalItems = submenuCounts[currentMenu];

    static MenuButtonState subUpHeld;
    static MenuButtonState subDownHeld;

    bool upPress = isMenuButtonPress(BUTTON_PIN_UP, subUpHeld, getMenuSubmenuRepeatDelay(submenu == 1));
    bool downPress = isMenuButtonPress(BUTTON_PIN_DOWN, subDownHeld, getMenuSubmenuRepeatDelay(submenu == 1));
    bool okClick = buttonOK.isClick() || buttonCenter.isClick();
    bool backClick = buttonBack.isClick();

    if (upPress || downPress || okClick || backClick) {
        resetActivityTimer();
    }

    if (upPress) {
        byte prev = *idxPtr;
        *idxPtr = (*idxPtr - 1 + totalItems) % totalItems;
        displayCurrentSubmenu(prev);
    }
    else if (downPress) {
        byte prev = *idxPtr;
        *idxPtr = (*idxPtr + 1) % totalItems;
        displayCurrentSubmenu(prev);
    }
    else if (backClick) {
        returnToMainMenu();
    }
    else if (okClick) {
        // Placeholder notice screen for sub-module
        display.clearDisplay();
        display.setTextColor(SH110X_WHITE);
        display.setTextSize(1);
        display.setCursor(4, 10);
        display.printf("[%s]", menuItems[currentMenu]);
        display.setCursor(4, 26);
        display.print("Selected Item: ");
        display.print(*idxPtr + 1);
        display.setCursor(4, 42);
        display.print("Ready to implement");
        display.setCursor(4, 54);
        display.print("< Back  |  OK Exit");
        display.display();

        // Wait for back button
        while (true) {
            buttonBack.tick();
            buttonCenter.tick();
            buttonOK.tick();
            if (buttonBack.isClick() || buttonCenter.isClick() || buttonOK.isClick()) {
                resetActivityTimer();
                break;
            }
            delay(10);
        }
        displayCurrentSubmenu();
    }
}

// =============================================================================
// SETUP
// =============================================================================
void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.println("\n[SYSTEM] Booting PROTOWARE Multi-Tool with ESPHACK Interface...");

    // 1. Initialize Display
    Wire.begin(OLED_SDA, OLED_SCL);
#if DISPLAY_TYPE == DISPLAY_SH1106
    display.begin(OLED_ADR, true);
#elif DISPLAY_TYPE == DISPLAY_SSD1306
    display.begin(SSD1306_SWITCHCAPVCC, OLED_ADR);
#endif
    display.clearDisplay();
    display.setTextWrap(false);
    display.display();

    // 2. Load Saved Preferences & Settings
    initSettings();
    initWiFiModule();
    initBluetoothModule();
    initFirmwareModule();

    // 3. Button configuration
    buttonUp.setDebounce(50);
    buttonUp.setTimeout(500);
    buttonUp.setClickTimeout(BUTTON_RELEASE_CLICK_MS);
    buttonUp.setStepTimeout(200);

    buttonDown.setDebounce(50);
    buttonDown.setTimeout(500);
    buttonDown.setClickTimeout(BUTTON_RELEASE_CLICK_MS);
    buttonDown.setStepTimeout(200);

    buttonOK.setDebounce(50);
    buttonOK.setTimeout(500);
    buttonOK.setClickTimeout(BUTTON_RELEASE_CLICK_MS);
    buttonOK.setStepTimeout(200);

    buttonBack.setDebounce(50);
    buttonBack.setTimeout(500);
    buttonBack.setClickTimeout(BUTTON_RELEASE_CLICK_MS);
    buttonBack.setStepTimeout(200);

    buttonCenter.setDebounce(50);
    buttonCenter.setTimeout(500);
    buttonCenter.setClickTimeout(BUTTON_RELEASE_CLICK_MS);
    buttonCenter.setStepTimeout(200);

    // 3. Initialize SD Card SPI pins
    pinMode(SD_CS, OUTPUT);
    digitalWrite(SD_CS, HIGH);
    sdSPI.begin(SPI_SCK_PIN, SPI_MISO_PIN, SPI_MOSI_PIN, SD_CS);
    if (SD.begin(SD_CS, sdSPI)) {
        sdCardReady = true;
        Serial.println("[STORAGE] SD Card Mounted.");
    } else {
        sdCardReady = false;
        Serial.println("[STORAGE] SD Card not detected.");
    }

    // 4. Show Boot Logo
    OLED_printLogo(display, true);
    delay(600);

    // 5. Initial Menu Display
    resetMainMenuAnimation(currentMenu);
    OLED_printMenu(display, currentMenu);
    Serial.println("[SYSTEM] Ready! Main Menu Running.");
}

// =============================================================================
// MAIN LOOP
// =============================================================================
void loop() {
    static MenuButtonState upHeld;
    static MenuButtonState downHeld;

    buttonUp.tick();
    buttonDown.tick();
    buttonOK.tick();
    buttonBack.tick();
    buttonCenter.tick();

    checkStandbyTimeout();

    if (inMenu) {
        bool upPress = isMainMenuButtonPress(BUTTON_PIN_UP, upHeld);
        bool downPress = isMainMenuButtonPress(BUTTON_PIN_DOWN, downHeld);
        bool leftPress = buttonBack.isClick();
        bool okClick = buttonOK.isClick() || buttonCenter.isClick();
        bool anyClick = upPress || downPress || leftPress || okClick;

        if (anyClick) {
            resetActivityTimer();
        }

        static unsigned long listRefreshMs = 0;
        if ((menu == 1 || menu == 2 || menu == 3) && !anyClick && (millis() - listRefreshMs >= 333)) {
            listRefreshMs = millis();
            OLED_printMenu(display, currentMenu);
        }

        if (upPress || leftPress) {
            byte previousIndex = currentMenu;
            currentMenu = (currentMenu - 1 + MENU_ITEM_COUNT) % MENU_ITEM_COUNT;
            OLED_printMenuAnimated(display, currentMenu, previousIndex);
        }
        else if (downPress) {
            byte previousIndex = currentMenu;
            currentMenu = (currentMenu + 1) % MENU_ITEM_COUNT;
            OLED_printMenuAnimated(display, currentMenu, previousIndex);
        }
        else if (okClick) {
            inMenu = false;
            byte* idxPtr = getCurrentSubmenuIndexPtr();
            *idxPtr = 0;
            displayCurrentSubmenu();
        }
    } else {
        handleSubmenuNavigation();
    }
}
