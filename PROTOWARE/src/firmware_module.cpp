#include "firmware_module.h"
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <Update.h>
#include <SD.h>
#include <vector>
#include <GyverButton.h>

#include "CONFIG.h"
#include "pindefs.h"
#include "display.h"
#include "interface/interface.h"
#include "menu/firmware.h"

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
extern byte firmwareMenuIndex;
extern SPIClass sdSPI;
extern bool sdCardReady;

extern void returnToMainMenu();
extern void resetActivityTimer();

// =============================================================================
// SUBMODULE STATES
// =============================================================================
enum FirmwareState : byte {
    FW_STATE_MENU = 0,
    FW_STATE_FLASH_LIST,
    FW_STATE_FLASH_CONFIRM,
    FW_STATE_FLASHING,
    FW_STATE_OTA_NOTICE,
    FW_STATE_VERSION
};

static FirmwareState currentFwState = FW_STATE_MENU;

// -----------------------------------------------------------------------------
// SD Card .bin Explorer
// -----------------------------------------------------------------------------
static const int MAX_BIN_FILES = 25;
static String binFilePaths[MAX_BIN_FILES];
static size_t binFileSizes[MAX_BIN_FILES];
static int totalBinFiles = 0;
static int binSelectedIdx = 0;
static int flashConfirmChoice = 0; // 0: Cancel, 1: Flash

static void scanSdForBinFiles() {
    totalBinFiles = 0;
    binSelectedIdx = 0;

    if (!sdCardReady) {
        sdSPI.begin(SPI_SCK_PIN, SPI_MISO_PIN, SPI_MOSI_PIN, SD_CS);
        sdCardReady = SD.begin(SD_CS, sdSPI);
    }
    if (!sdCardReady) return;

    const char* folders[3] = {"/firmware", "/bin", "/"};

    for (int f = 0; f < 3 && totalBinFiles < MAX_BIN_FILES; f++) {
        File dir = SD.open(folders[f]);
        if (!dir || !dir.isDirectory()) continue;

        File entry = dir.openNextFile();
        while (entry && totalBinFiles < MAX_BIN_FILES) {
            if (!entry.isDirectory()) {
                String name = entry.name();
                if (name.endsWith(".bin") || name.endsWith(".BIN")) {
                    String fullPath = (String(folders[f]) == "/") ? (String("/") + name) : (String(folders[f]) + "/" + name);

                    bool dup = false;
                    for (int i = 0; i < totalBinFiles; i++) {
                        if (binFilePaths[i] == fullPath) { dup = true; break; }
                    }
                    if (!dup) {
                        binFilePaths[totalBinFiles] = fullPath;
                        binFileSizes[totalBinFiles] = entry.size();
                        totalBinFiles++;
                    }
                }
            }
            entry = dir.openNextFile();
        }
        dir.close();
    }
}

static void renderBinFileList(int selected) {
    display.clearDisplay();
    display.setTextWrap(false);
    display.setTextColor(SH110X_WHITE);
    display.setTextSize(1);

    // Header
    display.fillRect(0, 0, 128, 11, SH110X_WHITE);
    display.setTextColor(SH110X_BLACK);
    display.setCursor(2, 2);
    display.print("SD Bin Files");
    if (totalBinFiles > 0) {
        display.setCursor(96, 2);
        display.printf("%d/%d", selected + 1, totalBinFiles);
    }

    if (!sdCardReady || totalBinFiles == 0) {
        display.setTextColor(SH110X_WHITE);
        display.setCursor(6, 20);
        display.print(!sdCardReady ? "No SD Card detected" : "No .bin files found");
        display.setCursor(6, 34);
        display.print("Put .bin into /firmware");
        display.setCursor(6, 52);
        display.print("< Back");
        display.display();
        return;
    }

    int visibleRows = 4;
    int startIdx = 0;
    if (selected >= visibleRows) {
        startIdx = selected - visibleRows + 1;
    }

    int y = 14;
    for (int i = startIdx; i < startIdx + visibleRows && i < totalBinFiles; i++) {
        bool isSel = (i == selected);
        if (isSel) {
            display.fillRect(0, y - 1, 128, 12, SH110X_WHITE);
            display.setTextColor(SH110X_BLACK);
        } else {
            display.setTextColor(SH110X_WHITE);
        }

        String displayName = binFilePaths[i];
        int lastSlash = displayName.lastIndexOf('/');
        if (lastSlash >= 0) displayName = displayName.substring(lastSlash + 1);

        if (displayName.length() > 11) {
            displayName = displayName.substring(0, 10) + "..";
        }
        display.setCursor(3, y + 1);
        display.print(displayName);

        // Size in KB
        display.setCursor(88, y + 1);
        display.print(binFileSizes[i] / 1024);
        display.print(" KB");

        y += 12;
    }

    display.display();
}

