Developer: Aliou Tippett — Spring 2026

# Smart Air Purifier — Project Overview 

A custom-built smart air purifier controlled wirelessly via Bluetooth Low Energy.
Built as an embedded systems portfolio project.

---

## How It Works

An iPhone app connects to an ESP32-S3 microcontroller over BLE.
The ESP32 is wired into a modified air purifier and controls
the fan speed, power, and scheduling directly via GPIO and PWM signals.

---

## Components

**iOS App** (`/ios`)
Native Swift/SwiftUI app. Handles BLE scanning, connecting, and sending
commands to the ESP32. See `ios/README.md` for details.

**ESP32 Firmware** (`/esp32`)
C firmware running on the ESP32-S3. Acts as a BLE GATT server and controls
the air purifier hardware via GPIO and PWM. See `esp32/README.md` for details.
