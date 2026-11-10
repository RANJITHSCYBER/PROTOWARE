#include "wifi_module.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <SD.h>
#include <GyverButton.h>

#include "CONFIG.h"
#include "pindefs.h"
#include "interface/interface.h"
#include "menu/wifi.h"

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
extern byte wifiMenuIndex;
extern SPIClass sdSPI;
extern bool sdCardReady;

extern void returnToMainMenu();
extern void resetActivityTimer();

// =============================================================================
// DATA TYPES & CONSTANTS
// =============================================================================
enum WiFiSubmenuState : byte {
    WIFI_STATE_MENU = 0,
    WIFI_STATE_SCANNING,
    WIFI_STATE_SCAN_LIST,
    WIFI_STATE_SCAN_DETAILS,
    WIFI_STATE_OTA_PORTAL,
    WIFI_STATE_CONNECT_SCAN,
    WIFI_STATE_CONNECT_LIST,
    WIFI_STATE_KEYBOARD,
    WIFI_STATE_CONNECTING,
    WIFI_STATE_LOGS_LIST,
    WIFI_STATE_LOGS_DETAILS,
    WIFI_STATE_LOGS_CONFIRM_DELETE
};

struct ScannedNetwork {
    String ssid;
    int32_t rssi;
    wifi_auth_mode_t auth;
    uint8_t bssid[6];
    int32_t channel;
};

static const int MAX_SCAN_NETWORKS = 25;
static ScannedNetwork scannedNetworks[MAX_SCAN_NETWORKS];
static int totalScanned = 0;
static int scanSelectedIdx = 0;

static const int MAX_SAVED_WIFI = 12;
struct SavedWiFiRecord {
    String ssid;
    String pass;
};
static SavedWiFiRecord savedNetworks[MAX_SAVED_WIFI];
static int totalSaved = 0;
static int logsSelectedIdx = 0;
static byte logsDetailActionIdx = 0; // 0: Connect, 1: Delete
static byte deleteDialogChoice = 0;  // 0: Cancel, 1: OK

// OTA Web Server & DNS
static WebServer otaServer(80);
static DNSServer otaDNS;
static bool otaPortalActive = false;
static String otaLastUploadMsg = "Ready for upload";
static unsigned long otaLastClientCheck = 0;
static File otaUploadFile;

// Keyboard state for WiFi Connect
static String connectSSID = "";
static String enteredPassword = "";
static byte keyRow = 0;
static byte keyCol = 0;
static bool capsMode = false;
static bool symMode = false;

static WiFiSubmenuState currentState = WIFI_STATE_MENU;

// =============================================================================
// NVS / PREFERENCES HELPER
// =============================================================================
static void loadSavedNetworks() {
    Preferences prefs;
    prefs.begin("pw_wifi", true);
    totalSaved = prefs.getUChar("count", 0);
    if (totalSaved > MAX_SAVED_WIFI) totalSaved = MAX_SAVED_WIFI;

    for (int i = 0; i < totalSaved; i++) {
        String kS = "s" + String(i);
        String kP = "p" + String(i);
        savedNetworks[i].ssid = prefs.getString(kS.c_str(), "");
        savedNetworks[i].pass = prefs.getString(kP.c_str(), "");
    }
    prefs.end();
}

static void saveNetworksToNVS() {
    Preferences prefs;
    prefs.begin("pw_wifi", false);
    prefs.putUChar("count", (uint8_t)totalSaved);
    for (int i = 0; i < totalSaved; i++) {
        String kS = "s" + String(i);
        String kP = "p" + String(i);
        prefs.putString(kS.c_str(), savedNetworks[i].ssid);
        prefs.putString(kP.c_str(), savedNetworks[i].pass);
    }
    prefs.end();
}

static void addOrUpdateSavedNetwork(const String &ssid, const String &pass) {
    for (int i = 0; i < totalSaved; i++) {
        if (savedNetworks[i].ssid == ssid) {
            savedNetworks[i].pass = pass;
            saveNetworksToNVS();
            return;
        }
    }
    if (totalSaved < MAX_SAVED_WIFI) {
        savedNetworks[totalSaved].ssid = ssid;
        savedNetworks[totalSaved].pass = pass;
        totalSaved++;
    } else {
        // Replace oldest
        for (int i = 0; i < MAX_SAVED_WIFI - 1; i++) {
            savedNetworks[i] = savedNetworks[i + 1];
        }
        savedNetworks[MAX_SAVED_WIFI - 1].ssid = ssid;
        savedNetworks[MAX_SAVED_WIFI - 1].pass = pass;
    }
    saveNetworksToNVS();
}

static void deleteSavedNetwork(int index) {
    if (index < 0 || index >= totalSaved) return;
    for (int i = index; i < totalSaved - 1; i++) {
        savedNetworks[i] = savedNetworks[i + 1];
    }
    totalSaved--;
    saveNetworksToNVS();
}

// =============================================================================
// SCANNING HELPERS
// =============================================================================
static const char* getAuthTypeName(wifi_auth_mode_t auth) {
    switch (auth) {
        case WIFI_AUTH_OPEN: return "OPEN";
        case WIFI_AUTH_WEP: return "WEP";
        case WIFI_AUTH_WPA_PSK: return "WPA-PSK";
        case WIFI_AUTH_WPA2_PSK: return "WPA2-PSK";
        case WIFI_AUTH_WPA_WPA2_PSK: return "WPA/WPA2";
        case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2-ENT";
        case WIFI_AUTH_WPA3_PSK: return "WPA3-PSK";
        case WIFI_AUTH_WPA2_WPA3_PSK: return "WPA2/WPA3";
        default: return "ENCRYPTED";
    }
}