static void renderFlashConfirmDialog(int idx) {
    if (idx < 0 || idx >= totalBinFiles) return;

    display.clearDisplay();
    display.setTextWrap(false);

    // Modal frame
    display.drawRoundRect(4, 4, 120, 56, 3, SH110X_WHITE);
    display.fillRect(5, 5, 118, 11, SH110X_WHITE);

    display.setTextColor(SH110X_BLACK);
    display.setTextSize(1);
    display.setCursor(18, 7);
    display.print("FLASH FIRMWARE");

    display.setTextColor(SH110X_WHITE);
    display.setCursor(8, 19);
    display.print("File: ");
    String fname = binFilePaths[idx];
    int slash = fname.lastIndexOf('/');
    if (slash >= 0) fname = fname.substring(slash + 1);
    if (fname.length() > 13) fname = fname.substring(0, 12) + "..";
    display.print(fname);

    display.setCursor(8, 30);
    display.printf("Size: %u KB", (unsigned)(binFileSizes[idx] / 1024));

    // Buttons: [ Cancel ] [ Flash ]
    int yBtn = 43;
    if (flashConfirmChoice == 0) {
        display.fillRect(10, yBtn, 48, 13, SH110X_WHITE);
        display.setTextColor(SH110X_BLACK);
    } else {
        display.drawRoundRect(10, yBtn, 48, 13, 2, SH110X_WHITE);
        display.setTextColor(SH110X_WHITE);
    }
    display.setCursor(16, yBtn + 3);
    display.print("Cancel");

    if (flashConfirmChoice == 1) {
        display.fillRect(70, yBtn, 48, 13, SH110X_WHITE);
        display.setTextColor(SH110X_BLACK);
    } else {
        display.drawRoundRect(70, yBtn, 48, 13, 2, SH110X_WHITE);
        display.setTextColor(SH110X_WHITE);
    }
    display.setCursor(78, yBtn + 3);
    display.print("Flash");

    display.display();
}

static void renderFlashingProgress(const String& filename, int pct) {
    display.clearDisplay();
    display.setTextWrap(false);
    display.setTextColor(SH110X_WHITE);
    display.setTextSize(1);

    display.fillRect(0, 0, 128, 11, SH110X_WHITE);
    display.setTextColor(SH110X_BLACK);
    display.setCursor(2, 2);
    display.print("Flashing...");

    display.setTextColor(SH110X_WHITE);
    display.setCursor(3, 14);
    String fname = filename;
    int slash = fname.lastIndexOf('/');
    if (slash >= 0) fname = fname.substring(slash + 1);
    if (fname.length() > 17) fname = fname.substring(0, 16) + "..";
    display.print(fname);

    // Progress Bar
    display.drawRect(8, 27, 112, 10, SH110X_WHITE);
    int fillW = map(pct, 0, 100, 0, 108);
    if (fillW > 0) {
        display.fillRect(10, 29, fillW, 6, SH110X_WHITE);
    }

    display.setCursor(54, 40);
    display.print(pct);
    display.print("%");

    display.setCursor(4, 52);
    display.print("Do NOT power off!");
    display.display();
}

