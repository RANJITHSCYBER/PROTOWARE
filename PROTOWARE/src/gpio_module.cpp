#include "gpio_module.h"

#include <Arduino.h>
#include <GyverButton.h>
#include <U8g2_for_Adafruit_GFX.h>

#include "CONFIG.h"
#include "pindefs.h"
#include "display.h"
#include "interface/interface.h"
#include "menu/gpio.h"

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
extern byte gpioMenuIndex;

extern void returnToMainMenu();
extern void resetActivityTimer();

// U8g2 font instance for rotated vertical pin labels
static U8G2_FOR_ADAFRUIT_GFX u8g2;
extern const uint8_t u8g2_font_4x6_tr[];

// =============================================================================
// HEADER PIN DEFINITIONS (24 PINS)
// =============================================================================
struct HeaderPin {
    const char* label;
    int8_t gpio;       // -1 for Power or Ground
    bool isOutput;    // Can drive PWM / output
    bool isSystem;    // Reserved by system hardware (Buttons, OLED display)
    const char* desc;
};

// Top Header Row (12 Pins)
static const HeaderPin topHeaderPins[12] = {
    {"3.3", -1, false, false, "3.3V Power"},
    {"SCL", 22, false, true,  "GPIO 22 [OLED SCL]"},
    {"SDA", 21, false, true,  "GPIO 21 [OLED SDA]"},
    {"2",    2, true,  false, "GPIO 2 (Available)"},
    {"18",  18, true,  false, "GPIO 18 [SPI SCK]"},
    {"19",  19, true,  false, "GPIO 19 [SPI MISO]"},
    {"23",  23, true,  false, "GPIO 23 [SPI MOSI]"},
    {"14",  14, true,  false, "GPIO 14 (Available)"},
    {"12",  12, true,  false, "GPIO 12 (Available)"},
    {"13",  13, true,  false, "GPIO 13 [SD CS]"},
    {"3.3", -1, false, false, "3.3V Power"},
    {"GND", -1, false, false, "Ground"}
};

// Bottom Header Row (12 Pins)
static const HeaderPin bottomHeaderPins[12] = {
    {"+5",  -1, false, false, "+5V Power (VIN)"},
    {"GND", -1, false, false, "Ground"},
    {"32",  32, false, true,  "GPIO 32 [Center Btn]"},
    {"27",  27, false, true,  "GPIO 27 [Right Btn]"},
    {"26",  26, false, true,  "GPIO 26 [Up Btn]"},
    {"25",  25, false, true,  "GPIO 25 [Left Btn]"},
    {"39",  39, false, false, "GPIO 39 [Input Only]"},
    {"36",  36, false, false, "GPIO 36 [Input Only]"},
    {"35",  35, false, false, "GPIO 35 [Input Only]"},
    {"34",  34, false, false, "GPIO 34 [Input Only]"},
    {"3.3", -1, false, false, "3.3V Power"},
    {"GND", -1, false, false, "Ground"}
};

// =============================================================================
// MODULE STATES
// =============================================================================
enum GPIOState : byte {
    GPIO_STATE_MENU = 0,
    GPIO_STATE_PIN_PICKER,
    GPIO_STATE_SERVO,
    GPIO_STATE_SINE_WAVE,
    GPIO_STATE_SQUARE_WAVE,
    GPIO_STATE_TRIANGLE_WAVE,
    GPIO_STATE_BLINK_LED,
    GPIO_STATE_FLASH_LED,
    GPIO_STATE_MOTOR_DRIVER,
    GPIO_STATE_RELAY,
    GPIO_STATE_NEO_PIXEL,
    GPIO_STATE_PUSH_BUTTON
};

static GPIOState currentGpioState = GPIO_STATE_MENU;
static byte activeFunctionIdx = 0;

// Pin picker cursor state (Defaults to GPIO 2: Top Row, Col 3 - 100% SAFE FREE GPIO!)
static int pickerRow = 0;       // 0: top row, 1: bottom row
static int pickerCol = 3;       // Col 3 = GPIO 2
static int8_t selectedGpio = 2; // Active locked GPIO pin
static char pickerNotice[28] = "";
static unsigned long pickerNoticeExpires = 0;

// LEDC Channel allocation
static const uint8_t GPIO_LEDC_CH = 7;

// =============================================================================
// SUB-FUNCTION PARAMETERS
// =============================================================================
// 1. Servo
static int servoAngle = 90;
static uint32_t servoPulseUs = 1500;
static int servoFocus = 0;      // 0: Angle Control, 1: [Edit Pin], 2: [Exit]

// 2. Sine Wave
static int sineFreq = 1000;     // Hz
static int sineFocus = 0;
static float sinePhase = 0.0f;
static unsigned long lastSineAnimTime = 0;

// 3. Square Wave
static int squareFreq = 1000;   // Hz
static int squareDuty = 50;     // %
static int squareFocus = 0;
static float squarePhase = 0.0f;
static unsigned long lastSquareAnimTime = 0;

// 4. Triangle Wave
static int triangleFreq = 500;  // Hz
static int triangleFocus = 0;
static float trianglePhase = 0.0f;
static unsigned long lastTriangleAnimTime = 0;

// 5. Blink LED
static int blinkRateIdx = 0;    // 0: 1Hz, 1: 2Hz, 2: 5Hz, 3: 10Hz, 4: Manual
static const int blinkRates[] = {1, 2, 5, 10, 0};
static bool blinkLedState = false;
static unsigned long lastBlinkToggle = 0;
static int blinkFocus = 0;

// 6. Flash LED
static int flashMode = 0;       // 0: Strobe, 1: Torch (ON), 2: Pulse
static int flashFreq = 15;      // Hz
static bool flashState = false;
static unsigned long lastFlashToggle = 0;
static int flashFocus = 0;

// 7. Motor Driver
static int motorSpeed = 50;     // 0..100 %
static bool motorFwd = true;
static int motorFocus = 0;

// 8. Relay
static bool relayState = false;
static int relayFocus = 0;

// 9. NeoPixel
static int neoColorIdx = 0;
static const char* neoColorNames[] = {"Red", "Green", "Blue", "White", "Rainbow", "Off"};
static int neoFocus = 0;

// 10. Push Button
static int buttonPressCount = 0;
static bool buttonLastRawState = HIGH;
static uint8_t buttonHistory[16] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
static unsigned long lastButtonSample = 0;
static int btnFocus = 0;

// Forward declarations
static void renderVisualPinSelector();
static void renderServoScreen();
static void renderSineScreen();
static void renderSquareScreen();
static void renderTriangleScreen();
static void renderBlinkScreen();
static void renderFlashScreen();
static void renderMotorScreen();
static void renderRelayScreen();
static void renderNeoScreen();
static void renderButtonScreen();

static void updateServoPWM();
static void stopAllGpioDrivers();
static void selectCurrentPin();

// =============================================================================
// INITIALIZATION
// =============================================================================
void initGPIOModule() {
    u8g2.begin(display);
    Serial.println("[GPIO] Module Initialized with Safe System Pin Protection.");
}

void enterGPIOSubmenu() {
    stopAllGpioDrivers();
    currentGpioState = GPIO_STATE_MENU;
    gpioMenuIndex = 0;
    displayGPIOMenu(display, gpioMenuIndex);
}

// =============================================================================
// STOP ALL GPIO DRIVERS SAFELY
// =============================================================================
static void stopAllGpioDrivers() {
    if (selectedGpio >= 0) {
        ledcWrite(GPIO_LEDC_CH, 0);
        ledcDetachPin(selectedGpio);
        pinMode(selectedGpio, INPUT);
    }
    blinkLedState = false;
    flashState = false;
    relayState = false;
}

// Exit cleanly from any sub-state back to GPIO Submenu
static unsigned long ignoreGpioMenuUntil = 0;

static void exitToGPIOMenu() {
    stopAllGpioDrivers();
    currentGpioState = GPIO_STATE_MENU;
    ignoreGpioMenuUntil = millis() + 350; // Guard against button click leakage
    buttonBack.isClick();
    buttonOK.isClick();
    buttonCenter.isClick();
    displayGPIOMenu(display, gpioMenuIndex);
}