static void renderScanNotice(const char* title, const char* subtitle) {
    display.clearDisplay();
    display.drawRoundRect(4, 8, 120, 48, 3, SH110X_WHITE);
    display.setTextColor(SH110X_WHITE);
    display.setTextSize(1);

    int16_t x1, y1;
    uint16_t w, h;

    display.getTextBounds(title, 0, 0, &x1, &y1, &w, &h);
    display.setCursor((128 - w) / 2, 20);
    display.print(title);

    display.getTextBounds(subtitle, 0, 0, &x1, &y1, &w, &h);
    display.setCursor((128 - w) / 2, 36);
    display.print(subtitle);

    display.display();
}

static bool performWiFiScan() {
    renderScanNotice("Scanning WiFi...", "Please wait");
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);

    int n = WiFi.scanNetworks(false, true); // blocking active scan
    totalScanned = 0;

    if (n > 0) {
        if (n > MAX_SCAN_NETWORKS) n = MAX_SCAN_NETWORKS;
        for (int i = 0; i < n; i++) {
            scannedNetworks[i].ssid = WiFi.SSID(i);
            if (scannedNetworks[i].ssid.length() == 0) {
                scannedNetworks[i].ssid = "<Hidden>";
            }
            scannedNetworks[i].rssi = WiFi.RSSI(i);
            scannedNetworks[i].auth = WiFi.encryptionType(i);
            scannedNetworks[i].channel = WiFi.channel(i);
            memcpy(scannedNetworks[i].bssid, WiFi.BSSID(i), 6);
        }
        totalScanned = n;

        // Sort by signal strength (descending RSSI)
        for (int i = 0; i < totalScanned - 1; i++) {
            for (int j = i + 1; j < totalScanned; j++) {
                if (scannedNetworks[j].rssi > scannedNetworks[i].rssi) {
                    ScannedNetwork temp = scannedNetworks[i];
                    scannedNetworks[i] = scannedNetworks[j];
                    scannedNetworks[j] = temp;
                }
            }
        }
        scanSelectedIdx = 0;
        return true;
    }
    return false;
}

// =============================================================================
// UI RENDERERS
// =============================================================================
static void renderScannedList(int selected, bool forConnect = false) {
    display.clearDisplay();
    display.setTextColor(SH110X_WHITE);
    display.setTextSize(1);

    // Header
    display.fillRect(0, 0, 128, 12, SH110X_WHITE);
    display.setTextColor(SH110X_BLACK);
    display.setCursor(2, 2);
    display.print(forConnect ? "Select WiFi to Connect" : "Scanned Networks");

    display.setCursor(102, 2);
    display.printf("%d/%d", selected + 1, totalScanned);

    if (totalScanned == 0) {
        display.setTextColor(SH110X_WHITE);
        display.setCursor(14, 28);
        display.print("No WiFi Found!");
        display.setCursor(14, 44);
        display.print("< Back to Menu");
        display.display();
        return;
    }

    // Scrollable 4 items on screen
    int startIdx = selected - 1;
    if (startIdx < 0) startIdx = 0;
    if (startIdx + 4 > totalScanned) startIdx = max(0, totalScanned - 4);

    for (int r = 0; r < 4; r++) {
        int itemIdx = startIdx + r;
        if (itemIdx >= totalScanned) break;

        int y = 14 + r * 12;
        bool isSel = (itemIdx == selected);

        if (isSel) {
            display.fillRect(0, y, 128, 11, SH110X_WHITE);
            display.setTextColor(SH110X_BLACK);
        } else {
            display.setTextColor(SH110X_WHITE);
        }

        // SSID (up to 14 chars)
        String s = scannedNetworks[itemIdx].ssid;
        if (s.length() > 14) s = s.substring(0, 13) + ".";
        display.setCursor(4, y + 2);
        display.print(s);

        // RSSI & Lock icon
        display.setCursor(92, y + 2);
        display.printf("%ddB", scannedNetworks[itemIdx].rssi);
        if (scannedNetworks[itemIdx].auth != WIFI_AUTH_OPEN) {
            display.drawPixel(124, y + 3, isSel ? SH110X_BLACK : SH110X_WHITE);
            display.drawPixel(124, y + 5, isSel ? SH110X_BLACK : SH110X_WHITE);
        }
    }

    display.display();
}

static void renderScanDetails(int idx) {
    if (idx < 0 || idx >= totalScanned) return;
    ScannedNetwork &net = scannedNetworks[idx];

    display.clearDisplay();
    display.setTextColor(SH110X_WHITE);
    display.setTextSize(1);

    // Title bar
    display.fillRect(0, 0, 128, 11, SH110X_WHITE);
    display.setTextColor(SH110X_BLACK);
    display.setCursor(2, 2);
    display.print("Network Info");

    display.setTextColor(SH110X_WHITE);

    // SSID
    display.setCursor(2, 13);
    display.print("SSID: ");
    String s = net.ssid;
    if (s.length() > 15) s = s.substring(0, 14) + "..";
    display.print(s);

    // Signal strength & bar
    display.setCursor(2, 23);
    int quality = 2 * (net.rssi + 100);
    if (quality < 0) quality = 0;
    if (quality > 100) quality = 100;
    display.printf("RSSI: %d dBm (%d%%)", net.rssi, quality);

    // Auth
    display.setCursor(2, 33);
    display.printf("Auth: %s", getAuthTypeName(net.auth));

    // MAC
    display.setCursor(2, 43);
    display.printf("MAC : %02X:%02X:%02X:%02X:%02X:%02X",
        net.bssid[0], net.bssid[1], net.bssid[2],
        net.bssid[3], net.bssid[4], net.bssid[5]);

    // Channel & Back hint
    display.setCursor(2, 53);
    display.printf("CH  : %d   < Back", net.channel);

    display.display();
}

