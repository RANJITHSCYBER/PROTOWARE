# ⚡ Proto S1 — Cyberdeck & Hardware Multi-Tool

<p align="center">
  <img src="https://readme-typing-svg.demolab.com?font=Fira+Code&weight=600&size=24&duration=3000&pause=1000&color=00FF66&center=true&vCenter=true&width=650&lines=%E2%9A%A1+PROTO+S1+MULTI-TOOL+v2.5;%F0%9F%93%A1+WiFi+%7C+BLE+%7C+MQTT+%7C+GPIO+%7C+OTA;%F0%9F%8E%AE+DSi+%26+Wii+Cyber+Interfaces;%F0%9F%9B%A0%EF%B8%8F+Portable+Hardware+Pentesting" alt="Proto S1 Typing Banner" />
</p>

<p align="center">
  <a href="#-specifications--pinout"><img src="https://img.shields.io/badge/Device-Proto%20S1-blue?style=for-the-badge&logo=espressif&logoColor=white" alt="Proto S1" /></a>
  <a href="#-firmware-version"><img src="https://img.shields.io/badge/Version-v2.5-brightgreen?style=for-the-badge" alt="v2.5" /></a>
  <a href="#-display--interface"><img src="https://img.shields.io/badge/Display-0.96%22%20%2F%201.3%22%20OLED-yellow?style=for-the-badge&logo=adafruit" alt="OLED" /></a>
  <a href="#-web-flasher"><img src="https://img.shields.io/badge/Web%20Flasher-Web%20Serial%20API-orange?style=for-the-badge&logo=google-chrome" alt="Web Flasher" /></a>
  <a href="#-license--credits"><img src="https://img.shields.io/badge/License-Open%20Source-purple?style=for-the-badge" alt="License" /></a>
</p>

---

## 🌟 What is Proto S1?

**Proto S1** is a portable hardware penetration testing, signal synthesis, and sensor diagnostic multi-tool built upon the **ESP32** architecture. Running **OS v2.5**, Proto S1 provides a tactile, multi-theme interface loaded with wireless security tools, BLE human-interface emulation (Mouse & Keyboard), an animated waveform synthesizer, an on-screen graphical GPIO patch-bay, MQTT chat, and PC-free firmware management.

```text
 _______________________________________________________________
|  [Proto S1 v2.5]                           [WiFi: ON] [SD: OK] |
| ┌───────────────────────────────────────────────────────────┐ |
| │  [>] 1. GPIO Waveform Synthesizer & Servos                │ |
| │      2. Wireless Packet Sniffer & Beacon Spammer          │ |
| │      3. Bluetooth Low Energy (BLE HID & Spammer)          │ |
| │      4. MQTT Secure Mesh Communication                    │ |
| │      5. Over-The-Air GitHub Updater & SD Flasher          │ |
| └───────────────────────────────────────────────────────────┘ |
|  [UP / DN] Navigate    [RIGHT] Select    [LEFT/CTR] Back/Exit │
|_______________________________________________________________|
```

---

## 🎮 Navigation & Button Controls

Proto S1 features a **5-button tactile navigation pad** designed for quick, one-handed field operation:

```text
                  ┌───────────────┐
                  │   ▲ UP (26)   │
                  │   Prev Item   │
 ┌────────────────┼───────────────┼────────────────┐
 │   ◄ LEFT (25)  │  ● CENTER(32) │   ► RIGHT(27)  │
 │   Back / Edit  │  Exit / Home  │  Select / Exec │
 └────────────────┼───────────────┼────────────────┘
                  │  ▼ DOWN (33)  │
                  │   Next Item   │
                  └───────────────┘
```

| Button | GPIO Pin | Primary Function | Secondary / Submenu Function |
| :--- | :---: | :--- | :--- |
| **UP** | `GPIO 26` | Navigate Up / Move cursor upwards | Increase servo angle / Adjust frequency |
| **DOWN** | `GPIO 33` | Navigate Down / Move cursor downwards | Decrease servo angle / Adjust frequency |
| **LEFT** | `GPIO 25` | Back / Return to parent submenu | Move cursor left in Pin Selector Grid |
| **RIGHT** | `GPIO 27` | **Select / Enter** submenu or execute | Move cursor right in Pin Selector Grid |
| **CENTER** | `GPIO 32` | **Global Exit** / Return to Main Menu | Long-press: Return from BLE Mouse / Keyboard |

---

## 🔌 Hardware Pinout & Wiring Map

Tested for clean, interference-free pin allocation on standard 30-pin and 38-pin **ESP32 DevKit** boards:

```text
                           ┌───────────────────┐
                    3V3 ───┤ 1              30 ├─── GND
                    EN  ───┤ 2              29 ├─── GPIO 23 (SD MOSI)
        (Debug) GPIO 36 ───┤ 3   ┌──────┐   28 ├─── GPIO 22 (OLED SCL)
        (Debug) GPIO 39 ───┤ 4   │ ESP  │   27 ├─── GPIO 1  (TX0)
        (Debug) GPIO 34 ───┤ 5   │ WROOM│   26 ├─── GPIO 3  (RX0)
        (Debug) GPIO 35 ───┤ 6   │  32  │   25 ├─── GPIO 21 (OLED SDA)
(BTN CENTER)    GPIO 32 ───┤ 7   └──────┘   24 ├─── GPIO 19 (SD MISO)
(BTN DOWN)      GPIO 33 ───┤ 8              23 ├─── GPIO 18 (SD SCK)
(BTN LEFT)      GPIO 25 ───┤ 9              22 ├─── GPIO 5  (RF / SS)
(BTN UP)        GPIO 26 ───┤ 10             21 ├─── GPIO 17 (TX2 / IR)
(BTN RIGHT)     GPIO 27 ───┤ 11             20 ├─── GPIO 16 (RX2)
(Servo / DAC)   GPIO 14 ───┤ 12             19 ├─── GPIO 4  (NeoPixel)
(PWM / Signal)  GPIO 12 ───┤ 13             18 ├─── GPIO 0  (Boot)
(SD Card CS)    GPIO 13 ───┤ 14             17 ├─── GPIO 2  (Status LED)
                    GND ───┤ 15             16 ├─── GPIO 15 (Transceiver)
                           └───────────────────┘
```