// =============================================================================
// CLEAN & BEAUTIFUL VISUAL PIN SELECTOR
// =============================================================================
static void renderVisualPinSelector() {
    display.clearDisplay();
    display.setTextWrap(false);

    // 1. DIP IC Package Body
    display.drawRoundRect(2, 19, 124, 26, 3, SH110X_WHITE);

    // DIP IC orientation notch on left edge
    display.drawCircle(2, 32, 3, SH110X_WHITE);
    display.fillRect(0, 30, 3, 5, SH110X_BLACK);
    display.drawCircle(2, 32, 3, SH110X_WHITE);

    // 2. Connector Solder Pads (12 Top and 12 Bottom)
    for (int c = 0; c < 12; c++) {
        int px = 7 + c * 10;
        // Top contact pad (extends 3px into the chip)
        display.fillRect(px, 19, 4, 3, SH110X_WHITE);
        // Bottom contact pad (extends 3px into the chip)
        display.fillRect(px, 42, 4, 3, SH110X_WHITE);
    }

    // 3. Vertical rotated labels using U8G2
    u8g2.setFontMode(1);
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.setFontDirection(3); // 270 degrees
    u8g2.setForegroundColor(SH110X_WHITE);

    // Draw Top Row Labels with uniform baseline at y = 17
    for (int c = 0; c < 12; c++) {
        u8g2.drawStr(11 + c * 10, 17, topHeaderPins[c].label);
    }

    // Draw Bottom Row Labels with uniform baseline at y = 61
    for (int c = 0; c < 12; c++) {
        u8g2.drawStr(11 + c * 10, 61, bottomHeaderPins[c].label);
    }

    // 4. Highlight the Selected Pin
    int selX = 6 + pickerCol * 10;
    if (pickerRow == 0) {
        // High-contrast inverted highlight box on top pin label
        display.fillRect(selX, 1, 7, 17, SH110X_INVERSE);
        // Crisp directional pointer inside chip pointing UP at the pad
        display.fillTriangle(8 + pickerCol * 10, 23, 6 + pickerCol * 10, 27, 10 + pickerCol * 10, 27, SH110X_WHITE);
    } else {
        // High-contrast inverted highlight box on bottom pin label
        display.fillRect(selX, 45, 7, 17, SH110X_INVERSE);
        // Crisp directional pointer inside chip pointing DOWN at the pad
        display.fillTriangle(8 + pickerCol * 10, 40, 6 + pickerCol * 10, 36, 10 + pickerCol * 10, 36, SH110X_WHITE);
    }

    // 5. Real-Time Pin Inspector Readout inside Chip Center (y = 24..38)
    const HeaderPin &curPin = (pickerRow == 0) ? topHeaderPins[pickerCol] : bottomHeaderPins[pickerCol];

    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);

    // Line 1: Pin Number / Function Name
    char pinTitle[24];
    if (curPin.gpio == -1) {
        snprintf(pinTitle, sizeof(pinTitle), "%s", curPin.desc);
    } else {
        snprintf(pinTitle, sizeof(pinTitle), "GPIO %d", curPin.gpio);
    }

    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(pinTitle, 0, 0, &x1, &y1, &w, &h);
    int cx = (128 - w) / 2;
    display.setCursor(cx, 25);
    display.print(pinTitle);

    // Line 2: Pin Status / Button Guide
    const char* hintText;
    if (pickerNotice[0] != '\0' && millis() < pickerNoticeExpires) {
        hintText = pickerNotice;
    } else if (curPin.gpio == -1) {
        hintText = "Power/GND Pin";
    } else if (curPin.isSystem) {
        hintText = "* System Pin! *";
    } else if (activeFunctionIdx != 9 && !curPin.isOutput) {
        hintText = "Input Only Pin";
    } else {
        hintText = "SELECT: Choose";
    }

    display.getTextBounds(hintText, 0, 0, &x1, &y1, &w, &h);
    int hx = (128 - w) / 2;
    display.setCursor(hx, 34);
    display.print(hintText);

    display.display();
}

static void showPickerNotice(const char* msg) {
    strncpy(pickerNotice, msg, sizeof(pickerNotice) - 1);
    pickerNotice[sizeof(pickerNotice) - 1] = '\0';
    pickerNoticeExpires = millis() + 1400;
    renderVisualPinSelector();
}

// =============================================================================
// SELECT CURRENT PIN
// =============================================================================
static void selectCurrentPin() {
    const HeaderPin &pin = (pickerRow == 0) ? topHeaderPins[pickerCol] : bottomHeaderPins[pickerCol];

    if (pin.gpio == -1) {
        showPickerNotice("Not a GPIO!");
        return;
    }

    // Prevent hijacking navigation buttons or OLED display I2C bus
    if (pin.isSystem) {
        showPickerNotice("System Pin (In Use)!");
        return;
    }

    // Output check for Servo, Wave, Blink, Relay, Motor, NeoPixel
    if (activeFunctionIdx != 9 && !pin.isOutput) {
        showPickerNotice("Input Only Pin!");
        return;
    }

    // Lock in selected GPIO pin
    selectedGpio = pin.gpio;
    Serial.printf("[GPIO] Pin locked: GPIO %d (%s) for function %d\n", selectedGpio, pin.desc, activeFunctionIdx);

    // Transition to active function state
    switch (activeFunctionIdx) {
        case 0: // Servo
            servoFocus = 0;
            currentGpioState = GPIO_STATE_SERVO;
            // Configure output pin & initialize LEDC 50Hz PWM
            pinMode(selectedGpio, OUTPUT);
            ledcSetup(GPIO_LEDC_CH, 50, 16);
            ledcAttachPin(selectedGpio, GPIO_LEDC_CH);
            updateServoPWM();
            renderServoScreen();
            break;
        case 1: // Sine wave
            pinMode(selectedGpio, OUTPUT);
            sineFocus = 0;
            sinePhase = 0.0f;
            lastSineAnimTime = millis();
            currentGpioState = GPIO_STATE_SINE_WAVE;
            renderSineScreen();
            break;
        case 2: // Square wave
            pinMode(selectedGpio, OUTPUT);
            ledcSetup(GPIO_LEDC_CH, squareFreq, 10);
            ledcAttachPin(selectedGpio, GPIO_LEDC_CH);
            ledcWrite(GPIO_LEDC_CH, (squareDuty * 1023ULL) / 100ULL);
            squareFocus = 0;
            squarePhase = 0.0f;
            lastSquareAnimTime = millis();
            currentGpioState = GPIO_STATE_SQUARE_WAVE;
            renderSquareScreen();
            break;
        case 3: // Triangle wave
            pinMode(selectedGpio, OUTPUT);
            triangleFocus = 0;
            trianglePhase = 0.0f;
            lastTriangleAnimTime = millis();
            currentGpioState = GPIO_STATE_TRIANGLE_WAVE;
            renderTriangleScreen();
            break;
        case 4: // Blink LED
            pinMode(selectedGpio, OUTPUT);
            blinkFocus = 0;
            currentGpioState = GPIO_STATE_BLINK_LED;
            renderBlinkScreen();
            break;
        case 5: // Flash LED
            pinMode(selectedGpio, OUTPUT);
            flashFocus = 0;
            currentGpioState = GPIO_STATE_FLASH_LED;
            renderFlashScreen();
            break;
        case 6: // Motor Driver
            pinMode(selectedGpio, OUTPUT);
            ledcSetup(GPIO_LEDC_CH, 5000, 8);
            ledcAttachPin(selectedGpio, GPIO_LEDC_CH);
            ledcWrite(GPIO_LEDC_CH, (motorSpeed * 255ULL) / 100ULL);
            motorFocus = 0;
            currentGpioState = GPIO_STATE_MOTOR_DRIVER;
            renderMotorScreen();
            break;
        case 7: // Relay
            pinMode(selectedGpio, OUTPUT);
            digitalWrite(selectedGpio, relayState ? HIGH : LOW);
            relayFocus = 0;
            currentGpioState = GPIO_STATE_RELAY;
            renderRelayScreen();
            break;
        case 8: // Neo Pixel
            pinMode(selectedGpio, OUTPUT);
            neoFocus = 0;
            currentGpioState = GPIO_STATE_NEO_PIXEL;
            renderNeoScreen();
            break;
        case 9: // Push button
            if (selectedGpio >= 34 && selectedGpio <= 39) {
                pinMode(selectedGpio, INPUT);
            } else {
                pinMode(selectedGpio, INPUT_PULLUP);
            }
            btnFocus = 0;
            currentGpioState = GPIO_STATE_PUSH_BUTTON;
            renderButtonScreen();
            break;
        default:
            break;
    }
}

// =============================================================================
// PIN PICKER 5-WAY NAVIGATION & SELECT
// =============================================================================
static void handlePinPickerNavigation() {
    // Drain GyverButton clicks so LEFT and RIGHT behave purely as directional arrows
    buttonBack.isClick();
    buttonOK.isClick();

    static MenuButtonState upState, downState, leftState, rightState;
    bool up = isMenuButtonPress(BUTTON_PIN_UP, upState, 150);
    bool down = isMenuButtonPress(BUTTON_PIN_DOWN, downState, 150);
    bool left = isMenuButtonPress(BUTTON_PIN_LEFT, leftState, 150);
    bool right = isMenuButtonPress(BUTTON_PIN_RIGHT, rightState, 150);

    // Dedicated Select button (Center / Pin 32)
    static unsigned long centerPressStart = 0;
    static bool centerWasPressed = false;
    bool centerPressed = (digitalRead(BUTTON_PIN_CENTER) == LOW);
    bool centerClick = false;

    if (centerPressed) {
        if (!centerWasPressed) {
            centerWasPressed = true;
            centerPressStart = millis();
        } else {
            // Hold Center (> 600ms) = Cancel / Back to GPIO Submenu
            if (centerPressStart > 0 && (millis() - centerPressStart >= 600)) {
                centerPressStart = 0;
                centerWasPressed = false;
                exitToGPIOMenu();
                return;
            }
        }
    } else {
        if (centerWasPressed) {
            unsigned long dur = (centerPressStart > 0) ? (millis() - centerPressStart) : 0;
            centerWasPressed = false;
            centerPressStart = 0;
            if (dur >= BUTTON_DEBOUNCE_MS && dur < 600) {
                centerClick = true;
            }
        }
    }

    if (centerClick) {
        selectCurrentPin();
        return;
    }

    if (up || down || left || right) {
        resetActivityTimer();
    }

    if (up) {
        pickerRow = 0; // Move up to top row
        renderVisualPinSelector();
    } else if (down) {
        pickerRow = 1; // Move down to bottom row
        renderVisualPinSelector();
    } else if (left) {
        pickerCol = (pickerCol > 0) ? pickerCol - 1 : 11; // Move left
        renderVisualPinSelector();
    } else if (right) {
        pickerCol = (pickerCol < 11) ? pickerCol + 1 : 0; // Move right
        renderVisualPinSelector();
    }
}