// =============================================================================
// VIRTUAL KEYBOARD FOR WIFI CONNECT
// =============================================================================
static const char* const kbRow0 = "1234567890";
static const char* const kbRow1 = "qwertyuiop";
static const char* const kbRow2 = "asdfghjkl.";
static const char* const kbRow3 = "zxcvbnm_@ ";

static const char* const kbSym0 = "!@#$%^&*()";
static const char* const kbSym1 = "-_=+[]{}\\|";
static const char* const kbSym2 = ";:'\",<>/??";
static const char* const kbSym3 = "~`#%^&*() ";

static void renderVirtualKeyboard() {
    display.clearDisplay();
    display.setTextColor(SH110X_WHITE);
    display.setTextSize(1);

    // Header: SSID
    String title = "WiFi: " + connectSSID;
    if (title.length() > 21) title = title.substring(0, 20) + ".";
    display.setCursor(2, 1);
    display.print(title);

    // Password input box
    display.drawRoundRect(0, 10, 128, 12, 2, SH110X_WHITE);
    display.setCursor(3, 12);
    String shownPass = enteredPassword;
    if (shownPass.length() > 18) {
        shownPass = shownPass.substring(shownPass.length() - 18);
    }
    display.print(shownPass);
    display.print("_");

    // Keyboard Rows
    const char* r0 = symMode ? kbSym0 : kbRow0;
    const char* r1 = symMode ? kbSym1 : kbRow1;
    const char* r2 = symMode ? kbSym2 : kbRow2;
    const char* r3 = symMode ? kbSym3 : kbRow3;

    const char* rows[4] = {r0, r1, r2, r3};

    for (int r = 0; r < 4; r++) {
        int y = 23 + r * 8;
        for (int c = 0; c < 10; c++) {
            char ch = rows[r][c];
            if (capsMode && !symMode && ch >= 'a' && ch <= 'z') {
                ch = ch - 'a' + 'A';
            }
            int x = 2 + c * 12;
            bool isSelected = (keyRow == r && keyCol == c);

            if (isSelected) {
                display.fillRect(x - 1, y - 1, 11, 8, SH110X_WHITE);
                display.setTextColor(SH110X_BLACK);
            } else {
                display.setTextColor(SH110X_WHITE);
            }
            display.setCursor(x + 1, y);
            display.print(ch);
        }
    }

    // Row 4: Controls [CAP] [SYM] [SPC] [DEL] [OK] [ESC]
    const char* ctrlLabels[6] = {
        capsMode ? "cap" : "CAP",
        symMode ? "ABC" : "SYM",
        "SPC",
        "DEL",
        "OK",
        "ESC"
    };
    int ctrlX[6] = {2, 24, 46, 68, 90, 110};
    int ctrlW[6] = {20, 20, 20, 20, 18, 17};

    int yCtrl = 55;
    for (int c = 0; c < 6; c++) {
        bool isSelected = (keyRow == 4 && keyCol == c);
        if (isSelected) {
            display.fillRect(ctrlX[c], yCtrl, ctrlW[c], 8, SH110X_WHITE);
            display.setTextColor(SH110X_BLACK);
        } else {
            display.drawRect(ctrlX[c], yCtrl, ctrlW[c], 8, SH110X_WHITE);
            display.setTextColor(SH110X_WHITE);
        }
        display.setCursor(ctrlX[c] + 2, yCtrl);
        display.print(ctrlLabels[c]);
    }

    display.display();
}

static void handleKeyboardClick() {
    if (keyRow < 4) {
        const char* r = (keyRow == 0) ? (symMode ? kbSym0 : kbRow0) :
                        (keyRow == 1) ? (symMode ? kbSym1 : kbRow1) :
                        (keyRow == 2) ? (symMode ? kbSym2 : kbRow2) :
                                        (symMode ? kbSym3 : kbRow3);
        char ch = r[keyCol];
        if (capsMode && !symMode && ch >= 'a' && ch <= 'z') {
            ch = ch - 'a' + 'A';
        }
        if (enteredPassword.length() < 63) {
            enteredPassword += ch;
        }
    } else {
        // Controls Row 4
        switch (keyCol) {
            case 0: capsMode = !capsMode; break;
            case 1: symMode = !symMode; break;
            case 2:
                if (enteredPassword.length() < 63) enteredPassword += ' ';
                break;
            case 3:
                if (enteredPassword.length() > 0) {
                    enteredPassword.remove(enteredPassword.length() - 1);
                }
                break;
            case 4: // OK / Submit
                currentState = WIFI_STATE_CONNECTING;
                break;
            case 5: // ESC / Cancel
                currentState = WIFI_STATE_CONNECT_LIST;
                renderScannedList(scanSelectedIdx, true);
                break;
        }
    }
}

