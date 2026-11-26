# ⚡ Proto S1 — Cyberdeck & Hardware Multi-Tool

<p align="center">
  <img src="https://readme-typing-svg.demolab.com?font=Fira+Code&weight=600&size=24&duration=3000&pause=1000&color=0071E3&center=true&vCenter=true&width=700&lines=%E2%9A%A1+PROTO+S1+MULTI-TOOL+v2.5;%F0%9F%94%97+Official+Site%3A+https%3A%2F%2Fs1.protox.in%2F;%F0%9F%93%A1+WiFi+%7C+BLE+%7C+MQTT+%7C+Sensor+Lab+%7C+GPIO;%F0%9F%8E%AE+4+Switchable+UI+Engines+(DSi+%26+Wii);%F0%9F%9B%A0%EF%B8%8F+Dual-OTA+Fail-Safe+Firmware+Architecture" alt="Proto S1 Typing Banner" />
</p>

<p align="center">
  <a href="https://s1.protox.in/"><img src="https://img.shields.io/badge/Official%20Device-s1.protox.in-0071e3?style=for-the-badge&logo=google-chrome&logoColor=white" alt="Official Website" /></a>
  <a href="#-specifications--architecture"><img src="https://img.shields.io/badge/Platform-ESP32%20Dual--Core-red?style=for-the-badge&logo=espressif&logoColor=white" alt="ESP32" /></a>
  <a href="#-firmware-version"><img src="https://img.shields.io/badge/Firmware-Protoware%20v2.5-10b981?style=for-the-badge" alt="v2.5" /></a>
  <a href="#-display--interface"><img src="https://img.shields.io/badge/Display-1.3%22%20%2F%200.96%22%20OLED-f59e0b?style=for-the-badge" alt="OLED" /></a>
  <a href="#-web-serial-flasher"><img src="https://img.shields.io/badge/Web%20Flasher-Web%20Serial%20API-8b5cf6?style=for-the-badge" alt="Web Flasher" /></a>
  <a href="https://wa.me/917338006528?text=Enquiry%20for%20the%20Proto%20S1%20hardware"><img src="https://img.shields.io/badge/Support-WhatsApp%20(+91%207338006528)-25d366?style=for-the-badge&logo=whatsapp&logoColor=white" alt="WhatsApp Support" /></a>
</p>

---

## 🌐 Official Links & Resources