// =============================================================================
// 1. SERVO CONTROLLER IMPLEMENTATION
// =============================================================================
static void updateServoPWM() {
    if (servoAngle < 0) servoAngle = 0;
    if (servoAngle > 180) servoAngle = 180;

    // Pulse: 500us (0 deg) to 2500us (180 deg)
    servoPulseUs = 500 + ((uint32_t)servoAngle * 2000ULL) / 180ULL;

    // 50Hz period = 20,000us -> 16 bit timer (65535 ticks)
    uint32_t duty = (servoPulseUs * 65535ULL) / 20000ULL;
    ledcWrite(GPIO_LEDC_CH, duty);
}

static void renderServoScreen() {
    display.clearDisplay();
    display.setTextWrap(false);

    // 1. Header Bar (y = 0..12)
    display.setTextColor(SH110X_WHITE);
    display.setTextSize(1);
    display.setCursor(4, 2);
    display.print("SERVO");

    // Pin badge at top right (x = 76 to 124, w = 48, h = 12)
    display.drawRoundRect(76, 0, 48, 12, 2, SH110X_WHITE);
    display.setCursor(80, 2);
    display.printf("PIN %2d", selectedGpio);

    display.drawLine(0, 13, 127, 13, SH110X_WHITE);

    // 2. Position Readouts (y = 16..24)
    // Left: Angle readout (x = 6 to 54)
    display.setCursor(6, 16);
    display.printf("ANG: %3d\xF7", servoAngle);

    // Right: Pulse width (x = 66 to 126)
    display.setCursor(66, 16);
    display.printf("PW: %4dus", servoPulseUs);

    // 3. Scale Ticks & Labels (y = 26)
    display.setCursor(14, 26);
    display.print("0\xF7");
    display.setCursor(56, 26);
    display.print("90\xF7");
    display.setCursor(98, 26);
    display.print("180\xF7");

    // Center 90-degree tick mark
    display.drawLine(64, 32, 64, 34, SH110X_WHITE);

    // Progress Bar (x = 14 to 114, w = 100, h = 9)
    display.drawRoundRect(14, 34, 100, 9, 2, SH110X_WHITE);
    int barW = (servoAngle * 96) / 180;
    if (barW > 0) {
        display.fillRect(16, 36, barW, 5, SH110X_WHITE);
    }

    // 4. Directional sweep arrows when in angle adjustment mode
    if (servoFocus == 0) {
        display.fillTriangle(6, 38, 10, 35, 10, 41, SH110X_WHITE);
        display.fillTriangle(121, 38, 117, 35, 117, 41, SH110X_WHITE);
    }

    // 5. Action buttons at bottom (y = 48..61)
    // Option 1: [Edit Pin] (x = 16, y = 48, w = 52, h = 13)
    if (servoFocus == 1) {
        display.fillRoundRect(16, 48, 52, 13, 2, SH110X_WHITE);
        display.setTextColor(SH110X_BLACK, SH110X_WHITE);
        display.setCursor(20, 51);
        display.print("Edit Pin");
    } else {
        display.drawRoundRect(16, 48, 52, 13, 2, SH110X_WHITE);
        display.setTextColor(SH110X_WHITE);
        display.setCursor(20, 51);
        display.print("Edit Pin");
    }

    // Option 2: [Exit] (x = 76, y = 48, w = 36, h = 13)
    if (servoFocus == 2) {
        display.fillRoundRect(76, 48, 36, 13, 2, SH110X_WHITE);
        display.setTextColor(SH110X_BLACK, SH110X_WHITE);
        display.setCursor(82, 51);
        display.print("Exit");
    } else {
        display.drawRoundRect(76, 48, 36, 13, 2, SH110X_WHITE);
        display.setTextColor(SH110X_WHITE);
        display.setCursor(82, 51);
        display.print("Exit");
    }

    display.display();
}

static void handleServoControls() {
    // Drain GyverButton clicks so LEFT and RIGHT behave purely as directional controls
    buttonBack.isClick();
    buttonOK.isClick();

    static MenuButtonState leftState;
    static MenuButtonState rightState;
    static MenuButtonState upState;
    static MenuButtonState downState;

    bool up = isMenuButtonPress(BUTTON_PIN_UP, upState, 180);
    bool down = isMenuButtonPress(BUTTON_PIN_DOWN, downState, 180);
    bool left = isMenuButtonPress(BUTTON_PIN_LEFT, leftState, 35);   // Smooth rapid rotation sweep
    bool right = isMenuButtonPress(BUTTON_PIN_RIGHT, rightState, 35); // Smooth rapid rotation sweep

    // Dedicated Select button (Center / Pin 32)
    static unsigned long centerPressStart = 0;
    static bool centerWasPressed = false;
    bool centerPressed = (digitalRead(BUTTON_PIN_CENTER) == LOW);
    bool centerClick = false;

    if (centerPressed) {
        if (!centerWasPressed) {
            centerWasPressed = true;
            centerPressStart = millis();
        } else {
            // Hold Center (> 600ms) = Back to GPIO Submenu
            if (centerPressStart > 0 && (millis() - centerPressStart >= 600)) {
                centerPressStart = 0;
                centerWasPressed = false;
                exitToGPIOMenu();
                return;
            }
        }
    } else {
        if (centerWasPressed) {
            unsigned long dur = (centerPressStart > 0) ? (millis() - centerPressStart) : 0;
            centerWasPressed = false;
            centerPressStart = 0;
            if (dur >= BUTTON_DEBOUNCE_MS && dur < 600) {
                centerClick = true;
            }
        }
    }

    if (up || down || left || right || centerClick) {
        resetActivityTimer();
    }

    // FOCUS 0: SERVO ANGLE SWEEP
    if (servoFocus == 0) {
        if (left && servoAngle > 0) {
            servoAngle = max(0, servoAngle - 2);
            updateServoPWM();
            renderServoScreen();
            return;
        }
        if (right && servoAngle < 180) {
            servoAngle = min(180, servoAngle + 2);
            updateServoPWM();
            renderServoScreen();
            return;
        }
        if (down) {
            servoFocus = 1; // Down to [Edit Pin]
            renderServoScreen();
            return;
        }
        if (centerClick) {
            servoFocus = 1;
            renderServoScreen();
            return;
        }
    }
    // FOCUS 1: [Edit Pin]
    else if (servoFocus == 1) {
        if (right || down) {
            servoFocus = 2; // Right / Down across to [Exit]
            renderServoScreen();
            return;
        }
        if (up) {
            servoFocus = 0; // Up back to Servo Angle
            renderServoScreen();
            return;
        }
        if (centerClick) {
            stopAllGpioDrivers();
            currentGpioState = GPIO_STATE_PIN_PICKER;
            renderVisualPinSelector();
            return;
        }
    }
    // FOCUS 2: [Exit]
    else if (servoFocus == 2) {
        if (left) {
            servoFocus = 1; // Left to [Edit Pin]
            renderServoScreen();
            return;
        }
        if (up) {
            servoFocus = 0; // Up back to Servo Angle
            renderServoScreen();
            return;
        }
        if (centerClick) {
            exitToGPIOMenu();
            return;
        }
    }
}

// =============================================================================
// 2. SINE WAVE GENERATOR
// =============================================================================
static void renderSineScreen() {
    display.clearDisplay();
    display.setTextWrap(false);
    display.setTextColor(SH110X_WHITE);
    display.setTextSize(1);

    display.setCursor(4, 2);
    display.print("SINE WAVE");
    display.drawRoundRect(76, 0, 48, 12, 2, SH110X_WHITE);
    display.setCursor(80, 2);
    display.printf("PIN %2d", selectedGpio);
    display.drawLine(0, 13, 127, 13, SH110X_WHITE);

    display.setCursor(6, 16);
    display.printf("FREQ: %4d Hz", sineFreq);

    // Draw smooth, continuously moving visual sine wave
    int prevY = 33 + (int)(sinf(sinePhase) * 9.0f);
    for (int x = 15; x <= 114; x++) {
        float rad = (float)(x - 14) * 0.13f + sinePhase;
        int y = 33 + (int)(sinf(rad) * 9.0f);
        display.drawLine(x - 1, prevY, x, y, SH110X_WHITE);
        prevY = y;
    }

    if (sineFocus == 1) {
        display.fillRoundRect(16, 48, 52, 13, 2, SH110X_WHITE);
        display.setTextColor(SH110X_BLACK, SH110X_WHITE);
        display.setCursor(20, 51);
        display.print("Edit Pin");
    } else {
        display.drawRoundRect(16, 48, 52, 13, 2, SH110X_WHITE);
        display.setTextColor(SH110X_WHITE);
        display.setCursor(20, 51);
        display.print("Edit Pin");
    }

    if (sineFocus == 2) {
        display.fillRoundRect(76, 48, 36, 13, 2, SH110X_WHITE);
        display.setTextColor(SH110X_BLACK, SH110X_WHITE);
        display.setCursor(82, 51);
        display.print("Exit");
    } else {
        display.drawRoundRect(76, 48, 36, 13, 2, SH110X_WHITE);
        display.setTextColor(SH110X_WHITE);
        display.setCursor(82, 51);
        display.print("Exit");
    }

    display.display();
}