// =============================================================================
// LOGS UI & COMPUTER-STYLE MODAL DIALOG
// =============================================================================
static void renderLogsList(int selected) {
    display.clearDisplay();
    display.setTextColor(SH110X_WHITE);
    display.setTextSize(1);

    // Title
    display.fillRect(0, 0, 128, 12, SH110X_WHITE);
    display.setTextColor(SH110X_BLACK);
    display.setCursor(2, 2);
    display.print("Saved WiFi Networks");
    display.setCursor(102, 2);
    display.printf("%d/%d", totalSaved > 0 ? selected + 1 : 0, totalSaved);

    if (totalSaved == 0) {
        display.setTextColor(SH110X_WHITE);
        display.setCursor(12, 24);
        display.print("No Saved Networks!");
        display.setCursor(12, 38);
        display.print("Use 'WiFi Connect'");
        display.setCursor(12, 50);
        display.print("< Back");
        display.display();
        return;
    }

    int startIdx = selected - 1;
    if (startIdx < 0) startIdx = 0;
    if (startIdx + 4 > totalSaved) startIdx = max(0, totalSaved - 4);

    for (int r = 0; r < 4; r++) {
        int itemIdx = startIdx + r;
        if (itemIdx >= totalSaved) break;

        int y = 14 + r * 12;
        bool isSel = (itemIdx == selected);

        if (isSel) {
            display.fillRect(0, y, 128, 11, SH110X_WHITE);
            display.setTextColor(SH110X_BLACK);
        } else {
            display.setTextColor(SH110X_WHITE);
        }

        String s = String(itemIdx + 1) + ". " + savedNetworks[itemIdx].ssid;
        if (s.length() > 19) s = s.substring(0, 18) + ".";
        display.setCursor(4, y + 2);
        display.print(s);
    }
    display.display();
}

static void renderLogsDetails(int idx) {
    if (idx < 0 || idx >= totalSaved) return;

    display.clearDisplay();
    display.setTextColor(SH110X_WHITE);
    display.setTextSize(1);

    // Window Header
    display.fillRect(0, 0, 128, 11, SH110X_WHITE);
    display.setTextColor(SH110X_BLACK);
    display.setCursor(2, 2);
    display.print("WiFi Credentials");

    display.setTextColor(SH110X_WHITE);
    display.setCursor(4, 15);
    display.print("SSID: ");
    String s = savedNetworks[idx].ssid;
    if (s.length() > 14) s = s.substring(0, 13) + "..";
    display.print(s);

    display.setCursor(4, 27);
    display.print("Pass: ");
    String p = savedNetworks[idx].pass;
    if (p.length() == 0) p = "<Open/None>";
    if (p.length() > 14) p = p.substring(0, 13) + "..";
    display.print(p);

    // Actions buttons at bottom: [Connect]  [Delete]
    int yBtn = 46;

    // Connect Button
    if (logsDetailActionIdx == 0) {
        display.fillRect(6, yBtn, 54, 14, SH110X_WHITE);
        display.setTextColor(SH110X_BLACK);
    } else {
        display.drawRoundRect(6, yBtn, 54, 14, 2, SH110X_WHITE);
        display.setTextColor(SH110X_WHITE);
    }
    display.setCursor(10, yBtn + 3);
    display.print("Connect");

    // Delete Button
    if (logsDetailActionIdx == 1) {
        display.fillRect(68, yBtn, 54, 14, SH110X_WHITE);
        display.setTextColor(SH110X_BLACK);
    } else {
        display.drawRoundRect(68, yBtn, 54, 14, 2, SH110X_WHITE);
        display.setTextColor(SH110X_WHITE);
    }
    display.setCursor(76, yBtn + 3);
    display.print("Delete");

    display.display();
}

static void renderComputerStyleDeleteModal() {
    display.clearDisplay();

    // Backdrop shadow box
    display.drawRoundRect(10, 8, 108, 48, 4, SH110X_WHITE);
    display.fillRect(11, 9, 106, 12, SH110X_WHITE);

    // Title bar like Windows / Mac dialog
    display.setTextColor(SH110X_BLACK);
    display.setTextSize(1);
    display.setCursor(20, 11);
    display.print("CONFIRM DELETE");

    // Question
    display.setTextColor(SH110X_WHITE);
    display.setCursor(16, 26);
    display.print("Delete this WiFi?");

    // Buttons: [ Cancel ] [ OK ]
    int yBtn = 38;

    // Cancel Button
    if (deleteDialogChoice == 0) {
        display.fillRect(18, yBtn, 42, 14, SH110X_WHITE);
        display.setTextColor(SH110X_BLACK);
    } else {
        display.drawRoundRect(18, yBtn, 42, 14, 2, SH110X_WHITE);
        display.setTextColor(SH110X_WHITE);
    }
    display.setCursor(22, yBtn + 3);
    display.print("Cancel");

    // OK Button
    if (deleteDialogChoice == 1) {
        display.fillRect(68, yBtn, 40, 14, SH110X_WHITE);
        display.setTextColor(SH110X_BLACK);
    } else {
        display.drawRoundRect(68, yBtn, 40, 14, 2, SH110X_WHITE);
        display.setTextColor(SH110X_WHITE);
    }
    display.setCursor(80, yBtn + 3);
    display.print("OK");

    display.display();
}

