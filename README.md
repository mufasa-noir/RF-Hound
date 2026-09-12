# RF Hound

**Status:** Early development - v1 scope is WiFi + Bluetooth. See [Roadmap](#roadmap) for what's coming next.

---

## Overview

RF Hound is an open-source device for testing and understanding wireless networks around you. This first version focuses entirely on **WiFi** and **Bluetooth/BLE**, giving you a compact, affordable tool for network reconnaissance, wireless auditing, and hands-on learning.

## Features (v1)

### WiFi

- **Network scanning** - enumerate nearby access points and connected clients
- **Deauth detection** - passively alert when deauthentication frames are seen on a network
- **Deauth capability** - send deauth frames for authorized testing
- **PMKID / handshake capture**- capture WPA handshakes for authorized audit work
- **Evil twin** - stand up a rogue AP mimicking a target network for authorized phishing-awareness testing
- **Evil portal / captive portal** - serve a custom captive portal page for authorized awareness testing
- **Beacon flooding** - send multiple beacons of similar SSID as a means of DOS for awareness testing.
- **Packet monitoring and injection** - mimick the monitor mode of powerful wireless adapters using the ESP32's Promiscuous mode(may not be as efficienct as using a monitor mode enabled wireless adapter).
- **2.4GHz signal jamming** - flood the 2.4GHz frequency with "noise" rendering communication of signals within this range useless(It affects bluetooth communication too). 

### Bluetooth / BLE

- **Scanning & sniffing** - discover nearby BLE devices with basic fingerprinting
- **Advertising spam** - send BLE advertising packets (notification-spam style) as an awareness proof-of-concept
- **HID injection** - inject BLE HID input for authorized testing

## Hardware (v1)

- Original ESP32/ESP32-S3 (dual-core, built-in WiFi + BLE radio)
- 1.8" TFT Display
- Buttons for navigation
- microSD for captures and logs
- LiPo battery
- nRF24L01 transceiver modules(optional)

No additional radio modules are required for this version - everything runs on the ESP32's onboard WiFi/BLE. Sub-GHz, NFC, and IR front-ends come in later hardware revisions (see below).

## Interface Modes

The device offers two ways to interact with it, selectable via a physical button:

- **On-device display UI** — navigate menus and run features directly from the device's screen and buttons, with no companion hardware needed.
- **Web UI** — connect from a browser (over WiFi or USB) for a fuller interface, better suited to reviewing captures and managing data.

Only one mode is active by default. Running both at once is supported but less recommended, since it splits the device's resources across two interfaces simultaneously.

## Roadmap

Later versions will expand RF Hound into a full-suite RF tool:

- Sub-GHz radio (capture/replay, protocol decoding, rolling-code analysis)
- NFC/RFID (13.56MHz and 125kHz read/write/emulate)
- Infrared (universal remote, capture/replay)
- USB/BadUSB (HID payload injection)
- GPIO hardware-hacking bench (logic analyzer, SPI/I2C/UART, JTAG/SWD)
- A wideband RF detector layer for finding hidden transmitters, cameras, and trackers - with direction-finding to help locate the source
- Companion app (web-based, WiFi/BLE/USB) with optional local-LLM-assisted analysis

## Getting Started

```
# placeholder - firmware build via ESP-IDF
git clone https://github.com/mufasa-noir/RF-Hound.git
cd RF Hound
# build & flash instructions coming soon
```

## Responsible Use

Deauth, evil twin, evil portal, PMKID capture, beacon flooding, packet monitoring/injection, 2.4GHz signal jamming and BLE HID injection are built for **authorized security testing only** - on networks and devices you own or have explicit permission to test. Wireless transmission and interception laws vary by country; check your local regulations before using transmit-capable features.

## Contributing

RF Hound is open source and community contributions are welcome - hardware designs, firmware modules, and documentation alike. Contribution guidelines will be added as the project structure firms up.

## License

Licensed under the GNU General Public License v3.0 (GPL-3.0) — see `LICENSE` for full text.