static void handleSineControls() {
    buttonBack.isClick();
    buttonOK.isClick();

    // Smooth animated wave motion
    unsigned long now = millis();
    if (now - lastSineAnimTime >= 35) {
        lastSineAnimTime = now;
        sinePhase += 0.22f;
        if (sinePhase >= 6.283185f) sinePhase -= 6.283185f;
        renderSineScreen();
    }

    static MenuButtonState leftState, rightState, upState, downState;
    bool up = isMenuButtonPress(BUTTON_PIN_UP, upState, 180);
    bool down = isMenuButtonPress(BUTTON_PIN_DOWN, downState, 180);
    bool left = isMenuButtonPress(BUTTON_PIN_LEFT, leftState, 60);
    bool right = isMenuButtonPress(BUTTON_PIN_RIGHT, rightState, 60);

    static unsigned long centerPressStart = 0;
    static bool centerWasPressed = false;
    bool centerPressed = (digitalRead(BUTTON_PIN_CENTER) == LOW);
    bool centerClick = false;

    if (centerPressed) {
        if (!centerWasPressed) {
            centerWasPressed = true;
            centerPressStart = millis();
        } else {
            if (centerPressStart > 0 && (millis() - centerPressStart >= 600)) {
                centerPressStart = 0;
                centerWasPressed = false;
                exitToGPIOMenu();
                return;
            }
        }
    } else {
        if (centerWasPressed) {
            unsigned long dur = (centerPressStart > 0) ? (millis() - centerPressStart) : 0;
            centerWasPressed = false;
            centerPressStart = 0;
            if (dur >= BUTTON_DEBOUNCE_MS && dur < 600) centerClick = true;
        }
    }

    if (up || down || left || right || centerClick) {
        resetActivityTimer();
    }

    if (sineFocus == 0) {
        if (left && sineFreq > 100) { sineFreq -= 50; renderSineScreen(); return; }
        if (right && sineFreq < 10000) { sineFreq += 50; renderSineScreen(); return; }
        if (down) { sineFocus = 1; renderSineScreen(); return; }
        if (centerClick) { sineFocus = 1; renderSineScreen(); return; }
    } else if (sineFocus == 1) {
        if (right || down) { sineFocus = 2; renderSineScreen(); return; }
        if (up) { sineFocus = 0; renderSineScreen(); return; }
        if (centerClick) {
            stopAllGpioDrivers();
            currentGpioState = GPIO_STATE_PIN_PICKER;
            renderVisualPinSelector();
            return;
        }
    } else if (sineFocus == 2) {
        if (left) { sineFocus = 1; renderSineScreen(); return; }
        if (up) { sineFocus = 0; renderSineScreen(); return; }
        if (centerClick) {
            exitToGPIOMenu();
            return;
        }
    }
}

// =============================================================================
// 3. SQUARE WAVE GENERATOR
// =============================================================================
static void updateSquareWave() {
    ledcSetup(GPIO_LEDC_CH, squareFreq, 10);
    ledcAttachPin(selectedGpio, GPIO_LEDC_CH);
    uint32_t duty = (squareDuty * 1023ULL) / 100ULL;
    ledcWrite(GPIO_LEDC_CH, duty);
}

static void renderSquareScreen() {
    display.clearDisplay();
    display.setTextWrap(false);
    display.setTextColor(SH110X_WHITE);
    display.setTextSize(1);

    display.setCursor(4, 2);
    display.print("SQUARE WAVE");
    display.drawRoundRect(76, 0, 48, 12, 2, SH110X_WHITE);
    display.setCursor(80, 2);
    display.printf("PIN %2d", selectedGpio);
    display.drawLine(0, 13, 127, 13, SH110X_WHITE);

    display.setCursor(6, 16);
    display.printf("FREQ:%5dHz", squareFreq);
    display.setCursor(76, 16);
    display.printf("DUTY:%2d%%", squareDuty);

    // Draw smooth, continuously moving visual square wave
    int highW = (24 * squareDuty) / 100;
    if (highW < 2) highW = 2;
    if (highW > 22) highW = 22;

    int offset = (int)squarePhase;
    int prevRelX = (14 + offset) % 24;
    int prevY = (prevRelX < highW) ? 26 : 40;

    for (int x = 15; x <= 114; x++) {
        int relX = (x + offset) % 24;
        int curY = (relX < highW) ? 26 : 40;
        if (curY != prevY) {
            display.drawLine(x - 1, 26, x - 1, 40, SH110X_WHITE);
            display.drawLine(x - 1, curY, x, curY, SH110X_WHITE);
        } else {
            display.drawLine(x - 1, curY, x, curY, SH110X_WHITE);
        }
        prevY = curY;
    }

    if (squareFocus == 1) {
        display.fillRoundRect(16, 48, 52, 13, 2, SH110X_WHITE);
        display.setTextColor(SH110X_BLACK, SH110X_WHITE);
        display.setCursor(20, 51);
        display.print("Edit Pin");
    } else {
        display.drawRoundRect(16, 48, 52, 13, 2, SH110X_WHITE);
        display.setTextColor(SH110X_WHITE);
        display.setCursor(20, 51);
        display.print("Edit Pin");
    }

    if (squareFocus == 2) {
        display.fillRoundRect(76, 48, 36, 13, 2, SH110X_WHITE);
        display.setTextColor(SH110X_BLACK, SH110X_WHITE);
        display.setCursor(82, 51);
        display.print("Exit");
    } else {
        display.drawRoundRect(76, 48, 36, 13, 2, SH110X_WHITE);
        display.setTextColor(SH110X_WHITE);
        display.setCursor(82, 51);
        display.print("Exit");
    }

    display.display();
}

static void handleSquareControls() {
    buttonBack.isClick();
    buttonOK.isClick();

    // Smooth animated wave motion
    unsigned long now = millis();
    if (now - lastSquareAnimTime >= 35) {
        lastSquareAnimTime = now;
        squarePhase += 1.0f;
        if (squarePhase >= 24.0f) squarePhase -= 24.0f;
        renderSquareScreen();
    }

    static MenuButtonState leftState, rightState, upState, downState;
    bool up = isMenuButtonPress(BUTTON_PIN_UP, upState, 180);
    bool down = isMenuButtonPress(BUTTON_PIN_DOWN, downState, 180);
    bool left = isMenuButtonPress(BUTTON_PIN_LEFT, leftState, 60);
    bool right = isMenuButtonPress(BUTTON_PIN_RIGHT, rightState, 60);

    static unsigned long centerPressStart = 0;
    static bool centerWasPressed = false;
    bool centerPressed = (digitalRead(BUTTON_PIN_CENTER) == LOW);
    bool centerClick = false;

    if (centerPressed) {
        if (!centerWasPressed) {
            centerWasPressed = true;
            centerPressStart = millis();
        } else {
            if (centerPressStart > 0 && (millis() - centerPressStart >= 600)) {
                centerPressStart = 0;
                centerWasPressed = false;
                exitToGPIOMenu();
                return;
            }
        }
    } else {
        if (centerWasPressed) {
            unsigned long dur = (centerPressStart > 0) ? (millis() - centerPressStart) : 0;
            centerWasPressed = false;
            centerPressStart = 0;
            if (dur >= BUTTON_DEBOUNCE_MS && dur < 600) centerClick = true;
        }
    }

    if (up || down || left || right || centerClick) {
        resetActivityTimer();
    }

    if (squareFocus == 0) {
        if (left && squareFreq > 50) { squareFreq -= 50; updateSquareWave(); renderSquareScreen(); return; }
        if (right && squareFreq < 40000) { squareFreq += 50; updateSquareWave(); renderSquareScreen(); return; }
        if (down) { squareFocus = 1; renderSquareScreen(); return; }
        if (centerClick) { squareFocus = 1; renderSquareScreen(); return; }
    } else if (squareFocus == 1) {
        if (right || down) { squareFocus = 2; renderSquareScreen(); return; }
        if (up) { squareFocus = 0; renderSquareScreen(); return; }
        if (centerClick) {
            stopAllGpioDrivers();
            currentGpioState = GPIO_STATE_PIN_PICKER;
            renderVisualPinSelector();
            return;
        }
    } else if (squareFocus == 2) {
        if (left) { squareFocus = 1; renderSquareScreen(); return; }
        if (up) { squareFocus = 0; renderSquareScreen(); return; }
        if (centerClick) {
            exitToGPIOMenu();
            return;
        }
    }
}

// =============================================================================
// 4. TRIANGLE WAVE GENERATOR
// =============================================================================
static int getTriWaveY(int px, int off) {
    int r = (px + off) % 20;
    if (r < 0) r += 20;
    if (r < 10) {
        return 40 - (r * 14) / 10;
    } else {
        return 26 + ((r - 10) * 14) / 10;
    }
}

static void renderTriangleScreen() {
    display.clearDisplay();
    display.setTextWrap(false);
    display.setTextColor(SH110X_WHITE);
    display.setTextSize(1);

    display.setCursor(4, 2);
    display.print("TRIANGLE");
    display.drawRoundRect(76, 0, 48, 12, 2, SH110X_WHITE);
    display.setCursor(80, 2);
    display.printf("PIN %2d", selectedGpio);
    display.drawLine(0, 13, 127, 13, SH110X_WHITE);

    display.setCursor(6, 16);
    display.printf("FREQ: %4d Hz", triangleFreq);

    // Draw smooth, continuously moving visual triangle wave
    int off = (int)trianglePhase;
    int prevY = getTriWaveY(14, off);
    for (int x = 15; x <= 114; x++) {
        int curY = getTriWaveY(x, off);
        display.drawLine(x - 1, prevY, x, curY, SH110X_WHITE);
        prevY = curY;
    }

    if (triangleFocus == 1) {
        display.fillRoundRect(16, 48, 52, 13, 2, SH110X_WHITE);
        display.setTextColor(SH110X_BLACK, SH110X_WHITE);
        display.setCursor(20, 51);
        display.print("Edit Pin");
    } else {
        display.drawRoundRect(16, 48, 52, 13, 2, SH110X_WHITE);
        display.setTextColor(SH110X_WHITE);
        display.setCursor(20, 51);
        display.print("Edit Pin");
    }

    if (triangleFocus == 2) {
        display.fillRoundRect(76, 48, 36, 13, 2, SH110X_WHITE);
        display.setTextColor(SH110X_BLACK, SH110X_WHITE);
        display.setCursor(82, 51);
        display.print("Exit");
    } else {
        display.drawRoundRect(76, 48, 36, 13, 2, SH110X_WHITE);
        display.setTextColor(SH110X_WHITE);
        display.setCursor(82, 51);
        display.print("Exit");
    }

    display.display();
}