// =============================================================================
// OTA UPLOAD WEB PORTAL
// =============================================================================
static String formatBytes(size_t bytes) {
    if (bytes < 1024) return String(bytes) + " B";
    if (bytes < 1048576) return String(bytes / 1024.0, 1) + " KB";
    return String(bytes / 1048576.0, 1) + " MB";
}

static void handleOTAPortalRoot() {
    uint64_t totalBytes = SD.totalBytes();
    uint64_t usedBytes = SD.usedBytes();
    uint64_t freeBytes = totalBytes - usedBytes;

    String html = F("<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>"
                    "<title>PROTOWARE OTA</title><style>"
                    "body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;background:#121214;color:#e0e0e0;margin:0;padding:15px;text-align:center}"
                    "h1{color:#00e5ff;font-size:22px;margin:10px 0 4px}"
                    "p{color:#888;font-size:13px;margin-bottom:15px}"
                    ".card{background:#1e1e24;border-radius:12px;padding:16px;margin:12px auto;max-width:440px;box-shadow:0 6px 16px rgba(0,0,0,0.4);border:1px solid #2a2a35}"
                    "input[type=file]{margin:12px 0;color:#ccc;font-size:13px}"
                    ".btn{background:#00e5ff;color:#050508;border:none;padding:10px 22px;border-radius:8px;font-weight:700;font-size:14px;cursor:pointer;transition:0.2s}"
                    ".btn:hover{background:#33ebff}"
                    ".btn-del{background:#ff3366;color:#fff;padding:4px 8px;border-radius:4px;text-decoration:none;font-size:11px;font-weight:600}"
                    "table{width:100%;border-collapse:collapse;margin-top:12px;font-size:13px;text-align:left}"
                    "th,td{padding:8px 6px;border-bottom:1px solid #282834}"
                    "th{color:#00e5ff;font-size:12px;text-transform:uppercase}"
                    ".bar{background:#2a2a38;border-radius:6px;height:10px;overflow:hidden;margin:10px 0}"
                    ".fill{background:#00e5ff;height:100%}"
                    "</style></head><body>"
                    "<h1>PROTOWARE OTA</h1><p>SD Card Wireless Portal</p>"
                    "<div class='card'>"
                    "<div style='display:flex;justify-content:space-between;font-size:12px;color:#aaa;margin-bottom:4px'>"
                    "<span>SD Storage</span><span>");

    html += formatBytes(usedBytes) + " / " + formatBytes(totalBytes) + "</span></div>";
    float pct = (totalBytes > 0) ? ((float)usedBytes / (float)totalBytes) * 100.0f : 0.0f;
    html += "<div class='bar'><div class='fill' style='width:" + String(pct, 1) + "%'></div></div>";
    html += "<span style='font-size:12px;color:#888'>Free: " + formatBytes(freeBytes) + "</span></div>";

    html += F("<div class='card'><h3>Upload File to SD</h3>"
              "<form method='POST' action='/upload' enctype='multipart/form-data'>"
              "<input type='file' name='file'><br>"
              "<input type='submit' value='Upload to SD Card' class='btn'>"
              "</form></div>"
              "<div class='card'><h3>Files on SD Card</h3><table>"
              "<tr><th>Name</th><th>Size</th><th>Action</th></tr>");

    File root = SD.open("/");
    if (root) {
        File file = root.openNextFile();
        int count = 0;
        while (file && count < 40) {
            if (!file.isDirectory()) {
                count++;
                String fname = String(file.name());
                if (fname.startsWith("/")) fname = fname.substring(1);
                html += "<tr><td>" + fname + "</td><td>" + formatBytes(file.size()) + "</td>";
                html += "<td><a class='btn-del' href='/delete?f=" + fname + "' onclick='return confirm(\"Delete " + fname + "?\")'>Delete</a></td></tr>";
            }
            file = root.openNextFile();
        }
        root.close();
        if (count == 0) {
            html += "<tr><td colspan='3' style='text-align:center;color:#666'>No files on SD Card</td></tr>";
        }
    }

    html += F("</table></div><p style='color:#555;font-size:11px'>PROTOWARE Multi-Tool ESP32</p></body></html>");
    otaServer.send(200, "text/html", html);
}

