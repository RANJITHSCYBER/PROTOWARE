#include "settings.h"
#include <Preferences.h>
#include <GyverButton.h>

#include "CONFIG.h"
#include "pindefs.h"
#include "interface/interface.h"
#include "menu/settings.h"

extern DisplayType display;
extern GButton buttonUp;
extern GButton buttonDown;
extern GButton buttonOK;
extern GButton buttonBack;
extern GButton buttonCenter;

extern byte menu;
extern byte submenu;
extern bool inMenu;
extern byte currentMenu;
extern byte settingsMenuIndex;
extern void displayCurrentSubmenu(int previousIndex = -1);

enum SettingsState : byte {
    SETTING_STATE_ROOT = 0,
    SETTING_STATE_INTERFACE,
    SETTING_STATE_MENU_STYLE,
    SETTING_STATE_SUBMENU_STYLE,
    SETTING_STATE_COLOR,
    SETTING_STATE_STANDBY,
    SETTING_STATE_ABOUT
};

static const char* const interfaceMenuItems[] = {
    "Menu", "Submenu", "Color", "Standby", "Back"
};
static const byte INTERFACE_MENU_COUNT = 5;

static const char* const menuStyleItems[] = {
    "Pages", "List", "Wii", "DSi"
};
static const byte MENU_STYLE_COUNT = 4;

static const char* const submenuStyleItems[] = {
    "Roll", "List"
};
static const byte SUBMENU_STYLE_COUNT = 2;

static const char* const colorItems[] = {
    "Normal", "Inverted"
};
static const byte COLOR_ITEM_COUNT = 2;

static const unsigned long standbyTimeoutOptionsMs[] = {0, 15000, 30000, 60000, 120000, 300000};
static const char* const standbyTimeoutLabels[] = {
    "Off", "15s", "30s", "1m", "2m", "5m"
};
static const byte STANDBY_ITEM_COUNT = 6;

static SettingsState currentState = SETTING_STATE_ROOT;
static byte interfaceIndex = 0;
static byte styleIndex = 0;
static byte submenuStyleIndex = 0;
static byte colorIndex = 0;
static byte standbyIndex = 0;

byte colorSelectionIndex = 0;
byte standbyTimeoutIndex = 0;
static unsigned long standbyTimeoutMs = 0;
static unsigned long lastActivityMs = 0;
static bool inStandby = false;

static void showFeedback(const char* title, const char* value) {
    display.clearDisplay();
    display.drawRoundRect(8, 12, 112, 40, 4, SH110X_WHITE);
    display.setTextColor(SH110X_WHITE);
    display.setTextSize(1);

    int16_t x1, y1;
    uint16_t w, h;

    display.getTextBounds(title, 0, 0, &x1, &y1, &w, &h);
    display.setCursor((128 - w) / 2, 20);
    display.print(title);

    display.getTextBounds(value, 0, 0, &x1, &y1, &w, &h);
    display.setCursor((128 - w) / 2, 34);
    display.print(value);

    display.display();
    delay(550);
}

void applyCurrentColorScheme() {
    display.invertDisplay(colorSelectionIndex != 0);
}

void initSettings() {
    Preferences prefs;
    prefs.begin("protoware", false);
    menu = prefs.getUChar("menu", 1);
    if (menu > 3) menu = 1;
    submenu = prefs.getUChar("submenu", 1);
    if (submenu > 1) submenu = 1;
    colorSelectionIndex = prefs.getUChar("color", 0);
    if (colorSelectionIndex > 1) colorSelectionIndex = 0;
    standbyTimeoutIndex = prefs.getUChar("standby", 0);
    if (standbyTimeoutIndex >= STANDBY_ITEM_COUNT) standbyTimeoutIndex = 0;
    prefs.end();

    standbyTimeoutMs = standbyTimeoutOptionsMs[standbyTimeoutIndex];
    applyCurrentColorScheme();
    lastActivityMs = millis();
}

