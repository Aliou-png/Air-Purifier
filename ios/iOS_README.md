# AirPurifier iOS App
### Native BLE Controller for ESP32-S3 Air Purifier

**Developer:** Aliou Tippett  
**Platform:** iOS 14.0+  
**Language:** Swift / SwiftUI  

---

## Overview

Native iOS app that wirelessly controls a custom air purifier via Bluetooth Low Energy. The app connects to an ESP32-S3 microcontroller acting as a BLE GATT server and provides full control over power, fan speed, mode, and scheduling.

---

## Bluetooth

- **Protocol:** BLE 5.0
- **Role:** GATT Central (iPhone scans and connects)
- **Framework:** CoreBluetooth
- **Peripheral:** ESP32-S3 GATT Server

### Service & Characteristics

| Characteristic | UUID | Type |
|---|---|---|
| Service | `009a5bad-d16d-481f-a049-aebd8138ec44` | Primary Service |
| Power | `05102fd8-a90d-416c-9e1e-1214f55f6790` | Write |
| Fan Speed | `282b90e2-75f9-4145-9aac-1cb3c2e5ab10` | Write |
| Mode | `4b173e51-0150-4a1a-9a75-b035e1846dfe` | Write |
| Status | `85eb9e95-c0e0-4aec-9b20-25b653c8f71f` | Notify |
| Schedule | `96beb0d1-56c5-4c3d-bd20-fa4c6993d33e` | Read/Write |
| Time | `aba0b55d-52bc-4f1d-b9dd-5d6da4cfc238` | Write |

### Data Formats

**Power** — 1 byte
```
0x00 = Off
0x01 = On
```

**Fan Speed** — 1 byte
```
0x19 = Low (25%)
0x32 = Medium (50%)
0x64 = High (100%)
```

**Mode** — 1 byte
```
0x00 = Auto
0x01 = Sleep
0x02 = Turbo
```

**Schedule** — 5 bytes
```
[enabled, startHour, startMin, endHour, endMin]
```

**Time** — 3 bytes (24-hour format, sent only when schedule is enabled)
```
[hour, minute, second]
```

**Status Notification** — 9 bytes (ESP32 → iPhone)
```
[isPowered, fanSpeed, mode, scheduleEnabled, startHour, startMin, endHour, endMin, filterStatus]
```

---

## Setup

### Requirements
- Xcode 13+
- iOS 14.0+ device
- Physical iPhone (Bluetooth not available in simulator)

### Info.plist Keys Required
```
NSBluetoothAlwaysUsageDescription
NSBluetoothCentralUsageDescription
NSBluetoothPeripheralUsageDescription
```

### Xcode Capabilities Required
```
Background Modes → Uses Bluetooth LE accessories
```

---

## Notes

- Before ESP32 integration, scanning is set to `withServices: nil` for testing. Change to `[serviceUUID]` for production.
- Time is only sent to the ESP32 when the user **enables** the schedule, not on connect.
- The ESP32 sends a status notification automatically on connect to sync current device state.

---

*Part of a larger embedded systems project — see ESP32 README for firmware details.*