static bool performFlashFromSD(const String& filepath) {
    File f = SD.open(filepath, FILE_READ);
    if (!f) {
        display.clearDisplay();
        display.setTextColor(SH110X_WHITE);
        display.setCursor(10, 24);
        display.print("Cannot open file!");
        display.display();
        delay(1500);
        return false;
    }

    size_t fileSize = f.size();
    if (fileSize == 0) {
        f.close();
        display.clearDisplay();
        display.setTextColor(SH110X_WHITE);
        display.setCursor(10, 24);
        display.print("Empty firmware file!");
        display.display();
        delay(1500);
        return false;
    }

    renderFlashingProgress(filepath, 0);

    // Try finding next OTA partition (app1) for dual-boot
    const esp_partition_t* updatePart = esp_ota_get_next_update_partition(NULL);
    esp_ota_handle_t otaHandle = 0;
    bool useEspOta = false;

    if (updatePart != NULL && updatePart->size >= fileSize) {
        esp_err_t err = esp_ota_begin(updatePart, OTA_WITH_SEQUENTIAL_WRITES, &otaHandle);
        if (err == ESP_OK) {
            useEspOta = true;
        }
    }

    if (!useEspOta) {
        // Fallback to Arduino Update
        if (!Update.begin(fileSize, U_FLASH)) {
            f.close();
            display.clearDisplay();
            display.setTextColor(SH110X_WHITE);
            display.setCursor(10, 24);
            display.print("Update.begin failed!");
            display.display();
            delay(1500);
            return false;
        }
    }

    uint8_t buf[2048];
    size_t written = 0;
    int lastShownPct = -1;

    while (f.available()) {
        size_t r = f.read(buf, sizeof(buf));
        if (r == 0) break;

        if (useEspOta) {
            esp_err_t werr = esp_ota_write(otaHandle, buf, r);
            if (werr != ESP_OK) {
                esp_ota_abort(otaHandle);
                f.close();
                return false;
            }
        } else {
            if (Update.write(buf, r) != r) {
                Update.abort();
                f.close();
                return false;
            }
        }

        written += r;
        int pct = (written * 100) / fileSize;
        if (pct != lastShownPct) {
            lastShownPct = pct;
            renderFlashingProgress(filepath, pct);
        }
        yield();
    }
    f.close();

    bool success = false;
    if (useEspOta) {
        if (esp_ota_end(otaHandle) == ESP_OK) {
            if (esp_ota_set_boot_partition(updatePart) == ESP_OK) {
                success = true;
            }
        }
    } else {
        if (Update.end(true) && Update.isFinished()) {
            success = true;
        }
    }

    if (success) {
        renderFlashingProgress(filepath, 100);
        display.clearDisplay();
        display.setTextColor(SH110X_WHITE);
        display.setTextSize(1);
        display.setCursor(14, 12);
        display.print("Flashing Complete!");
        display.setCursor(14, 26);
        display.print("Booting guest OS...");
        display.setCursor(14, 40);
        display.print("On restart, returns");
        display.setCursor(14, 50);
        display.print("to PROTOWARE!");
        display.display();
        delay(2200);
        ESP.restart();
        return true;
    } else {
        display.clearDisplay();
        display.setTextColor(SH110X_WHITE);
        display.setCursor(10, 24);
        display.print("Flashing failed!");
        display.display();
        delay(1500);
        return false;
    }
}

// =============================================================================
// 2. OTA UPDATE NOTICE SCREEN
// =============================================================================
static void renderOtaNoticeScreen() {
    display.clearDisplay();
    display.setTextWrap(false);
    display.setTextColor(SH110X_WHITE);
    display.setTextSize(1);

    display.fillRect(0, 0, 128, 11, SH110X_WHITE);
    display.setTextColor(SH110X_BLACK);
    display.setCursor(2, 2);
    display.print("OTA Update");

    display.setTextColor(SH110X_WHITE);
    display.setCursor(8, 22);
    display.print("OTA Update Module");
    display.setCursor(8, 36);
    display.print("Pending specs.");
    display.setCursor(8, 52);
    display.print("< Back");

    display.display();
}

// =============================================================================
// 3. VERSION SCREEN
// =============================================================================
static void renderVersionScreen() {
    display.clearDisplay();
    display.setTextWrap(false);
    display.setTextColor(SH110X_WHITE);
    display.setTextSize(1);

    // Header
    display.fillRect(0, 0, 128, 11, SH110X_WHITE);
    display.setTextColor(SH110X_BLACK);
    display.setCursor(2, 2);
    display.print("PROTOWARE Version");

    display.setTextColor(SH110X_WHITE);
    display.setCursor(2, 14);
    display.print("OS   : PROTOWARE v2.1");

    display.setCursor(2, 24);
    display.print("Build: ");
    display.print(__DATE__);

    display.setCursor(2, 34);
    display.print("IDF  : ");
    String sdk = ESP.getSdkVersion();
    if (sdk.length() > 13) sdk = sdk.substring(0, 12) + ".";
    display.print(sdk);

    display.setCursor(2, 44);
    display.print("Chip : ESP32-D0WD");

    display.setCursor(2, 54);
    display.print("< Back");

    display.display();
}

// =============================================================================
// SUBMODULE ROUTERS & INPUT HANDLER
// =============================================================================
void initFirmwareModule() {
    currentFwState = FW_STATE_MENU;
}