| Resource | URL |
| :--- | :--- |
| **Official Device Portal** | [https://s1.protox.in/](https://s1.protox.in/) |
| **Web Serial Flasher** | Direct 1-click in-browser deployment via [Web Flasher](web/index.html) |
| **Source Repository** | [https://github.com/RANJITHSCYBER/PROTOWARE](https://github.com/RANJITHSCYBER/PROTOWARE) |
| **Hardware & Sales Enquiry** | [WhatsApp (+91 7338006528)](https://wa.me/917338006528?text=Enquiry%20for%20the%20Proto%20S1%20hardware) |
| **Company** | **ProtoX Technologies** |

---

## 🌟 What is Proto S1?

**Proto S1** is an all-in-one portable cyberdeck, wireless security analyzer, sensor laboratory, and waveform synthesizer engineered on an **ESP32 dual-core Xtensa® LX6** architecture running **Protoware OS v2.5**.

Designed for embedded engineers, security researchers, and makers, the Proto S1 integrates a **1.3-inch OLED graphical interface**, an ergonomic **5-way tactile navigation pad**, an onboard **24-pin GPIO expansion bay**, a high-speed **FAT32 MicroSD interface**, and a robust **Dual-OTA partition architecture (1.98 MB twin slots)** with automated rollback protection.

```text
 ___________________________________________________________________
|  [Proto S1 v2.5]                               [WiFi: ON] [SD: OK] |
| ┌───────────────────────────────────────────────────────────────┐ |
| │  [>] 1. WiFi Suite         (Scanner, Sniffer, OTA Portal)     │ |
| │      2. Bluetooth LE       (BLE HID Mouse/Kbd, Multi-Spam)    │ |
| │      3. Communicate        (ESP-NOW Mesh, RF Chat, MQTT)      │ |
| │      4. Sensor Diagnostic  (24-Pin Interactive Matrix)        │ |
| │      5. GPIO Laboratory    (Waveform Synth, Servos, Relays)   │ |
| │      6. Retro Arcade       (Snake, Flappy Bird, Pong, Bricks) │ |
| │      7. System Settings    (Themes, Brightness, Diagnostics)  │ |
| │      8. Firmware Hub       (SD Binary Flash, GitHub Cloud)    │ |
| └───────────────────────────────────────────────────────────────┘ |
|  [UP / DN] Navigate    [RIGHT] Select    [LEFT] Back    [CTR] Exit|
|___________________________________________________________________|
```

---

## 🎮 Tactile Navigation Pad

Proto S1 features a dedicated **5-way tactile push-button pad** wired directly to hardware-debounced GPIO pins with internal pull-ups (`Active LOW`):

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

### Button Operations & Keybindings

| Button | ESP32 GPIO | Mode | Main Menu Function | Submenu & Tool Action |
| :--- | :---: | :---: | :--- | :--- |
| **UP** | `GPIO 26` | `INPUT_PULLUP` | Navigate upward | Increase servo angle / Increase signal frequency / Scroll up |
| **DOWN** | `GPIO 33` | `INPUT_PULLUP` | Navigate downward | Decrease servo angle / Decrease signal frequency / Scroll down |
| **LEFT** | `GPIO 25` | `INPUT_PULLUP` | Back / Previous view | Move cursor left in Pin Matrix / Cancel operation |
| **RIGHT** | `GPIO 27` | `INPUT_PULLUP` | **Select / Enter** | Move cursor right in Pin Matrix / Execute selected tool |
| **CENTER** | `GPIO 32` | `INPUT_PULLUP` | **Home / Global Exit** | Long-press: Break out of BLE HID Mouse/Kbd modes |

---

## 🔌 Hardware Pinout & Wiring Map

The Proto S1 custom black PCB routes all microcontroller signals with zero peripheral bus contention:

```text
                               ┌───────────────────┐
                        3V3 ───┤ 1              30 ├─── GND
                        EN  ───┤ 2              29 ├─── GPIO 23 (SD MOSI)
            (Debug) GPIO 36 ───┤ 3   ┌──────┐   28 ├─── GPIO 22 (OLED SCL)
            (Debug) GPIO 39 ───┤ 4   │ ESP  │   27 ├─── GPIO 1  (UART TX0)
            (Debug) GPIO 34 ───┤ 5   │ WROOM│   26 ├─── GPIO 3  (UART RX0)
            (Debug) GPIO 35 ───┤ 6   │  32  │   25 ├─── GPIO 21 (OLED SDA)
   (BTN CENTER)     GPIO 32 ───┤ 7   └──────┘   24 ├─── GPIO 19 (SD MISO)
   (BTN DOWN)       GPIO 33 ───┤ 8              23 ├─── GPIO 18 (SD SCK)
   (BTN LEFT)       GPIO 25 ───┤ 9              22 ├─── GPIO 5  (RF Transceiver CS)
   (BTN UP)         GPIO 26 ───┤ 10             21 ├─── GPIO 17 (IR Transmitter)
   (BTN RIGHT)      GPIO 27 ───┤ 11             20 ├─── GPIO 16 (Auxiliary Serial)
   (Servo / DAC)    GPIO 14 ───┤ 12             19 ├─── GPIO 4  (NeoPixel WS2812B)
   (PWM / Signal)   GPIO 12 ───┤ 13             18 ├─── GPIO 0  (Bootloader Mode)
   (SD Card CS)     GPIO 13 ───┤ 14             17 ├─── GPIO 2  (Hardware Status LED)
                        GND ───┤ 15             16 ├─── GPIO 15 (Transceiver IRQ)
                               └───────────────────┘
```

---

## 🔬 24-Pin Laboratory Expansion Bay

Proto S1 incorporates an on-screen **interactive 24-pin diagnostic header** with animated pin-state monitoring:

```text
Top Row (12 Pins):
  [ 3.3V ] [ SCL-22 ] [ SDA-21 ] [ GPIO-2 ] [ SCK-18 ] [ MISO-19 ] [ MOSI-23 ] [ GPIO-14 ] [ GPIO-12 ] [ CS-13 ] [ 3.3V ] [ GND ]

Bottom Row (12 Pins):
  [ +5V  ] [ GND    ] [ BTN-32 ] [ BTN-27 ] [ BTN-26 ] [ BTN-25  ] [ GPI-39  ] [ GPI-36  ] [ GPI-35  ] [ GPI-34 ] [ 3.3V ] [ GND ]
```

| Header Position | Pin Label | ESP32 GPIO | Direction | Purpose / Capability |
| :---: | :---: | :---: | :---: | :--- |
| **Top 1** | `3.3V` | — | Power Out | Dedicated clean 3.3V rail (up to 500 mA) |
| **Top 2** | `SCL` | `GPIO 22` | Output (OD) | Primary I2C Clock (400 kHz Fast-Mode) |
| **Top 3** | `SDA` | `GPIO 21` | In / Out | Primary I2C Data line |
| **Top 4** | `IO2` | `GPIO 2` | In / Out | General Purpose I/O & Onboard Blue LED indicator |
| **Top 5** | `SCK` | `GPIO 18` | Output | High-Speed Hardware VSPI Clock |
| **Top 6** | `MISO` | `GPIO 19` | Input | Hardware VSPI Master-In / Slave-Out |
| **Top 7** | `MOSI` | `GPIO 23` | Output | Hardware VSPI Master-Out / Slave-In |
| **Top 8** | `IO14` | `GPIO 14` | In / Out | High-Current Servo PWM & Ultrasonic Echo |
| **Top 9** | `IO12` | `GPIO 12` | In / Out | Waveform Synthesizer / PWM Signal Output |
| **Top 10** | `CS` | `GPIO 13` | Output | MicroSD High-Speed SPI Chip Select line |
| **Top 11** | `3.3V` | — | Power Out | Auxiliary 3.3V Power Rail |
| **Top 12** | `GND` | — | Power | Common System Ground |
| **Bottom 1** | `+5V` | — | Power Out | Unregulated USB 5V (VBUS) Power Rail |
| **Bottom 2** | `GND` | — | Power | Common System Ground |
| **Bottom 3** | `IO32` | `GPIO 32` | Input | Tactile CENTER Button / External Interrupt |
| **Bottom 4** | `IO27` | `GPIO 27` | Input | Tactile RIGHT Button / External Interrupt |
| **Bottom 5** | `IO26` | `GPIO 26` | Input | Tactile UP Button / External Interrupt |
| **Bottom 6** | `IO25` | `GPIO 25` | Input | Tactile LEFT Button / External Interrupt |
| **Bottom 7** | `IN39` | `GPIO 39` | Input Only | Low-Noise Analog ADC1_CH3 / Sensor Input (Sensor VN) |
| **Bottom 8** | `IN36` | `GPIO 36` | Input Only | Low-Noise Analog ADC1_CH0 / Sensor Input (Sensor VP) |
| **Bottom 9** | `IN35` | `GPIO 35` | Input Only | High-Impedance ADC1_CH7 Input |
| **Bottom 10**| `IN34` | `GPIO 34` | Input Only | High-Impedance ADC1_CH6 Input |
| **Bottom 11**| `3.3V` | — | Power Out | Auxiliary 3.3V Power Rail |
| **Bottom 12**| `GND` | — | Power | Common System Ground |

---

## 🛠️ Complete Subsystem Breakdown

### 1. 📡 Wireless Suite: WiFi
* **WiFi Scanner**: Real-time channel survey across 2.4 GHz spectrum with live RSSI signal strength bars and BSSID discovery.
* **OTA Web Portal**: Starts a local Captive AP with an embedded Web UI allowing browser uploads without internet connection.
* **Network Client**: Connects to pre-configured or field-scanned access points with automatic reconnect logic.
* **Packet Capture & Logs**: Real-time traffic analysis saved to FAT32 SD card in pcap-compatible format.

### 2. 🔵 Bluetooth Low Energy (NimBLE Architecture)
* **BLE Scanner**: Scans advertising packets, UUIDs, Manufacturer data, and device names.
* **BLE HID Mouse Emulation**: Turns Proto S1 into a wireless optical mouse using the 5-way D-pad (smooth directional vector tracking).
* **BLE HID Keyboard Emulation**: Sends keyboard shortcuts, text strings, and automated macros to paired computers/tablets.
* **Multi-Platform Notification Suite**:
  * **Apple iOS**: AirDrop & AirPods pairing beacon sequence.
  * **Google Android**: Google Fast Pair advertisement burst.
  * **Microsoft Windows**: Swift Pair discovery beacons.

### 3. 💬 Communication Subsystem
* **ESP-NOW Mesh**: High-speed, zero-router direct packet messaging between Proto S1 devices ($<2$ ms latency).
* **RF Transceiver Chat**: Support for 433 MHz / 2.4 GHz transceiver packet exchange.
* **MQTT Terminal Chat**: Secure message publishing and topic subscribing over local or cloud MQTT brokers.

### 4. 🎛️ Sensor Diagnostic Laboratory
The Sensor module features an **Interactive Graphical Pin Selector** allowing you to dynamically assign pins from the 24-pin header:
* **MPU6050 6-Axis IMU**: Real-time pitch, roll, yaw calculation with animated 3D horizon representation on OLED.
* **Ultrasonic Distance (HC-SR04)**: Microsecond-precision echo calculation with interactive TRIG/ECHO pin allocation and continuous distance graph ($2\text{ cm} - 400\text{ cm}$).
* **Infrared Receiver (IR)**: Protocol decoding for NEC, Sony, RC5, and raw pulse timing visualization.
* **PIR Motion Sensor**: High-sensitivity motion detection trigger with active state hold timer.
* **Capacitive Touch**: Real-time reading of ESP32 internal touch sensor thresholds.
* **DHT11 / DHT22**: Single-bus temperature ($^\circ\text{C} / ^\circ\text{F}$) and relative humidity (\%) decoding.

### 5. ⚡ GPIO Laboratory & Waveform Synthesizer
* **Servo Positioner**: $0^\circ \text{ to } 180^\circ$ continuous angle control with live microsecond pulse-width readout ($500\,\mu\text{s} - 2500\,\mu\text{s}$).
* **Waveform Generators**:
  * **Sine Wave**: Real-time computed DAC output with frequency adjustment ($1\text{ Hz} - 20\text{ kHz}$).
  * **Square Wave**: Hardware LEDC PWM with adjustable duty cycle ($0\% - 100\%$) and frequency.
  * **Triangle Wave**: Linear ramp synthesis for hardware calibration.
* **Actuators**: Motor H-Bridge controller, relay trigger sequencer, NeoPixel WS2812B rainbow & strobe effects.
* **I2C Bus Scanner**: Scans addresses `0x01` to `0x7F` and identifies known chipsets (OLED, RTC, IMU, EEPROM, Sensors).

### 6. 🎮 Switchable Cyberdeck UI Engines
Proto S1 features **4 distinct UI rendering engines** switchable in Settings:
1. **List Menu**: Professional animated terminal with smooth scrolling indicator and contextual icon previews.
2. **Nintendo DSi Theme**: Dual-screen split layout with carousel card transitions and bottom navigation cues.
3. **Nintendo Wii Theme**: Dynamic grid channel launcher reminiscent of classic gaming consoles.
4. **Pages View**: High-contrast, minimalist card layout optimized for direct sunlight readability.

### 7. 🕹️ Retro Arcade Games
Engineered with high frame-rate vector graphics:
* **Snake**: Classic directional arcade snake with score counter and speed ramps.
* **Bird**: Vector Flappy Bird clone with physics-based velocity and obstacle generation.
* **Bricks**: Breakout paddle game with angle deflection and destructible brick matrix.
* **Pong**: Single-player vs AI tennis game with predictive trajectory computation.

### 8. 🛡️ Fail-Safe Dual-OTA Partition Layout
Proto S1 uses a dedicated custom partition table (`partitions_ota.csv`) ensuring the device can never be bricked:

```csv
# Name,   Type, SubType, Offset,  Size, Flags
nvs,      data, nvs,     0x9000,  0x5000,
otadata,  data, ota,     0xe000,  0x2000,
app0,     app,  ota_0,   0x10000, 0x1F0000,
app1,     app,  ota_1,   0x200000,0x1F0000,
```

* **Twin 1.98 MB Slots**: Updates are flashed to the inactive slot while the current system runs safely.
* **Automatic Rollback**: If a new image crashes on boot, the ESP32 hardware watchdog automatically reverts to the known-good partition.
* **SD Card Field Flashing**: Place any compiled `firmware.bin` in `/firmware` on your SD card and flash without a computer.

---

## 🌐 Web Serial Flasher Suite

The repository includes a modern, zero-install Web Serial Flasher suite located in `web/index.html` (matching the bright studio design system of [s1.protox.in](https://s1.protox.in/)):

```text
 ┌──────────────────────────────────────────────────────────────────┐
 │  PROTO S1 v2.5 — Web Serial Deployment Suite                     │
 ├──────────────────────────────────────────────────────────────────┤
 │  [Step 1] Connect Hardware    --> Select Serial Port             │
 │  [Step 2] Remote MAC Check   --> Validate Hardware License      │
 │  [Step 3] Cloud Firmware Fetch--> Stream official v2.5 binary    │
 │  [Step 4] Auto-Inject Boot    --> Writes Bootloader + Partitions │
 └──────────────────────────────────────────────────────────────────┘
```

### Browser Requirements
* **Supported Browsers**: Google Chrome, Microsoft Edge, Brave, Opera (any browser with Web Serial API support).
* **Connection**: Plug Proto S1 into your computer via USB-C.

### Flashing Procedure
1. Open `web/index.html` locally or access it through your deployed portal.
2. Click **Connect Proto S1** and select the ESP32 COM port from the prompt.
3. The engine automatically inspects the chip and queries the remote hardware authorization list.
4. Select **Official Cloud Server** (or pick a local `.bin` file).
5. Click **Flash Proto S1 Firmware**. The suite injects:
   * **Bootloader** $\rightarrow$ `0x1000`
   * **Partition Table** $\rightarrow$ `0x8000`
   * **Boot App0 Flags** $\rightarrow$ `0xE000`
   * **Protoware v2.5 Application** $\rightarrow$ `0x10000`
6. Proto S1 automatically soft-resets into the updated operating system!

---

## 💻 Building from Source (PlatformIO)

### Prerequisites
* [VS Code](https://code.visualstudio.com/) with the [PlatformIO Extension](https://platformio.org/) installed.
* [Git](https://git-scm.com/) installed on your machine.

### 1. Clone the Repository
```bash
git clone https://github.com/RANJITHSCYBER/PROTOWARE.git
cd PROTOWARE
```

### 2. PlatformIO Configuration (`platformio.ini`)
```ini
[platformio]
default_envs = esp32dev

[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
board_build.partitions = partitions_ota.csv
upload_speed = 460800
monitor_speed = 115200
build_flags = 
    -Wl,-z,muldefs
    -D DISPLAY_TYPE=DISPLAY_AUTO
    -I src
    -I include
lib_deps = 
    adafruit/Adafruit GFX Library@^1.11.9
    adafruit/Adafruit SH110X@^2.1.14
    adafruit/Adafruit SSD1306@^2.5.17
    gyverlibs/GyverButton@^3.8
    olikraus/U8g2_for_Adafruit_GFX@^1.8.0
    h2zero/NimBLE-Arduino@^2.5.0
    knolleary/PubSubClient@^2.8
```

### 3. Compile & Upload via Command Line
```bash
# Build the firmware binary
pio run

# Upload to Proto S1 via USB Serial
pio run -t upload

# Open the serial monitor (115200 baud)
pio run -t monitor
```

---

## 📂 Repository File Layout

```text
PROTOWARE/
├── .pio/                     # PlatformIO build artifacts
├── include/
│   ├── CONFIG.h              # Global system configuration & URLs
│   ├── display.h             # OLED graphics driver & display abstraction
│   ├── hourglass_anim.h      # Animated splash/loading graphics
│   └── pindefs.h             # Fixed hardware GPIO allocations
├── partitions_ota.csv        # Dual-slot 1.98 MB OTA partition table
├── platformio.ini            # PlatformIO environments and library dependencies
├── protoware WEB/            # Official brand website assets (https://s1.protox.in/)
│   └── protoware/
│       ├── index.html        # Interactive marketing & specifications landing page
│       ├── styles.css        # Bright studio aesthetic & design system
│       └── script.js         # GSAP cinematic video scroll engine
├── src/
│   ├── interface/            # Menu engine hierarchy (List, DSi, Wii, Pages)
│   ├── menu/                 # Header declarations for all 8 subsystems
│   ├── about_animation.cpp   # Custom cinematic brand animation
│   ├── bluetooth_module.cpp  # NimBLE HID Mouse/Keyboard & Multi-Spammer
│   ├── boot_animation.cpp    # 150-frame startup cinematic sequence
│   ├── communicate_module.cpp# ESP-NOW, RF, and MQTT message routing
│   ├── firmware_module.cpp   # MicroSD .bin flasher & GitHub OTA client
│   ├── games_module.cpp      # Snake, Flappy Bird, Bricks, and Pong games
│   ├── gpio_module.cpp       # Waveform generator, Servo PWM, and Actuators
│   ├── main.cpp              # System lifecycle, button dispatcher, main loop
│   ├── sensor_module.cpp     # 24-Pin interactive matrix & sensor drivers
│   ├── settings.cpp          # Non-volatile EEPROM preferences & themes
│   └── wifi_module.cpp       # WiFi scanner, packet sniffer, and web portal
├── web/
│   ├── authorized_devices.json# Hardware MAC whitelist registry
│   ├── embedded_boot_data.js # Base64-encoded bootloader & partition blobs
│   ├── esptool.js            # In-browser Web Serial flashing engine
│   └── index.html            # Web Serial Flasher suite (Bright theme)
└── README.md                 # Complete system documentation
```

---

## 📞 Hardware Availability & Support

For hardware pre-orders, corporate licensing, custom PCB modifications, or device activation:

* **Official Device Portal**: [https://s1.protox.in/](https://s1.protox.in/)
* **Direct WhatsApp Support**: [Chat on WhatsApp (+91 7338006528)](https://wa.me/917338006528?text=Enquiry%20for%20the%20Proto%20S1%20hardware)
* **Phone / WhatsApp**: `+91 7338006528`
* **Lead Engineer**: [RANJITHSCYBER](https://github.com/RANJITHSCYBER)
* **Organization**: **ProtoX Technologies**

---

<p align="center">
  <sub>Proto S1 Multi-Tool • Protoware OS v2.5 • Developed by ProtoX & RANJITHSCYBER. All rights reserved.</sub>
</p>