### Peripherals Summary

| Hardware Subsystem | Signal | ESP32 GPIO | Description / Notes |
| :--- | :--- | :---: | :--- |
| **OLED Display** | `SDA` | **GPIO 21** | 0.96" SSD1306 or 1.3" SH1106 (`0x3C`) |
| | `SCL` | **GPIO 22** | 400 kHz Fast I2C Bus |
| **MicroSD Card** | `SCK` | **GPIO 18** | High-speed SPI Clock |
| | `MISO` | **GPIO 19** | SPI Master In / Slave Out |
| | `MOSI` | **GPIO 23** | SPI Master Out / Slave In |
| | `CS` | **GPIO 13** | SD Chip Select line |
| **Tactile Buttons** | `UP` | **GPIO 26** | Internal Pull-Up (`Active LOW`) |
| | `DOWN` | **GPIO 33** | Internal Pull-Up (`Active LOW`) |
| | `LEFT` | **GPIO 25** | Internal Pull-Up (`Active LOW`) |
| | `RIGHT` | **GPIO 27** | Internal Pull-Up (`Active LOW`) |
| | `CENTER` | **GPIO 32** | Internal Pull-Up (`Active LOW`) |

---

## 🛠️ Feature Overview (OS v2.5)

### 1. 🎛️ Graphical GPIO Laboratory & Waveform Synthesizer
* **Interactive Matrix**: Dual-row pin selector rendered directly on the OLED.
* **Servo Angle Controller**: Real-time PWM angle adjustment ($0^\circ$ to $180^\circ$) with live pulse-width gauge.
* **Waveform Generators**: Sine Wave, Square Wave, and Triangle Wave generators.
* **Actuator Control**: Motor H-Bridge, Relay Switcher, NeoPixel WS2812B effects, and LED strobe.

### 2. 📡 Wireless Suite: WiFi, BLE & MQTT
* **WiFi Tools**: Scanner with live RSSI meters, SSID Beacon Spammer, and Packet Sniffer.
* **Bluetooth Low Energy (NimBLE)**:
  * **BLE HID Mouse & Keyboard**: Emulates a wireless mouse/keyboard with directional control and long-press exit.
  * **Multi-Platform Notification Spammer**: AirDrop / AirPods (iOS), Google Fast Pair (Android), and Swift Pair (Windows).
* **MQTT Mesh Communication**: Device-to-device chat over MQTT topics with full menu navigation.

### 3. 🚀 Autonomous Dual-OTA & MicroSD Flasher
* **Direct GitHub Cloud Updates**: Connects to WiFi and streams the latest `firmware.bin` directly from GitHub.
* **Offline MicroSD Flasher**: Place any compiled `.bin` file in `/firmware` on your SD card and flash directly in the field.

### 4. 🎨 Multi-Theme Cyber Interfaces
* **4 Switchable UI Engines**:
  * **List Menu**: Cyberdeck terminal layout with icon previews.
  * **Nintendo DSi**: Dual-screen card view with carousel scrolling.
  * **Nintendo Wii**: Grid channel launcher interface.
  * **Pages Menu**: Minimalist high-contrast cards.
* **Cinematic Boot Animation**: 150-frame startup sequence with instant-skip on any button press.

---

## 🌐 Web Flasher Deployment

Proto S1 includes a dedicated **Firmware Deployment Suite** built with the Web Serial API for 1-click browser flashing:

### Flashing via Browser
1. Open the [Proto S1 Web Flasher](web/index.html) in Google Chrome, Microsoft Edge, Brave, or Opera.
2. Connect your Proto S1 via USB-C.
3. Click **Connect Proto S1** and select your serial port (`COM` / `/dev/ttyUSB`).
4. Click **Fetch Latest v2.5 from GitHub** (or select a local `.bin`).
5. Click **Flash Proto S1** — the flasher writes the bootloader, partition table, boot flags, and application automatically.

### Deploying the Web Flasher to Cloudflare Pages (Free)
1. Go to [Cloudflare Dashboard](https://dash.cloudflare.com/) > **Workers & Pages**.
2. Click **Create Application** > **Pages** > **Upload assets**.
3. Name your project `proto-s1` and drag-and-drop the **`web`** folder.
4. Click **Deploy Site** — your public URL is live instantly at:
   ```text
   https://proto-s1.pages.dev
   ```

---

## 💻 Building from Source (PlatformIO)

1. Install [VS Code](https://code.visualstudio.com/) and the [PlatformIO IDE Extension](https://platformio.org/).
2. Clone this repository:
   ```bash
   git clone https://github.com/RANJITHSCYBER/PROTOWARE.git
   ```
3. Open the folder in PlatformIO.
4. Build and upload:
   ```bash
   pio run -t upload
   ```

---

## 📜 License & Credits

* Developed by [RANJITHSCYBER](https://github.com/RANJITHSCYBER).
* Graphics powered by [Adafruit GFX](https://github.com/adafruit/Adafruit-GFX-Library) & [U8g2](https://github.com/olikraus/u8g2).
* Bluetooth stack powered by [NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino).
* Button debouncing by [GyverButton](https://github.com/GyverLibs/GyverButton).