void enterFirmwareSubmenu() {
    currentFwState = FW_STATE_MENU;
    firmwareMenuIndex = 0;
    displayFirmwareMenu(display, firmwareMenuIndex);
}

void handleFirmwareMenu() {
    static MenuButtonState upState;
    static MenuButtonState downState;

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

    switch (currentFwState) {
        // ---------------------------------------------------------------------
        // 0. SUBMENU ROOT: Flash, System Info, OTA Update, Reboot, Version
        // ---------------------------------------------------------------------
        case FW_STATE_MENU: {
            if (upPress) {
                byte prev = firmwareMenuIndex;
                firmwareMenuIndex = (firmwareMenuIndex + FIRMWARE_MENU_ITEM_COUNT - 1) % FIRMWARE_MENU_ITEM_COUNT;
                displayFirmwareMenu(display, firmwareMenuIndex, prev);
            } else if (downPress) {
                byte prev = firmwareMenuIndex;
                firmwareMenuIndex = (firmwareMenuIndex + 1) % FIRMWARE_MENU_ITEM_COUNT;
                displayFirmwareMenu(display, firmwareMenuIndex, prev);
            } else if (backClick) {
                returnToMainMenu();
            } else if (okClick) {
                if (firmwareMenuIndex == 0) { // Flash (.bin from SD)
                    scanSdForBinFiles();
                    currentFwState = FW_STATE_FLASH_LIST;
                    renderBinFileList(binSelectedIdx);
                } else if (firmwareMenuIndex == 1) { // OTA Update
                    currentFwState = FW_STATE_OTA_NOTICE;
                    renderOtaNoticeScreen();
                } else if (firmwareMenuIndex == 2) { // Version
                    currentFwState = FW_STATE_VERSION;
                    renderVersionScreen();
                }
            }
            break;
        }

        // ---------------------------------------------------------------------
        // 1. FLASH LIST & MODAL CONFIRM
        // ---------------------------------------------------------------------
        case FW_STATE_FLASH_LIST: {
            if (totalBinFiles > 0) {
                if (upPress) {
                    binSelectedIdx = (binSelectedIdx + totalBinFiles - 1) % totalBinFiles;
                    renderBinFileList(binSelectedIdx);
                } else if (downPress) {
                    binSelectedIdx = (binSelectedIdx + 1) % totalBinFiles;
                    renderBinFileList(binSelectedIdx);
                } else if (okClick) {
                    flashConfirmChoice = 0; // Default to Cancel
                    currentFwState = FW_STATE_FLASH_CONFIRM;
                    renderFlashConfirmDialog(binSelectedIdx);
                }
            }
            if (backClick) {
                currentFwState = FW_STATE_MENU;
                displayFirmwareMenu(display, firmwareMenuIndex);
            }
            break;
        }

        case FW_STATE_FLASH_CONFIRM: {
            if (clickLeft || clickRight || upPress || downPress) {
                flashConfirmChoice = (flashConfirmChoice == 0) ? 1 : 0;
                renderFlashConfirmDialog(binSelectedIdx);
            } else if (backClick) {
                currentFwState = FW_STATE_FLASH_LIST;
                renderBinFileList(binSelectedIdx);
            } else if (okClick) {
                if (flashConfirmChoice == 1) { // Confirmed Flash
                    currentFwState = FW_STATE_FLASHING;
                    performFlashFromSD(binFilePaths[binSelectedIdx]);
                    currentFwState = FW_STATE_FLASH_LIST;
                    renderBinFileList(binSelectedIdx);
                } else {
                    currentFwState = FW_STATE_FLASH_LIST;
                    renderBinFileList(binSelectedIdx);
                }
            }
            break;
        }

        case FW_STATE_FLASHING: {
            // Handled synchronously in performFlashFromSD
            break;
        }

        // ---------------------------------------------------------------------
        // 2. OTA NOTICE
        // ---------------------------------------------------------------------
        case FW_STATE_OTA_NOTICE: {
            if (backClick || okClick) {
                currentFwState = FW_STATE_MENU;
                displayFirmwareMenu(display, firmwareMenuIndex);
            }
            break;
        }

        // ---------------------------------------------------------------------
        // 3. VERSION
        // ---------------------------------------------------------------------
        case FW_STATE_VERSION: {
            if (backClick || okClick) {
                currentFwState = FW_STATE_MENU;
                displayFirmwareMenu(display, firmwareMenuIndex);
            }
            break;
        }
    }
}

