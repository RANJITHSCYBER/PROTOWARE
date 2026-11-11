# ⚡ PROTOWARE — The Ultimate ESP32 Cyberdeck & Multi-Tool

<p align="center">
  <img src="https://readme-typing-svg.demolab.com?font=Fira+Code&weight=600&size=24&duration=3000&pause=1000&color=00FF66&center=true&vCenter=true&width=650&lines=%E2%9A%A1+PROTOWARE+MULTI-TOOL+v1.0;%F0%9F%93%A1+RF+%7C+WiFi+%7C+BLE+%7C+GPIO+%7C+OTA;%F0%9F%8E%AE+DSi+%26+Wii+Cyber+Interfaces;%F0%9F%9B%A0%EF%B8%8F+Swiss-Army+Hardware+Pentesting" alt="PROTOWARE Typing Banner" />
</p>

<p align="center">
  <a href="#-hardware-pinout--wiring-map"><img src="https://img.shields.io/badge/Board-ESP32%20Dev%20Module-blue?style=for-the-badge&logo=espressif&logoColor=white" alt="ESP32" /></a>
  <a href="#-display--visual-interface"><img src="https://img.shields.io/badge/Display-0.96%22%20OLED%20(128x64)-yellow?style=for-the-badge&logo=adafruit" alt="OLED" /></a>
  <a href="#-wireless-suite-wifi--bluetooth"><img src="https://img.shields.io/badge/Wireless-WiFi%20%7C%20BLE%20%7C%20SubGHz-red?style=for-the-badge&logo=wi-fi" alt="Wireless" /></a>
  <a href="#-autonomous-ota--sd-flashing"><img src="https://img.shields.io/badge/OTA-GitHub%20Auto--Update-brightgreen?style=for-the-badge&logo=github" alt="GitHub OTA" /></a>
  <a href="#-license--credits"><img src="https://img.shields.io/badge/License-Open%20Source-purple?style=for-the-badge" alt="License" /></a>
</p>

---

## 🌟 What is PROTOWARE?

**PROTOWARE** is an all-in-one portable hardware penetration testing, signal synthesis, and hardware debugging device built upon the **ESP32 Dev Module**. Inspired by the cyberdeck aesthetics of the Flipper Zero and ESPHACK interface, PROTOWARE delivers an interactive tactile UI packed with multi-protocol wireless tools, animated waveform generators, an on-screen graphical GPIO patch-bay, and standalone PC-free firmware flashing.

```text
 _______________________________________________________________
|  [PROTOWARE v1.0]                          [WiFi: ON] [SD: OK] |
| ┌───────────────────────────────────────────────────────────┐ |
| │  [>] 1. GPIO Waveform Synthesizer & Servos                │ |
| │      2. Wireless Packet Sniffer & Beacon Spammer          │ |
| │      3. Bluetooth Low Energy (BLE) Multi-Spammer          │ |
| │      4. Sub-GHz Transceiver & Infrared Cloner             │ |
| │      5. Over-The-Air GitHub Updater & SD Flasher          │ |
| └───────────────────────────────────────────────────────────┘ |
|  [UP / DN] Navigate    [RIGHT] Select    [LEFT/CTR] Back/Exit │
|_______________________________________________________________|
```

---

## 🎮 Navigation & Button Controls

PROTOWARE features a **5-button tactile navigation pad** designed for quick, one-handed field operation without needing a touch screen or keyboard:

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
| **LEFT** | `GPIO 25` | Back / Return to parent menu | Move cursor left in Pin Selector Grid |
| **RIGHT** | `GPIO 27` | **Select / Enter** submenu or run tool | Move cursor right in Pin Selector Grid |
| **CENTER** | `GPIO 32` | **Global Exit** / Return to Main Menu | Toggle output ON / OFF |

---

## 🔌 Hardware Pinout & Wiring Map

PROTOWARE has been configured and tested for clean, interference-free pin allocation on standard 30-pin and 38-pin **ESP32 DevKit** boards:

```text
                           ┌───────────────────┐
                    3V3 ───┤ 1              30 ├─── GND
                    EN  ───┤ 2              29 ├─── GPIO 23 (SD MOSI)
         (Debug) GPIO 36 ───┤ 3   ┌──────┐   28 ├─── GPIO 22 (OLED SCL)
         (Debug) GPIO 39 ───┤ 4   │ ESP  │   27 ├─── GPIO 1  (TX0)
         (Debug) GPIO 34 ───┤ 5   │ WROOM│   26 ├─── GPIO 3  (RX0)
         (Debug) GPIO 35 ───┤ 6   │  32  │   25 ├─── GPIO 21 (OLED SDA)
(BTN UP)         GPIO 32 ───┤ 7   └──────┘   24 ├─── GPIO 19 (SD MISO)
(BTN DOWN)       GPIO 33 ───┤ 8              23 ├─── GPIO 18 (SD SCK)
(BTN LEFT)       GPIO 25 ───┤ 9              22 ├─── GPIO 5  (RF / SS)
(BTN RIGHT)      GPIO 26 ───┤ 10             21 ├─── GPIO 17 (TX2 / IR)
(BTN CENTER)     GPIO 27 ───┤ 11             20 ├─── GPIO 16 (RX2)
(Servo / DAC)    GPIO 14 ───┤ 12             19 ├─── GPIO 4  (NeoPixel)
(PWM / Signal)   GPIO 12 ───┤ 13             18 ├─── GPIO 0  (Boot)
(SD Card CS)     GPIO 13 ───┤ 14             17 ├─── GPIO 2  (Status LED)
                    GND ───┤ 15             16 ├─── GPIO 15 (Transceiver)
                           └───────────────────┘
```

### 1. Peripherals Summary Table

| Hardware Subsystem | Signal Name | ESP32 GPIO | Description / Notes |
| :--- | :--- | :---: | :--- |
| **OLED Display** | `SDA` | **GPIO 21** | 0.96" I2C Monochrome Display (Address `0x3C`) |
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
| **GPIO Tools** | User-Selected | `GPIO 2, 4, 12, 14, 19, 23, 25, 26, 27, 32` | Dynamically assigned via on-screen matrix |

> [!IMPORTANT]
> **Input-Only Pins Warning**: GPIO 34, 35, 36 (VP), and 39 (VN) do not have internal software pull-up resistors and cannot be used as digital outputs. PROTOWARE reserves these pins for analog sensors and input-only logic testing.

---

## 🛠️ Detailed Feature Breakdown

### 1. 🎛️ Graphical GPIO Laboratory & Waveform Synthesizer
Never guess which pin is running your actuator. PROTOWARE features an **on-screen interactive dual-row pin selector** rendered directly onto the OLED display:

```text
 ┌───────────────────────────────────────────────────────────┐
 │ +5  GND  32  27  26  25  39  36  35  34  3.3  GND        │
 │ [•] [ ]  [ ] [ ] [ ] [ ] [ ] [ ] [ ] [ ] [ ]  [ ]         │
 │                                                           │
 │ 3.3 SCL  SDA  2   18  19  23  14  12  13 3.3  GND         │
 │ [ ] [ ]  [ ] [ ] [ ] [ ] [ ] [ ] [ ] [ ] [ ]  [ ]         │
 └───────────────────────────────────────────────────────────┘
```

* **Servo Angle Controller**:
  * Real-time PWM angle adjustment ($0^\circ$ to $180^\circ$).
  * Dynamic pulse-width rendering ($500\,\mu\text{s} - 2500\,\mu\text{s}$) with live bar gauge.
  * Instant Pin Re-assignment without restarting the device.
* **Smooth Waveform Generators**:
  * **Sine Wave**: Ultra-smooth animated sine wave with dynamic scrolling on the display.
  * **Square Wave**: Adjustable frequency & duty cycle generator for clock signal emulation.
  * **Triangle Wave**: Linear ramp generator for oscilloscope calibration and filter testing.
* **Actuator Drivers**:
  * **Motor Driver (H-Bridge)**: Forward/Reverse and PWM throttle control.
  * **Relay Switcher**: Clean debounced latching and momentary switching.
  * **NeoPixel Strip (WS2812B)**: Rainbow cycles, chase effects, strobe, and RGB color wheel.
  * **Blink & Flash LED**: Variable strobe and beacon frequency tester.
  * **Button Logic Tester**: Real-time logic level viewer (HIGH/LOW/Float) for input testing.

---

### 2. 📡 Wireless Suite: WiFi & Bluetooth

#### 📶 WiFi Pen-Testing & Diagnostics
* **Network Scanner**: Displays SSID, RSSI (signal strength bar), BSSID, and Encryption type (WPA2/WPA3).
* **Beacon Spammer**: Broadcasts custom/random SSID beacon packets into the air.
* **Packet Sniffer**: Real-time 802.11 packet activity and channel saturation monitoring.
* **Saved WiFi Auto-Manager**: Stores and auto-authenticates with home/lab access points from non-volatile flash (`NVS`).

#### 🔵 Bluetooth Low Energy (NimBLE)
* **BLE Scanner**: Discovers active BLE tags, wearables, smartphones, and beacons with live RSSI meters.
* **Multi-Platform BLE Notification Spammer**:
  * **Apple iOS**: AirDrop, AirPods, Apple TV setup dialog popups.
  * **Android**: Google Fast Pair popups.
  * **Windows**: Swift Pair device detection requests.