static void handleTriangleControls() {
    buttonBack.isClick();
    buttonOK.isClick();

    // Smooth animated wave motion
    unsigned long now = millis();
    if (now - lastTriangleAnimTime >= 35) {
        lastTriangleAnimTime = now;
        trianglePhase += 1.0f;
        if (trianglePhase >= 20.0f) trianglePhase -= 20.0f;
        renderTriangleScreen();
    }

    static MenuButtonState leftState, rightState, upState, downState;
    bool up = isMenuButtonPress(BUTTON_PIN_UP, upState, 180);
    bool down = isMenuButtonPress(BUTTON_PIN_DOWN, downState, 180);
    bool left = isMenuButtonPress(BUTTON_PIN_LEFT, leftState, 60);
    bool right = isMenuButtonPress(BUTTON_PIN_RIGHT, rightState, 60);

    static unsigned long centerPressStart = 0;
    static bool centerWasPressed = false;
    bool centerPressed = (digitalRead(BUTTON_PIN_CENTER) == LOW);
    bool centerClick = false;

    if (centerPressed) {
        if (!centerWasPressed) {
            centerWasPressed = true;
            centerPressStart = millis();
        } else {
            if (centerPressStart > 0 && (millis() - centerPressStart >= 600)) {
                centerPressStart = 0;
                centerWasPressed = false;
                exitToGPIOMenu();
                return;
            }
        }
    } else {
        if (centerWasPressed) {
            unsigned long dur = (centerPressStart > 0) ? (millis() - centerPressStart) : 0;
            centerWasPressed = false;
            centerPressStart = 0;
            if (dur >= BUTTON_DEBOUNCE_MS && dur < 600) centerClick = true;
        }
    }

    if (up || down || left || right || centerClick) {
        resetActivityTimer();
    }

    if (triangleFocus == 0) {
        if (left && triangleFreq > 50) { triangleFreq -= 25; renderTriangleScreen(); return; }
        if (right && triangleFreq < 5000) { triangleFreq += 25; renderTriangleScreen(); return; }
        if (down) { triangleFocus = 1; renderTriangleScreen(); return; }
        if (centerClick) { triangleFocus = 1; renderTriangleScreen(); return; }
    } else if (triangleFocus == 1) {
        if (right || down) { triangleFocus = 2; renderTriangleScreen(); return; }
        if (up) { triangleFocus = 0; renderTriangleScreen(); return; }
        if (centerClick) {
            stopAllGpioDrivers();
            currentGpioState = GPIO_STATE_PIN_PICKER;
            renderVisualPinSelector();
            return;
        }
    } else if (triangleFocus == 2) {
        if (left) { triangleFocus = 1; renderTriangleScreen(); return; }
        if (up) { triangleFocus = 0; renderTriangleScreen(); return; }
        if (centerClick) {
            exitToGPIOMenu();
            return;
        }
    }
}

// =============================================================================
// 5. BLINK LED
// =============================================================================
static void renderBlinkScreen() {
    display.clearDisplay();
    display.setTextWrap(false);
    display.setTextColor(SH110X_WHITE);
    display.setTextSize(1);

    display.setCursor(4, 2);
    display.print("BLINK LED");
    display.drawRoundRect(76, 0, 48, 12, 2, SH110X_WHITE);
    display.setCursor(80, 2);
    display.printf("PIN %2d", selectedGpio);
    display.drawLine(0, 13, 127, 13, SH110X_WHITE);

    display.setCursor(6, 16);
    if (blinkRates[blinkRateIdx] > 0) {
        display.printf("RATE: %2d Hz", blinkRates[blinkRateIdx]);
    } else {
        display.print("RATE: MANUAL");
    }

    display.setCursor(76, 16);
    display.printf("LED:%s", blinkLedState ? "ON " : "OFF");

    // Big visual LED indicator
    if (blinkLedState) {
        display.fillCircle(64, 33, 7, SH110X_WHITE);
        display.drawCircle(64, 33, 10, SH110X_WHITE);
    } else {
        display.drawCircle(64, 33, 7, SH110X_WHITE);
    }

    if (blinkFocus == 1) {
        display.fillRoundRect(16, 48, 52, 13, 2, SH110X_WHITE);
        display.setTextColor(SH110X_BLACK, SH110X_WHITE);
        display.setCursor(20, 51);
        display.print("Edit Pin");
    } else {
        display.drawRoundRect(16, 48, 52, 13, 2, SH110X_WHITE);
        display.setTextColor(SH110X_WHITE);
        display.setCursor(20, 51);
        display.print("Edit Pin");
    }

    if (blinkFocus == 2) {
        display.fillRoundRect(76, 48, 36, 13, 2, SH110X_WHITE);
        display.setTextColor(SH110X_BLACK, SH110X_WHITE);
        display.setCursor(82, 51);
        display.print("Exit");
    } else {
        display.drawRoundRect(76, 48, 36, 13, 2, SH110X_WHITE);
        display.setTextColor(SH110X_WHITE);
        display.setCursor(82, 51);
        display.print("Exit");
    }

    display.display();
}

static void handleBlinkControls() {
    buttonBack.isClick();
    buttonOK.isClick();

    static MenuButtonState leftState, rightState, upState, downState;
    bool up = isMenuButtonPress(BUTTON_PIN_UP, upState, 180);
    bool down = isMenuButtonPress(BUTTON_PIN_DOWN, downState, 180);
    bool left = isMenuButtonPress(BUTTON_PIN_LEFT, leftState, 180);
    bool right = isMenuButtonPress(BUTTON_PIN_RIGHT, rightState, 180);

    static unsigned long centerPressStart = 0;
    static bool centerWasPressed = false;
    bool centerPressed = (digitalRead(BUTTON_PIN_CENTER) == LOW);
    bool centerClick = false;

    if (centerPressed) {
        if (!centerWasPressed) {
            centerWasPressed = true;
            centerPressStart = millis();
        } else {
            if (centerPressStart > 0 && (millis() - centerPressStart >= 600)) {
                centerPressStart = 0;
                centerWasPressed = false;
                exitToGPIOMenu();
                return;
            }
        }
    } else {
        if (centerWasPressed) {
            unsigned long dur = (centerPressStart > 0) ? (millis() - centerPressStart) : 0;
            centerWasPressed = false;
            centerPressStart = 0;
            if (dur >= BUTTON_DEBOUNCE_MS && dur < 600) centerClick = true;
        }
    }

    if (up || down || left || right || centerClick) {
        resetActivityTimer();
    }

    // Auto blink logic
    int r = blinkRates[blinkRateIdx];
    if (r > 0) {
        unsigned long interval = 500 / r;
        if (millis() - lastBlinkToggle >= interval) {
            lastBlinkToggle = millis();
            blinkLedState = !blinkLedState;
            digitalWrite(selectedGpio, blinkLedState ? HIGH : LOW);
            renderBlinkScreen();
        }
    }

    if (blinkFocus == 0) {
        if (left) {
            blinkRateIdx = (blinkRateIdx > 0) ? blinkRateIdx - 1 : 4;
            renderBlinkScreen();
            return;
        }
        if (right) {
            blinkRateIdx = (blinkRateIdx < 4) ? blinkRateIdx + 1 : 0;
            renderBlinkScreen();
            return;
        }
        if (down) {
            blinkFocus = 1;
            renderBlinkScreen();
            return;
        }
        if (centerClick) {
            if (r == 0) {
                blinkLedState = !blinkLedState;
                digitalWrite(selectedGpio, blinkLedState ? HIGH : LOW);
                renderBlinkScreen();
            } else {
                blinkFocus = 1;
                renderBlinkScreen();
            }
            return;
        }
    } else if (blinkFocus == 1) {
        if (right || down) {
            blinkFocus = 2;
            renderBlinkScreen();
            return;
        }
        if (up) {
            blinkFocus = 0;
            renderBlinkScreen();
            return;
        }
        if (centerClick) {
            stopAllGpioDrivers();
            currentGpioState = GPIO_STATE_PIN_PICKER;
            renderVisualPinSelector();
            return;
        }
    } else if (blinkFocus == 2) {
        if (left) {
            blinkFocus = 1;
            renderBlinkScreen();
            return;
        }
        if (up) {
            blinkFocus = 0;
            renderBlinkScreen();
            return;
        }
        if (centerClick) {
            exitToGPIOMenu();
            return;
        }
    }
}