void saveAppearanceConfig() {
    Preferences prefs;
    prefs.begin("protoware", false);
    prefs.putUChar("menu", menu);
    prefs.putUChar("submenu", submenu);
    prefs.putUChar("color", colorSelectionIndex);
    prefs.putUChar("standby", standbyTimeoutIndex);
    prefs.end();
}

void resetActivityTimer() {
    lastActivityMs = millis();
    if (inStandby) {
        inStandby = false;
        applyCurrentColorScheme();
        if (inMenu) {
            resetMainMenuAnimation(currentMenu);
            OLED_printMenu(display, currentMenu);
        } else {
            if (currentMenu == 6) {
                enterSettingsSubmenu();
            } else {
                displayCurrentSubmenu();
            }
        }
    }
}

void checkStandbyTimeout() {
    if (standbyTimeoutMs > 0 && !inStandby && (millis() - lastActivityMs >= standbyTimeoutMs)) {
        inStandby = true;
        display.clearDisplay();
        display.display();
    }
}

void enterSettingsSubmenu() {
    currentState = SETTING_STATE_ROOT;
    settingsMenuIndex = 0;
    displaySubmenu(display, settingsMenuItems, SETTINGS_MENU_ITEM_COUNT, settingsMenuIndex);
}

void handleSettingsMenu() {
    static MenuButtonState upState;
    static MenuButtonState downState;

    unsigned long repeatDelay = getMenuSubmenuRepeatDelay(submenu == 1);
    bool upPress = isMenuButtonPress(BUTTON_PIN_UP, upState, repeatDelay);
    bool downPress = isMenuButtonPress(BUTTON_PIN_DOWN, downState, repeatDelay);
    bool okClick = buttonOK.isClick() || buttonCenter.isClick();
    bool backClick = buttonBack.isClick();

    if (upPress || downPress || okClick || backClick) {
        resetActivityTimer();
    }

    switch (currentState) {
        case SETTING_STATE_ROOT: {
            if (upPress) {
                byte prev = settingsMenuIndex;
                settingsMenuIndex = (settingsMenuIndex + SETTINGS_MENU_ITEM_COUNT - 1) % SETTINGS_MENU_ITEM_COUNT;
                displaySubmenu(display, settingsMenuItems, SETTINGS_MENU_ITEM_COUNT, settingsMenuIndex, prev);
            } else if (downPress) {
                byte prev = settingsMenuIndex;
                settingsMenuIndex = (settingsMenuIndex + 1) % SETTINGS_MENU_ITEM_COUNT;
                displaySubmenu(display, settingsMenuItems, SETTINGS_MENU_ITEM_COUNT, settingsMenuIndex, prev);
            } else if (backClick) {
                returnToMainMenu();
            } else if (okClick) {
                if (settingsMenuIndex == 0) { // Interface
                    currentState = SETTING_STATE_INTERFACE;
                    interfaceIndex = 0;
                    displaySubmenu(display, interfaceMenuItems, INTERFACE_MENU_COUNT, interfaceIndex);
                } else if (settingsMenuIndex == 1) { // Restart
                    display.clearDisplay();
                    display.setTextColor(SH110X_WHITE);
                    display.setTextSize(1);
                    display.setCursor(30, 28);
                    display.print("Rebooting...");
                    display.display();
                    delay(600);
                    ESP.restart();
                } else if (settingsMenuIndex == 2) { // Reset
                    menu = 1;
                    submenu = 1;
                    colorSelectionIndex = 0;
                    standbyTimeoutIndex = 0;
                    saveAppearanceConfig();
                    display.clearDisplay();
                    display.setTextColor(SH110X_WHITE);
                    display.setTextSize(1);
                    display.setCursor(20, 28);
                    display.print("Defaults Reset!");
                    display.display();
                    delay(800);
                    ESP.restart();
                } else if (settingsMenuIndex == 3) { // Update
                    display.clearDisplay();
                    display.setTextColor(SH110X_WHITE);
                    display.setTextSize(1);
                    display.setCursor(16, 20);
                    display.print("Firmware Update");
                    display.setCursor(16, 36);
                    display.print("No SD file found");
                    display.display();
                    delay(1200);
                    displaySubmenu(display, settingsMenuItems, SETTINGS_MENU_ITEM_COUNT, settingsMenuIndex);
                } else if (settingsMenuIndex == 4) { // About
                    currentState = SETTING_STATE_ABOUT;
                    display.clearDisplay();
                    display.setTextColor(SH110X_WHITE);
                    display.setTextSize(2);
                    display.setCursor(4, 5);
                    display.print("PROTO");
                    display.setCursor(4, 23);
                    display.print("WARE");
                    display.setTextSize(1);
                    display.setCursor(4, 43);
                    display.print("Multi-Tool ESP32");
                    display.setCursor(4, 54);
                    display.print("< Back");
                    display.drawBitmap(68, 0, image_Teapot_bits, 63, 64, SH110X_WHITE);
                    display.display();
                }
            }
            break;
        }

        case SETTING_STATE_INTERFACE: {
            if (upPress) {
                byte prev = interfaceIndex;
                interfaceIndex = (interfaceIndex + INTERFACE_MENU_COUNT - 1) % INTERFACE_MENU_COUNT;
                displaySubmenu(display, interfaceMenuItems, INTERFACE_MENU_COUNT, interfaceIndex, prev);
            } else if (downPress) {
                byte prev = interfaceIndex;
                interfaceIndex = (interfaceIndex + 1) % INTERFACE_MENU_COUNT;
                displaySubmenu(display, interfaceMenuItems, INTERFACE_MENU_COUNT, interfaceIndex, prev);
            } else if (backClick) {
                currentState = SETTING_STATE_ROOT;
                displaySubmenu(display, settingsMenuItems, SETTINGS_MENU_ITEM_COUNT, settingsMenuIndex);
            } else if (okClick) {
                if (interfaceIndex == 0) { // Menu Style
                    currentState = SETTING_STATE_MENU_STYLE;
                    styleIndex = menu;
                    displaySubmenu(display, menuStyleItems, MENU_STYLE_COUNT, styleIndex);
                } else if (interfaceIndex == 1) { // Submenu Style
                    currentState = SETTING_STATE_SUBMENU_STYLE;
                    submenuStyleIndex = submenu;
                    displaySubmenu(display, submenuStyleItems, SUBMENU_STYLE_COUNT, submenuStyleIndex);
                } else if (interfaceIndex == 2) { // Color
                    currentState = SETTING_STATE_COLOR;
                    colorIndex = colorSelectionIndex;
                    displaySubmenu(display, colorItems, COLOR_ITEM_COUNT, colorIndex);
                } else if (interfaceIndex == 3) { // Standby
                    currentState = SETTING_STATE_STANDBY;
                    standbyIndex = standbyTimeoutIndex;
                    displaySubmenu(display, standbyTimeoutLabels, STANDBY_ITEM_COUNT, standbyIndex);
                } else if (interfaceIndex == 4) { // Back
                    currentState = SETTING_STATE_ROOT;
                    displaySubmenu(display, settingsMenuItems, SETTINGS_MENU_ITEM_COUNT, settingsMenuIndex);
                }
            }
            break;
        }

        case SETTING_STATE_MENU_STYLE: {
            if (upPress) {
                byte prev = styleIndex;
                styleIndex = (styleIndex + MENU_STYLE_COUNT - 1) % MENU_STYLE_COUNT;
                displaySubmenu(display, menuStyleItems, MENU_STYLE_COUNT, styleIndex, prev);
            } else if (downPress) {
                byte prev = styleIndex;
                styleIndex = (styleIndex + 1) % MENU_STYLE_COUNT;
                displaySubmenu(display, menuStyleItems, MENU_STYLE_COUNT, styleIndex, prev);
            } else if (backClick) {
                currentState = SETTING_STATE_INTERFACE;
                displaySubmenu(display, interfaceMenuItems, INTERFACE_MENU_COUNT, interfaceIndex);
            } else if (okClick) {
                menu = styleIndex;
                saveAppearanceConfig();
                showFeedback("Menu Style", menuStyleItems[menu]);
                currentState = SETTING_STATE_INTERFACE;
                displaySubmenu(display, interfaceMenuItems, INTERFACE_MENU_COUNT, interfaceIndex);
            }
            break;
        }

        case SETTING_STATE_SUBMENU_STYLE: {
            if (upPress || downPress) {
                byte prev = submenuStyleIndex;
                submenuStyleIndex = (submenuStyleIndex == 0) ? 1 : 0;
                displaySubmenu(display, submenuStyleItems, SUBMENU_STYLE_COUNT, submenuStyleIndex, prev);
            } else if (backClick) {
                currentState = SETTING_STATE_INTERFACE;
                displaySubmenu(display, interfaceMenuItems, INTERFACE_MENU_COUNT, interfaceIndex);
            } else if (okClick) {
                submenu = submenuStyleIndex;
                saveAppearanceConfig();
                showFeedback("Submenu Style", submenuStyleItems[submenu]);
                currentState = SETTING_STATE_INTERFACE;
                displaySubmenu(display, interfaceMenuItems, INTERFACE_MENU_COUNT, interfaceIndex);
            }
            break;
        }

        case SETTING_STATE_COLOR: {
            if (upPress || downPress) {
                byte prev = colorIndex;
                colorIndex = (colorIndex == 0) ? 1 : 0;
                displaySubmenu(display, colorItems, COLOR_ITEM_COUNT, colorIndex, prev);
            } else if (backClick) {
                currentState = SETTING_STATE_INTERFACE;
                displaySubmenu(display, interfaceMenuItems, INTERFACE_MENU_COUNT, interfaceIndex);
            } else if (okClick) {
                colorSelectionIndex = colorIndex;
                applyCurrentColorScheme();
                saveAppearanceConfig();
                showFeedback("Color Scheme", colorItems[colorSelectionIndex]);
                currentState = SETTING_STATE_INTERFACE;
                displaySubmenu(display, interfaceMenuItems, INTERFACE_MENU_COUNT, interfaceIndex);
            }
            break;
        }

        case SETTING_STATE_STANDBY: {
            if (upPress) {
                byte prev = standbyIndex;
                standbyIndex = (standbyIndex + STANDBY_ITEM_COUNT - 1) % STANDBY_ITEM_COUNT;
                displaySubmenu(display, standbyTimeoutLabels, STANDBY_ITEM_COUNT, standbyIndex, prev);
            } else if (downPress) {
                byte prev = standbyIndex;
                standbyIndex = (standbyIndex + 1) % STANDBY_ITEM_COUNT;
                displaySubmenu(display, standbyTimeoutLabels, STANDBY_ITEM_COUNT, standbyIndex, prev);
            } else if (backClick) {
                currentState = SETTING_STATE_INTERFACE;
                displaySubmenu(display, interfaceMenuItems, INTERFACE_MENU_COUNT, interfaceIndex);
            } else if (okClick) {
                standbyTimeoutIndex = standbyIndex;
                standbyTimeoutMs = standbyTimeoutOptionsMs[standbyTimeoutIndex];
                saveAppearanceConfig();
                showFeedback("Standby Timeout", standbyTimeoutLabels[standbyTimeoutIndex]);
                currentState = SETTING_STATE_INTERFACE;
                displaySubmenu(display, interfaceMenuItems, INTERFACE_MENU_COUNT, interfaceIndex);
            }
            break;
        }

        case SETTING_STATE_ABOUT: {
            if (okClick || backClick || upPress || downPress) {
                currentState = SETTING_STATE_ROOT;
                displaySubmenu(display, settingsMenuItems, SETTINGS_MENU_ITEM_COUNT, settingsMenuIndex);
            }
            break;
        }
    }
}