---

### 3. 🚀 Autonomous Dual-OTA & MicroSD Flasher

Update your multi-tool without ever plugging a USB cable into a computer:

```text
 ┌───────────────────────────────────────────────────────────┐
 │                   [ OTA FIRMWARE UPDATE ]                 │
 │ Connecting: WiFi Access Point... [Connected]              │
 │ Source: https://github.com/RANJITHSCYBER/PROTOWARE        │
 │                                                           │
 │ Flashing: firmware.bin                                    │
 │ Progress: [=====================>           ] 68%         │
 │ 1056 KB / 1549 KB (Stream Active)                         │
 └───────────────────────────────────────────────────────────┘
```

* **Cloud GitHub OTA Updates**:
  * Automatically connects to saved WiFi credentials.
  * Fetches the latest `firmware.bin` release directly from GitHub:
    `https://github.com/RANJITHSCYBER/PROTOWARE/blob/main/firmware.bin`
  * Automatically resolves GitHub browser URLs into raw binary streams with strict redirect following (`HTTPC_STRICT_FOLLOW_REDIRECTS`).
  * Features live on-screen transfer speed, byte counter, and progress bar with automated post-flash reboot.
* **Offline MicroSD `.bin` Flasher**:
  * Place any `.bin` firmware file on your MicroSD card under `/firmware`.
  * Browse files in real-time, inspect file size, and flash directly to ESP32 partitions in the field!

---

### 4. 🎨 Multi-Theme Cyber Interfaces & 150-Frame Boot Animation

Personalize your device with 4 completely distinct UI engines:

| Theme Name | Style Inspiration | Description |
| :--- | :--- | :--- |
| **List Menu** | Cyberdeck Terminal | Animated sliding selection window with icon previews. |
| **Nintendo DSi** | Retro Handheld | Dual-screen card view with carousel scrolling. |
| **Nintendo Wii** | Channel Launcher | Minimalist grid channel interface with hover effects. |
| **Pages Menu** | Classic Pager | High-contrast full-screen cards for high-speed skimming. |

* **150-Frame OLED Boot Animation**:
  * Plays a 15 FPS startup cinematic sequence on power-up.
  * **Instant-Skip Feature**: Press any button during boot to bypass the animation immediately and jump straight into the menu.

---

## ⚡ Unique Highlights & Benefits

| Feature | Standard ESP32 Firmware | **PROTOWARE** |
| :--- | :---: | :---: |
| **On-Screen GPIO Selector** | ❌ Hardcoded in code | ✅ **Live graphical OLED pin matrix** |
| **Over-The-Air Update** | ❌ Local network only | ✅ **Direct from GitHub via Cloud** |
| **Offline Flashing** | ❌ Requires PC + esptool | ✅ **Direct from MicroSD card** |
| **Interface Themes** | ❌ 1 Plain text menu | ✅ **4 Switchable UI engines (DSi, Wii, List, Pages)** |
| **Startup Screen** | ❌ Static logo | ✅ **150-Frame smooth 15 FPS boot cinematic** |
| **BLE Protocol Engine** | ❌ Basic scanner | ✅ **NimBLE Multi-OS Notification Spammer** |

---

## 📋 Installation & Flashing Guide

### Method A: PlatformIO (Recommended)
1. Install [VS Code](https://code.visualstudio.com/) and the [PlatformIO IDE Extension](https://platformio.org/).
2. Clone this repository:
   ```bash
   git clone https://github.com/RANJITHSCYBER/PROTOWARE.git
   ```
3. Open the folder in PlatformIO.
4. Connect your ESP32 board via USB.
5. Click **Build** and **Upload** (or run `pio run -t upload`).

### Method B: Flash Pre-Compiled `firmware.bin` via Web Flasher
1. Open [ESP Web Flasher](https://espressif.github.io/esptool-js/) in Google Chrome or Microsoft Edge.
2. Select your ESP32 COM port at `115200` baud.
3. Flash `.pio/build/esp32dev/firmware.bin` to address `0x10000`.

---

## 📜 License & Credits

* Developed with ❤️ by [RANJITHSCYBER](https://github.com/RANJITHSCYBER).
* Display & Graphics powered by [Adafruit GFX](https://github.com/adafruit/Adafruit-GFX-Library) & [U8g2](https://github.com/olikraus/u8g2).
* Bluetooth stack powered by [NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino).
* Button debouncing by [GyverButton](https://github.com/GyverLibs/GyverButton).