// =============================================================================
// 6. FLASH LED
// =============================================================================
static void renderFlashScreen() {
    display.clearDisplay();
    display.setTextWrap(false);
    display.setTextColor(SH110X_WHITE);
    display.setTextSize(1);

    display.setCursor(4, 2);
    display.print("FLASH LED");
    display.drawRoundRect(76, 0, 48, 12, 2, SH110X_WHITE);
    display.setCursor(80, 2);
    display.printf("PIN %2d", selectedGpio);
    display.drawLine(0, 13, 127, 13, SH110X_WHITE);

    display.setCursor(6, 16);
    if (flashMode == 0) display.printf("MODE: STROBE %2dHz", flashFreq);
    else if (flashMode == 1) display.print("MODE: TORCH (ON) ");
    else display.print("MODE: PULSE FLASH");

    // Visual strobe box
    if (flashState) {
        display.fillRect(36, 29, 56, 12, SH110X_WHITE);
        display.setTextColor(SH110X_BLACK, SH110X_WHITE);
        display.setCursor(48, 31);
        display.print("*FLASH*");
    } else {
        display.drawRect(36, 29, 56, 12, SH110X_WHITE);
        display.setTextColor(SH110X_WHITE);
        display.setCursor(48, 31);
        display.print(" READY ");
    }

    if (flashFocus == 1) {
        display.fillRoundRect(16, 48, 52, 13, 2, SH110X_WHITE);
        display.setTextColor(SH110X_BLACK, SH110X_WHITE);
        display.setCursor(20, 51);
        display.print("Edit Pin");
    } else {
        display.drawRoundRect(16, 48, 52, 13, 2, SH110X_WHITE);
        display.setTextColor(SH110X_WHITE);
        display.setCursor(20, 51);
        display.print("Edit Pin");
    }

    if (flashFocus == 2) {
        display.fillRoundRect(76, 48, 36, 13, 2, SH110X_WHITE);
        display.setTextColor(SH110X_BLACK, SH110X_WHITE);
        display.setCursor(82, 51);
        display.print("Exit");
    } else {
        display.drawRoundRect(76, 48, 36, 13, 2, SH110X_WHITE);
        display.setTextColor(SH110X_WHITE);
        display.setCursor(82, 51);
        display.print("Exit");
    }

    display.display();
}

static void handleFlashControls() {
    buttonBack.isClick();
    buttonOK.isClick();

    static MenuButtonState leftState, rightState, upState, downState;
    bool up = isMenuButtonPress(BUTTON_PIN_UP, upState, 180);
    bool down = isMenuButtonPress(BUTTON_PIN_DOWN, downState, 180);
    bool left = isMenuButtonPress(BUTTON_PIN_LEFT, leftState, 180);
    bool right = isMenuButtonPress(BUTTON_PIN_RIGHT, rightState, 180);

    static unsigned long centerPressStart = 0;
    static bool centerWasPressed = false;
    bool centerPressed = (digitalRead(BUTTON_PIN_CENTER) == LOW);
    bool centerClick = false;

    if (centerPressed) {
        if (!centerWasPressed) {
            centerWasPressed = true;
            centerPressStart = millis();
        } else {
            if (centerPressStart > 0 && (millis() - centerPressStart >= 600)) {
                centerPressStart = 0;
                centerWasPressed = false;
                exitToGPIOMenu();
                return;
            }
        }
    } else {
        if (centerWasPressed) {
            unsigned long dur = (centerPressStart > 0) ? (millis() - centerPressStart) : 0;
            centerWasPressed = false;
            centerPressStart = 0;
            if (dur >= BUTTON_DEBOUNCE_MS && dur < 600) centerClick = true;
        }
    }

    if (up || down || left || right || centerClick) {
        resetActivityTimer();
    }

    if (flashMode == 0) {
        // Strobe
        unsigned long interval = 500 / flashFreq;
        if (millis() - lastFlashToggle >= interval) {
            lastFlashToggle = millis();
            flashState = !flashState;
            digitalWrite(selectedGpio, flashState ? HIGH : LOW);
            renderFlashScreen();
        }
    }

    if (flashFocus == 0) {
        if (left) {
            flashMode = (flashMode > 0) ? flashMode - 1 : 2;
            flashState = (flashMode == 1);
            digitalWrite(selectedGpio, flashState ? HIGH : LOW);
            renderFlashScreen();
            return;
        }
        if (right) {
            flashMode = (flashMode < 2) ? flashMode + 1 : 0;
            flashState = (flashMode == 1);
            digitalWrite(selectedGpio, flashState ? HIGH : LOW);
            renderFlashScreen();
            return;
        }
        if (down) {
            flashFocus = 1;
            renderFlashScreen();
            return;
        }
        if (centerClick) {
            if (flashMode == 2) {
                // Pulse flash
                digitalWrite(selectedGpio, HIGH);
                flashState = true;
                renderFlashScreen();
                delay(100);
                digitalWrite(selectedGpio, LOW);
                flashState = false;
                renderFlashScreen();
            } else {
                flashFocus = 1;
                renderFlashScreen();
            }
            return;
        }
    } else if (flashFocus == 1) {
        if (right || down) {
            flashFocus = 2;
            renderFlashScreen();
            return;
        }
        if (up) {
            flashFocus = 0;
            renderFlashScreen();
            return;
        }
        if (centerClick) {
            stopAllGpioDrivers();
            currentGpioState = GPIO_STATE_PIN_PICKER;
            renderVisualPinSelector();
            return;
        }
    } else if (flashFocus == 2) {
        if (left) {
            flashFocus = 1;
            renderFlashScreen();
            return;
        }
        if (up) {
            flashFocus = 0;
            renderFlashScreen();
            return;
        }
        if (centerClick) {
            exitToGPIOMenu();
            return;
        }
    }
}

// =============================================================================
// 7. MOTOR DRIVER
// =============================================================================
static void updateMotorPWM() {
    ledcSetup(GPIO_LEDC_CH, 5000, 8);
    ledcAttachPin(selectedGpio, GPIO_LEDC_CH);
    uint32_t duty = (motorSpeed * 255ULL) / 100ULL;
    ledcWrite(GPIO_LEDC_CH, duty);
}

static void renderMotorScreen() {
    display.clearDisplay();
    display.setTextWrap(false);
    display.setTextColor(SH110X_WHITE);
    display.setTextSize(1);

    display.setCursor(4, 2);
    display.print("MOTOR PWM");
    display.drawRoundRect(76, 0, 48, 12, 2, SH110X_WHITE);
    display.setCursor(80, 2);
    display.printf("PIN %2d", selectedGpio);
    display.drawLine(0, 13, 127, 13, SH110X_WHITE);

    display.setCursor(6, 16);
    display.printf("SPD: %3d%%", motorSpeed);
    display.setCursor(76, 16);
    display.printf("DIR: %s", motorFwd ? "FWD" : "REV");

    // Progress bar for speed
    display.drawRoundRect(14, 30, 100, 10, 2, SH110X_WHITE);
    int w = (motorSpeed * 96) / 100;
    if (w > 0) display.fillRect(16, 32, w, 6, SH110X_WHITE);

    if (motorFocus == 1) {
        display.fillRoundRect(16, 48, 52, 13, 2, SH110X_WHITE);
        display.setTextColor(SH110X_BLACK, SH110X_WHITE);
        display.setCursor(20, 51);
        display.print("Edit Pin");
    } else {
        display.drawRoundRect(16, 48, 52, 13, 2, SH110X_WHITE);
        display.setTextColor(SH110X_WHITE);
        display.setCursor(20, 51);
        display.print("Edit Pin");
    }

    if (motorFocus == 2) {
        display.fillRoundRect(76, 48, 36, 13, 2, SH110X_WHITE);
        display.setTextColor(SH110X_BLACK, SH110X_WHITE);
        display.setCursor(82, 51);
        display.print("Exit");
    } else {
        display.drawRoundRect(76, 48, 36, 13, 2, SH110X_WHITE);
        display.setTextColor(SH110X_WHITE);
        display.setCursor(82, 51);
        display.print("Exit");
    }

    display.display();
}

static void handleMotorControls() {
    buttonBack.isClick();
    buttonOK.isClick();

    static MenuButtonState leftState, rightState, upState, downState;
    bool up = isMenuButtonPress(BUTTON_PIN_UP, upState, 180);
    bool down = isMenuButtonPress(BUTTON_PIN_DOWN, downState, 180);
    bool left = isMenuButtonPress(BUTTON_PIN_LEFT, leftState, 50);
    bool right = isMenuButtonPress(BUTTON_PIN_RIGHT, rightState, 50);

    static unsigned long centerPressStart = 0;
    static bool centerWasPressed = false;
    bool centerPressed = (digitalRead(BUTTON_PIN_CENTER) == LOW);
    bool centerClick = false;

    if (centerPressed) {
        if (!centerWasPressed) {
            centerWasPressed = true;
            centerPressStart = millis();
        } else {
            if (centerPressStart > 0 && (millis() - centerPressStart >= 600)) {
                centerPressStart = 0;
                centerWasPressed = false;
                exitToGPIOMenu();
                return;
            }
        }
    } else {
        if (centerWasPressed) {
            unsigned long dur = (centerPressStart > 0) ? (millis() - centerPressStart) : 0;
            centerWasPressed = false;
            centerPressStart = 0;
            if (dur >= BUTTON_DEBOUNCE_MS && dur < 600) centerClick = true;
        }
    }

    if (up || down || left || right || centerClick) {
        resetActivityTimer();
    }

    if (motorFocus == 0) {
        if (left && motorSpeed > 0) { motorSpeed = max(0, motorSpeed - 5); updateMotorPWM(); renderMotorScreen(); return; }
        if (right && motorSpeed < 100) { motorSpeed = min(100, motorSpeed + 5); updateMotorPWM(); renderMotorScreen(); return; }
        if (down) { motorFocus = 1; renderMotorScreen(); return; }
        if (centerClick) {
            motorFwd = !motorFwd;
            renderMotorScreen();
            return;
        }
    } else if (motorFocus == 1) {
        if (right || down) { motorFocus = 2; renderMotorScreen(); return; }
        if (up) { motorFocus = 0; renderMotorScreen(); return; }
        if (centerClick) {
            stopAllGpioDrivers();
            currentGpioState = GPIO_STATE_PIN_PICKER;
            renderVisualPinSelector();
            return;
        }
    } else if (motorFocus == 2) {
        if (left) { motorFocus = 1; renderMotorScreen(); return; }
        if (up) { motorFocus = 0; renderMotorScreen(); return; }
        if (centerClick) {
            exitToGPIOMenu();
            return;
        }
    }
}