static void handleOTAUpload() {
    HTTPUpload& upload = otaServer.upload();
    if (upload.status == UPLOAD_FILE_START) {
        String filename = upload.filename;
        if (!filename.startsWith("/")) filename = "/" + filename;
        if (SD.exists(filename)) SD.remove(filename);
        otaUploadFile = SD.open(filename, FILE_WRITE);
        otaLastUploadMsg = "Saving: " + upload.filename;
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (otaUploadFile) {
            otaUploadFile.write(upload.buf, upload.currentSize);
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (otaUploadFile) {
            otaUploadFile.close();
        }
        otaLastUploadMsg = "Done: " + upload.filename;
    }
}

static void handleOTADelete() {
    if (otaServer.hasArg("f")) {
        String path = "/" + otaServer.arg("f");
        if (SD.exists(path)) {
            SD.remove(path);
            otaLastUploadMsg = "Deleted: " + otaServer.arg("f");
        }
    }
    otaServer.sendHeader("Location", "/");
    otaServer.send(303);
}

static void startOTAPortal() {
    renderScanNotice("Starting OTA AP...", "Checking SD Card");

    if (!sdCardReady) {
        pinMode(SD_CS, OUTPUT);
        digitalWrite(SD_CS, HIGH);
        sdSPI.begin(SPI_SCK_PIN, SPI_MISO_PIN, SPI_MOSI_PIN, SD_CS);
        sdCardReady = SD.begin(SD_CS, sdSPI);
    }

    if (!sdCardReady) {
        display.clearDisplay();
        display.setTextColor(SH110X_WHITE);
        display.drawRoundRect(4, 8, 120, 48, 3, SH110X_WHITE);
        display.setCursor(16, 20);
        display.print("SD Card Missing!");
        display.setCursor(16, 36);
        display.print("Insert card & retry");
        display.display();
        delay(1500);
        currentState = WIFI_STATE_MENU;
        displayWiFiMenu(display, wifiMenuIndex);
        return;
    }

    WiFi.mode(WIFI_AP);
    WiFi.softAP("PROTOWARE-OTA");
    IPAddress apIP = WiFi.softAPIP();

    otaDNS.start(53, "*", apIP);

    otaServer.on("/", HTTP_GET, handleOTAPortalRoot);
    otaServer.on("/upload", HTTP_POST, []() {
        otaServer.sendHeader("Location", "/");
        otaServer.send(303);
    }, handleOTAUpload);
    otaServer.on("/delete", HTTP_GET, handleOTADelete);

    // Captive portal detection redirects
    otaServer.on("/generate_204", []() {
        otaServer.sendHeader("Location", "/");
        otaServer.send(302, "text/plain", "");
    });
    otaServer.on("/hotspot-detect.html", []() {
        otaServer.sendHeader("Location", "/");
        otaServer.send(302, "text/plain", "");
    });
    otaServer.onNotFound([]() {
        otaServer.sendHeader("Location", "/");
        otaServer.send(302, "text/plain", "");
    });

    otaServer.begin();
    otaPortalActive = true;
    otaLastUploadMsg = "Ready for upload";
    currentState = WIFI_STATE_OTA_PORTAL;
}

static void renderOTAPortalScreen() {
    display.clearDisplay();
    display.setTextColor(SH110X_WHITE);
    display.setTextSize(1);

    // Header banner
    display.fillRect(0, 0, 128, 11, SH110X_WHITE);
    display.setTextColor(SH110X_BLACK);
    display.setCursor(4, 2);
    display.print("OTA SD WEB PORTAL");

    display.setTextColor(SH110X_WHITE);

    display.setCursor(2, 14);
    display.print("SSID: PROTOWARE-OTA");

    display.setCursor(2, 25);
    display.print("IP  : 192.168.4.1");

    display.setCursor(2, 36);
    int clients = WiFi.softAPgetStationNum();
    display.printf("Clients: %d connected", clients);

    display.setCursor(2, 46);
    String msg = otaLastUploadMsg;
    if (msg.length() > 20) msg = msg.substring(0, 19) + ".";
    display.print(msg);

    display.setCursor(2, 56);
    display.print("< Back to Stop & Exit");

    display.display();
}

static void stopOTAPortal() {
    if (otaPortalActive) {
        otaServer.stop();
        otaDNS.stop();
        WiFi.softAPdisconnect(true);
        WiFi.mode(WIFI_OFF);
        otaPortalActive = false;
    }
}

// =============================================================================
// PUBLIC INTERFACE
// =============================================================================
void initWiFiModule() {
    loadSavedNetworks();
}

void enterWiFiSubmenu() {
    currentState = WIFI_STATE_MENU;
    wifiMenuIndex = 0;
    displayWiFiMenu(display, wifiMenuIndex);
}

void handleWiFiMenu() {
    static MenuButtonState upState;
    static MenuButtonState downState;

    unsigned long repeatDelay = getMenuSubmenuRepeatDelay(submenu == 1);
    bool upPress = isMenuButtonPress(BUTTON_PIN_UP, upState, repeatDelay);
    bool downPress = isMenuButtonPress(BUTTON_PIN_DOWN, downState, repeatDelay);

    // Read button click states ONCE per loop to avoid consuming GyverButton flags
    bool clickLeft = buttonBack.isClick();
    bool clickRight = buttonOK.isClick();
    bool clickCenter = buttonCenter.isClick();

    // Standard menu actions
    bool okClick = clickRight || clickCenter;
    bool backClick = clickLeft;

    if (upPress || downPress || okClick || backClick) {
        resetActivityTimer();
    }

    switch (currentState) {
        // ---------------------------------------------------------------------
        // 0. SUBMENU ROOT: Scan WiFi, OTA Upload, WiFi Connect, Logs
        // ---------------------------------------------------------------------
        case WIFI_STATE_MENU: {
            if (upPress) {
                byte prev = wifiMenuIndex;
                wifiMenuIndex = (wifiMenuIndex + WIFI_MENU_ITEM_COUNT - 1) % WIFI_MENU_ITEM_COUNT;
                displayWiFiMenu(display, wifiMenuIndex, prev);
            } else if (downPress) {
                byte prev = wifiMenuIndex;
                wifiMenuIndex = (wifiMenuIndex + 1) % WIFI_MENU_ITEM_COUNT;
                displayWiFiMenu(display, wifiMenuIndex, prev);
            } else if (backClick) {
                returnToMainMenu();
            } else if (okClick) {
                if (wifiMenuIndex == 0) { // Scan WiFi
                    if (performWiFiScan()) {
                        currentState = WIFI_STATE_SCAN_LIST;
                        renderScannedList(scanSelectedIdx, false);
                    } else {
                        currentState = WIFI_STATE_SCAN_LIST;
                        renderScannedList(0, false);
                    }
                } else if (wifiMenuIndex == 1) { // OTA Upload
                    startOTAPortal();
                    renderOTAPortalScreen();
                } else if (wifiMenuIndex == 2) { // WiFi Connect
                    if (performWiFiScan()) {
                        currentState = WIFI_STATE_CONNECT_LIST;
                        renderScannedList(scanSelectedIdx, true);
                    } else {
                        currentState = WIFI_STATE_CONNECT_LIST;
                        renderScannedList(0, true);
                    }
                } else if (wifiMenuIndex == 3) { // Logs
                    loadSavedNetworks();
                    logsSelectedIdx = 0;
                    currentState = WIFI_STATE_LOGS_LIST;
                    renderLogsList(logsSelectedIdx);
                }
            }
            break;
        }

        // ---------------------------------------------------------------------
        // 1. SCAN WIFI: List & Details
        // ---------------------------------------------------------------------
        case WIFI_STATE_SCAN_LIST: {
            if (totalScanned > 0) {
                if (upPress) {
                    scanSelectedIdx = (scanSelectedIdx + totalScanned - 1) % totalScanned;
                    renderScannedList(scanSelectedIdx, false);
                } else if (downPress) {
                    scanSelectedIdx = (scanSelectedIdx + 1) % totalScanned;
                    renderScannedList(scanSelectedIdx, false);
                } else if (okClick) {
                    currentState = WIFI_STATE_SCAN_DETAILS;
                    renderScanDetails(scanSelectedIdx);
                }
            }
            if (backClick) {
                currentState = WIFI_STATE_MENU;
                displayWiFiMenu(display, wifiMenuIndex);
            }
            break;
        }

        case WIFI_STATE_SCAN_DETAILS: {
            if (backClick || okClick) {
                currentState = WIFI_STATE_SCAN_LIST;
                renderScannedList(scanSelectedIdx, false);
            }
            break;
        }

        // ---------------------------------------------------------------------
        // 2. OTA UPLOAD: SoftAP Web File Server
        // ---------------------------------------------------------------------
        case WIFI_STATE_OTA_PORTAL: {
            otaDNS.processNextRequest();
            otaServer.handleClient();

            if (millis() - otaLastClientCheck >= 1000) {
                otaLastClientCheck = millis();
                renderOTAPortalScreen();
            }

            if (backClick || clickCenter) {
                stopOTAPortal();
                renderScanNotice("Stopping OTA AP...", "Exiting portal");
                delay(500);
                currentState = WIFI_STATE_MENU;
                displayWiFiMenu(display, wifiMenuIndex);
            }
            break;
        }

        // ---------------------------------------------------------------------
        // 3. WIFI CONNECT: Select Network & Virtual Keyboard
        // ---------------------------------------------------------------------
        case WIFI_STATE_CONNECT_LIST: {
            if (totalScanned > 0) {
                if (upPress) {
                    scanSelectedIdx = (scanSelectedIdx + totalScanned - 1) % totalScanned;
                    renderScannedList(scanSelectedIdx, true);
                } else if (downPress) {
                    scanSelectedIdx = (scanSelectedIdx + 1) % totalScanned;
                    renderScannedList(scanSelectedIdx, true);
                } else if (okClick) {
                    connectSSID = scannedNetworks[scanSelectedIdx].ssid;
                    enteredPassword = "";

                    // Check if already in saved logs to pre-fill
                    for (int i = 0; i < totalSaved; i++) {
                        if (savedNetworks[i].ssid == connectSSID) {
                            enteredPassword = savedNetworks[i].pass;
                            break;
                        }
                    }

                    if (scannedNetworks[scanSelectedIdx].auth == WIFI_AUTH_OPEN) {
                        currentState = WIFI_STATE_CONNECTING;
                    } else {
                        keyRow = 0;
                        keyCol = 0;
                        capsMode = false;
                        symMode = false;
                        currentState = WIFI_STATE_KEYBOARD;
                        renderVirtualKeyboard();
                    }
                }
            }
            if (backClick) {
                currentState = WIFI_STATE_MENU;
                displayWiFiMenu(display, wifiMenuIndex);
            }
            break;
        }

        case WIFI_STATE_KEYBOARD: {
            static MenuButtonState kbUpState;
            static MenuButtonState kbDownState;
            static MenuButtonState kbLeftState;
            static MenuButtonState kbRightState;
            static MenuButtonState kbCenterState;

            bool kbUp = isMenuButtonPress(BUTTON_PIN_UP, kbUpState, 160);
            bool kbDown = isMenuButtonPress(BUTTON_PIN_DOWN, kbDownState, 160);
            bool kbLeft = isMenuButtonPress(BUTTON_PIN_LEFT, kbLeftState, 160);
            bool kbRight = isMenuButtonPress(BUTTON_PIN_RIGHT, kbRightState, 160);
            bool kbSelect = isMenuButtonPress(BUTTON_PIN_CENTER, kbCenterState, 0); // Single-shot: strictly 1 character per press

            if (kbUp || kbDown || kbLeft || kbRight || kbSelect) {
                resetActivityTimer();
            }

            bool moved = false;
            if (kbUp) {
                keyRow = (keyRow + 4) % 5;
                if (keyRow == 4 && keyCol > 5) keyCol = 5;
                moved = true;
            } else if (kbDown) {
                keyRow = (keyRow + 1) % 5;
                if (keyRow == 4 && keyCol > 5) keyCol = 5;
                moved = true;
            } else if (kbLeft) {
                int maxCols = (keyRow == 4) ? 6 : 10;
                keyCol = (keyCol + maxCols - 1) % maxCols;
                moved = true;
            } else if (kbRight) {
                int maxCols = (keyRow == 4) ? 6 : 10;
                keyCol = (keyCol + 1) % maxCols;
                moved = true;
            }

            if (kbSelect) {
                handleKeyboardClick();
                moved = true;
            }

            if (currentState == WIFI_STATE_KEYBOARD && moved) {
                renderVirtualKeyboard();
            }
            break;
        }

        case WIFI_STATE_CONNECTING: {
            renderScanNotice("Connecting to:", connectSSID.c_str());

            WiFi.mode(WIFI_STA);
            WiFi.disconnect();
            delay(100);

            if (enteredPassword.length() > 0) {
                WiFi.begin(connectSSID.c_str(), enteredPassword.c_str());
            } else {
                WiFi.begin(connectSSID.c_str());
            }

            unsigned long startAttempt = millis();
            bool connected = false;

            while (millis() - startAttempt < 12000) {
                if (WiFi.status() == WL_CONNECTED) {
                    connected = true;
                    break;
                }
                delay(200);
            }

            display.clearDisplay();
            display.setTextColor(SH110X_WHITE);
            display.drawRoundRect(4, 6, 120, 52, 3, SH110X_WHITE);

            if (connected) {
                addOrUpdateSavedNetwork(connectSSID, enteredPassword);
                display.setCursor(14, 14);
                display.print("CONNECTED!");
                display.setCursor(14, 28);
                display.print("IP: ");
                display.print(WiFi.localIP());
                display.setCursor(14, 42);
                display.print("Saved to Logs!");
                display.display();
                delay(2000);
            } else {
                display.setCursor(14, 16);
                display.print("Connection Failed!");
                display.setCursor(14, 32);
                display.print("Check password or");
                display.setCursor(14, 44);
                display.print("network distance.");
                display.display();
                delay(2000);
            }

            currentState = WIFI_STATE_MENU;
            displayWiFiMenu(display, wifiMenuIndex);
            break;
        }

        // ---------------------------------------------------------------------
        // 4. LOGS: Saved Networks, Password Reveal, Connect & Delete Modal
        // ---------------------------------------------------------------------
        case WIFI_STATE_LOGS_LIST: {
            if (totalSaved > 0) {
                if (upPress) {
                    logsSelectedIdx = (logsSelectedIdx + totalSaved - 1) % totalSaved;
                    renderLogsList(logsSelectedIdx);
                } else if (downPress) {
                    logsSelectedIdx = (logsSelectedIdx + 1) % totalSaved;
                    renderLogsList(logsSelectedIdx);
                } else if (okClick) {
                    logsDetailActionIdx = 0; // Default to Connect
                    currentState = WIFI_STATE_LOGS_DETAILS;
                    renderLogsDetails(logsSelectedIdx);
                }
            }
            if (backClick) {
                currentState = WIFI_STATE_MENU;
                displayWiFiMenu(display, wifiMenuIndex);
            }
            break;
        }

        case WIFI_STATE_LOGS_DETAILS: {
            if (clickLeft || clickRight) {
                logsDetailActionIdx = (logsDetailActionIdx == 0) ? 1 : 0;
                renderLogsDetails(logsSelectedIdx);
            } else if (backClick) {
                currentState = WIFI_STATE_LOGS_LIST;
                renderLogsList(logsSelectedIdx);
            } else if (okClick) {
                if (logsDetailActionIdx == 0) { // Connect
                    connectSSID = savedNetworks[logsSelectedIdx].ssid;
                    enteredPassword = savedNetworks[logsSelectedIdx].pass;
                    currentState = WIFI_STATE_CONNECTING;
                } else if (logsDetailActionIdx == 1) { // Delete
                    deleteDialogChoice = 0; // Default to Cancel
                    currentState = WIFI_STATE_LOGS_CONFIRM_DELETE;
                    renderComputerStyleDeleteModal();
                }
            }
            break;
        }

        case WIFI_STATE_LOGS_CONFIRM_DELETE: {
            if (clickLeft || clickRight || upPress || downPress) {
                deleteDialogChoice = (deleteDialogChoice == 0) ? 1 : 0;
                renderComputerStyleDeleteModal();
            } else if (backClick) {
                currentState = WIFI_STATE_LOGS_DETAILS;
                renderLogsDetails(logsSelectedIdx);
            } else if (okClick) {
                if (deleteDialogChoice == 1) { // OK confirmed delete
                    deleteSavedNetwork(logsSelectedIdx);
                    if (logsSelectedIdx >= totalSaved && totalSaved > 0) {
                        logsSelectedIdx = totalSaved - 1;
                    }
                    renderScanNotice("Network Deleted", "Removed from Logs");
                    delay(800);
                    currentState = WIFI_STATE_LOGS_LIST;
                    renderLogsList(logsSelectedIdx);
                } else { // Cancel
                    currentState = WIFI_STATE_LOGS_DETAILS;
                    renderLogsDetails(logsSelectedIdx);
                }
            }
            break;
        }
    }
}