// =============================================================================
// 8. RELAY CONTROLLER
// =============================================================================
static void renderRelayScreen() {
    display.clearDisplay();
    display.setTextWrap(false);
    display.setTextColor(SH110X_WHITE);
    display.setTextSize(1);

    display.setCursor(4, 2);
    display.print("RELAY");
    display.drawRoundRect(76, 0, 48, 12, 2, SH110X_WHITE);
    display.setCursor(80, 2);
    display.printf("PIN %2d", selectedGpio);
    display.drawLine(0, 13, 127, 13, SH110X_WHITE);

    display.setCursor(6, 16);
    display.printf("STATE: %s", relayState ? "ON " : "OFF");
    display.setCursor(76, 16);
    display.printf("OUT: %s", relayState ? "HIGH" : "LOW ");

    // Big mechanical switch graphic
    display.drawRoundRect(28, 28, 72, 16, 4, SH110X_WHITE);
    if (relayState) {
        display.fillRoundRect(62, 30, 36, 12, 3, SH110X_WHITE);
        display.setTextColor(SH110X_BLACK, SH110X_WHITE);
        display.setCursor(70, 32);
        display.print("ON");
        display.setTextColor(SH110X_WHITE);
        display.setCursor(36, 32);
        display.print("OFF");
    } else {
        display.fillRoundRect(30, 30, 36, 12, 3, SH110X_WHITE);
        display.setTextColor(SH110X_BLACK, SH110X_WHITE);
        display.setCursor(38, 32);
        display.print("OFF");
        display.setTextColor(SH110X_WHITE);
        display.setCursor(72, 32);
        display.print("ON");
    }

    if (relayFocus == 1) {
        display.fillRoundRect(16, 48, 52, 13, 2, SH110X_WHITE);
        display.setTextColor(SH110X_BLACK, SH110X_WHITE);
        display.setCursor(20, 51);
        display.print("Edit Pin");
    } else {
        display.drawRoundRect(16, 48, 52, 13, 2, SH110X_WHITE);
        display.setTextColor(SH110X_WHITE);
        display.setCursor(20, 51);
        display.print("Edit Pin");
    }

    if (relayFocus == 2) {
        display.fillRoundRect(76, 48, 36, 13, 2, SH110X_WHITE);
        display.setTextColor(SH110X_BLACK, SH110X_WHITE);
        display.setCursor(82, 51);
        display.print("Exit");
    } else {
        display.drawRoundRect(76, 48, 36, 13, 2, SH110X_WHITE);
        display.setTextColor(SH110X_WHITE);
        display.setCursor(82, 51);
        display.print("Exit");
    }

    display.display();
}

static void handleRelayControls() {
    buttonBack.isClick();
    buttonOK.isClick();

    static MenuButtonState leftState, rightState, upState, downState;
    bool up = isMenuButtonPress(BUTTON_PIN_UP, upState, 180);
    bool down = isMenuButtonPress(BUTTON_PIN_DOWN, downState, 180);
    bool left = isMenuButtonPress(BUTTON_PIN_LEFT, leftState, 180);
    bool right = isMenuButtonPress(BUTTON_PIN_RIGHT, rightState, 180);

    static unsigned long centerPressStart = 0;
    static bool centerWasPressed = false;
    bool centerPressed = (digitalRead(BUTTON_PIN_CENTER) == LOW);
    bool centerClick = false;

    if (centerPressed) {
        if (!centerWasPressed) {
            centerWasPressed = true;
            centerPressStart = millis();
        } else {
            if (centerPressStart > 0 && (millis() - centerPressStart >= 600)) {
                centerPressStart = 0;
                centerWasPressed = false;
                exitToGPIOMenu();
                return;
            }
        }
    } else {
        if (centerWasPressed) {
            unsigned long dur = (centerPressStart > 0) ? (millis() - centerPressStart) : 0;
            centerWasPressed = false;
            centerPressStart = 0;
            if (dur >= BUTTON_DEBOUNCE_MS && dur < 600) centerClick = true;
        }
    }

    if (up || down || left || right || centerClick) {
        resetActivityTimer();
    }

    if (relayFocus == 0) {
        if (left || right || centerClick) {
            relayState = !relayState;
            digitalWrite(selectedGpio, relayState ? HIGH : LOW);
            renderRelayScreen();
            return;
        }
        if (down) {
            relayFocus = 1;
            renderRelayScreen();
            return;
        }
    } else if (relayFocus == 1) {
        if (right || down) {
            relayFocus = 2;
            renderRelayScreen();
            return;
        }
        if (up) {
            relayFocus = 0;
            renderRelayScreen();
            return;
        }
        if (centerClick) {
            stopAllGpioDrivers();
            currentGpioState = GPIO_STATE_PIN_PICKER;
            renderVisualPinSelector();
            return;
        }
    } else if (relayFocus == 2) {
        if (left) {
            relayFocus = 1;
            renderRelayScreen();
            return;
        }
        if (up) {
            relayFocus = 0;
            renderRelayScreen();
            return;
        }
        if (centerClick) {
            exitToGPIOMenu();
            return;
        }
    }
}

// =============================================================================
// 9. NEO PIXEL (RGB LED)
// =============================================================================
static void renderNeoScreen() {
    display.clearDisplay();
    display.setTextWrap(false);
    display.setTextColor(SH110X_WHITE);
    display.setTextSize(1);

    display.setCursor(4, 2);
    display.print("NEOPIXEL");
    display.drawRoundRect(76, 0, 48, 12, 2, SH110X_WHITE);
    display.setCursor(80, 2);
    display.printf("PIN %2d", selectedGpio);
    display.drawLine(0, 13, 127, 13, SH110X_WHITE);

    display.setCursor(6, 16);
    display.printf("COLOR: %s", neoColorNames[neoColorIdx]);

    // Draw RGB boxes
    for (int i = 0; i < 6; i++) {
        int bx = 16 + i * 16;
        if (i == neoColorIdx) {
            display.fillRect(bx, 30, 12, 10, SH110X_WHITE);
        } else {
            display.drawRect(bx, 30, 12, 10, SH110X_WHITE);
        }
    }

    if (neoFocus == 1) {
        display.fillRoundRect(16, 48, 52, 13, 2, SH110X_WHITE);
        display.setTextColor(SH110X_BLACK, SH110X_WHITE);
        display.setCursor(20, 51);
        display.print("Edit Pin");
    } else {
        display.drawRoundRect(16, 48, 52, 13, 2, SH110X_WHITE);
        display.setTextColor(SH110X_WHITE);
        display.setCursor(20, 51);
        display.print("Edit Pin");
    }

    if (neoFocus == 2) {
        display.fillRoundRect(76, 48, 36, 13, 2, SH110X_WHITE);
        display.setTextColor(SH110X_BLACK, SH110X_WHITE);
        display.setCursor(82, 51);
        display.print("Exit");
    } else {
        display.drawRoundRect(76, 48, 36, 13, 2, SH110X_WHITE);
        display.setTextColor(SH110X_WHITE);
        display.setCursor(82, 51);
        display.print("Exit");
    }

    display.display();
}

static void handleNeoControls() {
    buttonBack.isClick();
    buttonOK.isClick();

    static MenuButtonState leftState, rightState, upState, downState;
    bool up = isMenuButtonPress(BUTTON_PIN_UP, upState, 180);
    bool down = isMenuButtonPress(BUTTON_PIN_DOWN, downState, 180);
    bool left = isMenuButtonPress(BUTTON_PIN_LEFT, leftState, 180);
    bool right = isMenuButtonPress(BUTTON_PIN_RIGHT, rightState, 180);

    static unsigned long centerPressStart = 0;
    static bool centerWasPressed = false;
    bool centerPressed = (digitalRead(BUTTON_PIN_CENTER) == LOW);
    bool centerClick = false;

    if (centerPressed) {
        if (!centerWasPressed) {
            centerWasPressed = true;
            centerPressStart = millis();
        } else {
            if (centerPressStart > 0 && (millis() - centerPressStart >= 600)) {
                centerPressStart = 0;
                centerWasPressed = false;
                exitToGPIOMenu();
                return;
            }
        }
    } else {
        if (centerWasPressed) {
            unsigned long dur = (centerPressStart > 0) ? (millis() - centerPressStart) : 0;
            centerWasPressed = false;
            centerPressStart = 0;
            if (dur >= BUTTON_DEBOUNCE_MS && dur < 600) centerClick = true;
        }
    }

    if (up || down || left || right || centerClick) {
        resetActivityTimer();
    }

    if (neoFocus == 0) {
        if (left) {
            neoColorIdx = (neoColorIdx > 0) ? neoColorIdx - 1 : 5;
            renderNeoScreen();
            return;
        }
        if (right) {
            neoColorIdx = (neoColorIdx < 5) ? neoColorIdx + 1 : 0;
            renderNeoScreen();
            return;
        }
        if (down) {
            neoFocus = 1;
            renderNeoScreen();
            return;
        }
        if (centerClick) {
            neoFocus = 1;
            renderNeoScreen();
            return;
        }
    } else if (neoFocus == 1) {
        if (right || down) {
            neoFocus = 2;
            renderNeoScreen();
            return;
        }
        if (up) {
            neoFocus = 0;
            renderNeoScreen();
            return;
        }
        if (centerClick) {
            stopAllGpioDrivers();
            currentGpioState = GPIO_STATE_PIN_PICKER;
            renderVisualPinSelector();
            return;
        }
    } else if (neoFocus == 2) {
        if (left) {
            neoFocus = 1;
            renderNeoScreen();
            return;
        }
        if (up) {
            neoFocus = 0;
            renderNeoScreen();
            return;
        }
        if (centerClick) {
            exitToGPIOMenu();
            return;
        }
    }
}

// =============================================================================
// 10. PUSH BUTTON / DIGITAL INPUT TESTER
// =============================================================================
static void renderButtonScreen() {
    display.clearDisplay();
    display.setTextWrap(false);
    display.setTextColor(SH110X_WHITE);
    display.setTextSize(1);

    display.setCursor(4, 2);
    display.print("BUTTON / IN");
    display.drawRoundRect(76, 0, 48, 12, 2, SH110X_WHITE);
    display.setCursor(80, 2);
    display.printf("PIN %2d", selectedGpio);
    display.drawLine(0, 13, 127, 13, SH110X_WHITE);

    display.setCursor(6, 16);
    display.printf("VAL: %s", buttonLastRawState == HIGH ? "HIGH" : "LOW ");
    display.setCursor(76, 16);
    display.printf("CNT:%4d", buttonPressCount);

    // Live logic trace history
    for (int i = 0; i < 15; i++) {
        int x1 = 16 + i * 6;
        int x2 = x1 + 6;
        int y1 = (buttonHistory[i] == 1) ? 28 : 40;
        int y2 = (buttonHistory[i + 1] == 1) ? 28 : 40;
        display.drawLine(x1, y1, x2, y1, SH110X_WHITE);
        if (y1 != y2) display.drawLine(x2, y1, x2, y2, SH110X_WHITE);
    }

    if (btnFocus == 1) {
        display.fillRoundRect(16, 48, 52, 13, 2, SH110X_WHITE);
        display.setTextColor(SH110X_BLACK, SH110X_WHITE);
        display.setCursor(20, 51);
        display.print("Edit Pin");
    } else {
        display.drawRoundRect(16, 48, 52, 13, 2, SH110X_WHITE);
        display.setTextColor(SH110X_WHITE);
        display.setCursor(20, 51);
        display.print("Edit Pin");
    }

    if (btnFocus == 2) {
        display.fillRoundRect(76, 48, 36, 13, 2, SH110X_WHITE);
        display.setTextColor(SH110X_BLACK, SH110X_WHITE);
        display.setCursor(82, 51);
        display.print("Exit");
    } else {
        display.drawRoundRect(76, 48, 36, 13, 2, SH110X_WHITE);
        display.setTextColor(SH110X_WHITE);
        display.setCursor(82, 51);
        display.print("Exit");
    }

    display.display();
}

static void handleButtonControls() {
    buttonBack.isClick();
    buttonOK.isClick();

    static MenuButtonState leftState, rightState, upState, downState;
    bool up = isMenuButtonPress(BUTTON_PIN_UP, upState, 180);
    bool down = isMenuButtonPress(BUTTON_PIN_DOWN, downState, 180);
    bool left = isMenuButtonPress(BUTTON_PIN_LEFT, leftState, 180);
    bool right = isMenuButtonPress(BUTTON_PIN_RIGHT, rightState, 180);

    static unsigned long centerPressStart = 0;
    static bool centerWasPressed = false;
    bool centerPressed = (digitalRead(BUTTON_PIN_CENTER) == LOW);
    bool centerClick = false;

    if (centerPressed) {
        if (!centerWasPressed) {
            centerWasPressed = true;
            centerPressStart = millis();
        } else {
            if (centerPressStart > 0 && (millis() - centerPressStart >= 600)) {
                centerPressStart = 0;
                centerWasPressed = false;
                exitToGPIOMenu();
                return;
            }
        }
    } else {
        if (centerWasPressed) {
            unsigned long dur = (centerPressStart > 0) ? (millis() - centerPressStart) : 0;
            centerWasPressed = false;
            centerPressStart = 0;
            if (dur >= BUTTON_DEBOUNCE_MS && dur < 600) centerClick = true;
        }
    }

    if (up || down || left || right || centerClick) {
        resetActivityTimer();
    }

    // Read selected pin logic level
    bool raw = (digitalRead(selectedGpio) == HIGH);
    if (raw != buttonLastRawState) {
        buttonLastRawState = raw;
        if (!raw) buttonPressCount++; // Active LOW trigger
        renderButtonScreen();
    }

    // Scroll logic history periodically
    if (millis() - lastButtonSample >= 80) {
        lastButtonSample = millis();
        for (int i = 0; i < 15; i++) {
            buttonHistory[i] = buttonHistory[i + 1];
        }
        buttonHistory[15] = raw ? 1 : 0;
        renderButtonScreen();
    }

    if (btnFocus == 0) {
        if (down) {
            btnFocus = 1;
            renderButtonScreen();
            return;
        }
        if (centerClick) {
            btnFocus = 1;
            renderButtonScreen();
            return;
        }
    } else if (btnFocus == 1) {
        if (right || down) {
            btnFocus = 2;
            renderButtonScreen();
            return;
        }
        if (up) {
            btnFocus = 0;
            renderButtonScreen();
            return;
        }
        if (centerClick) {
            stopAllGpioDrivers();
            currentGpioState = GPIO_STATE_PIN_PICKER;
            renderVisualPinSelector();
            return;
        }
    } else if (btnFocus == 2) {
        if (left) {
            btnFocus = 1;
            renderButtonScreen();
            return;
        }
        if (up) {
            btnFocus = 0;
            renderButtonScreen();
            return;
        }
        if (centerClick) {
            exitToGPIOMenu();
            return;
        }
    }
}

// =============================================================================
// MAIN GPIO MENU ROUTER
// =============================================================================
void handleGPIOMenu() {
    // 1. Root Submenu Navigation
    if (currentGpioState == GPIO_STATE_MENU) {
        if (millis() < ignoreGpioMenuUntil) {
            buttonBack.isClick();
            buttonOK.isClick();
            buttonCenter.isClick();
            return;
        }

        static MenuButtonState subUpState;
        static MenuButtonState subDownState;
        unsigned long repeatDelay = getMenuSubmenuRepeatDelay(submenu == 1);

        bool up = isMenuButtonPress(BUTTON_PIN_UP, subUpState, repeatDelay);
        bool down = isMenuButtonPress(BUTTON_PIN_DOWN, subDownState, repeatDelay);
        bool clickBack = buttonBack.isClick();
        bool okClick = buttonOK.isClick() || buttonCenter.isClick();

        if (up || down || clickBack || okClick) {
            resetActivityTimer();
        }

        if (up) {
            byte prev = gpioMenuIndex;
            gpioMenuIndex = (gpioMenuIndex - 1 + GPIO_MENU_ITEM_COUNT) % GPIO_MENU_ITEM_COUNT;
            displayGPIOMenu(display, gpioMenuIndex, prev);
        } else if (down) {
            byte prev = gpioMenuIndex;
            gpioMenuIndex = (gpioMenuIndex + 1) % GPIO_MENU_ITEM_COUNT;
            displayGPIOMenu(display, gpioMenuIndex, prev);
        } else if (clickBack) {
            returnToMainMenu();
        } else if (okClick) {
            // Selected a function -> Open Visual Pin Selector
            activeFunctionIdx = gpioMenuIndex;
            // Default to safe external pin: GPIO 2 (Top Row, Col 3)
            pickerRow = 0;
            pickerCol = 3;
            pickerNotice[0] = '\0';
            currentGpioState = GPIO_STATE_PIN_PICKER;
            renderVisualPinSelector();
        }
        return;
    }

    // 2. Visual Pin Picker (UP, DOWN, LEFT, RIGHT, and SELECT button)
    if (currentGpioState == GPIO_STATE_PIN_PICKER) {
        handlePinPickerNavigation();
        return;
    }

    // 3. Servo Controller
    if (currentGpioState == GPIO_STATE_SERVO) {
        handleServoControls();
        return;
    }

    // 4. Sine Wave
    if (currentGpioState == GPIO_STATE_SINE_WAVE) {
        handleSineControls();
        return;
    }

    // 5. Square Wave
    if (currentGpioState == GPIO_STATE_SQUARE_WAVE) {
        handleSquareControls();
        return;
    }

    // 6. Triangle Wave
    if (currentGpioState == GPIO_STATE_TRIANGLE_WAVE) {
        handleTriangleControls();
        return;
    }

    // 7. Blink LED
    if (currentGpioState == GPIO_STATE_BLINK_LED) {
        handleBlinkControls();
        return;
    }

    // 8. Flash LED
    if (currentGpioState == GPIO_STATE_FLASH_LED) {
        handleFlashControls();
        return;
    }

    // 9. Motor Driver
    if (currentGpioState == GPIO_STATE_MOTOR_DRIVER) {
        handleMotorControls();
        return;
    }

    // 10. Relay Switch
    if (currentGpioState == GPIO_STATE_RELAY) {
        handleRelayControls();
        return;
    }

    // 11. NeoPixel
    if (currentGpioState == GPIO_STATE_NEO_PIXEL) {
        handleNeoControls();
        return;
    }

    // 12. Push Button
    if (currentGpioState == GPIO_STATE_PUSH_BUTTON) {
        handleButtonControls();
        return;
    }
}
