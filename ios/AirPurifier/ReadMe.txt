Smart Air Purifier iOS App

Native iOS BLE Controller for ESP32-S3 Hardware
Developer: Aliou Tippett
Started: April 25, 2026
Status: iOS App Complete — Awaiting ESP32 Hardware
Type: Embedded Systems / iOS Development Project

Project Overview

A native iOS application built in Swift/SwiftUI that provides full wireless control of a custom-modified air purifier via Bluetooth Low Energy (BLE). The app communicates with an ESP32-S3 microcontroller that interfaces directly with the purifier's hardware, simulating button presses and controlling fan speed via PWM signals.
This project demonstrates end-to-end embedded systems integration — from low-level hardware control on a microcontroller, through a custom BLE GATT service, to a polished native iOS interface.

Technical Stack
LayerTechnologyiOS LanguageSwift 5.5+iOS FrameworkSwiftUIArchitectureMVVM (Model-View-ViewModel)Wireless ProtocolBluetooth Low Energy (BLE) 5.0iOS BLE FrameworkCoreBluetoothMicrocontrollerESP32-S3 N16R8Minimum iOS14.0+

Hardware Overview
ESP32-S3 (Lonely Binary N16R8 Gold Edition)

Role: BLE Peripheral / GATT Server
Wireless: Wi-Fi + BLE 5.0
GPIO: 30+ pins
Memory: 16MB Flash, 8MB PSRAM
Function: Acts as BLE server, interfaces with air purifier hardware

Air Purifier Hardware

Original power board (no MCU) controls fan via PWM
Button board with mechanical spring buttons
Fan control via two pins: FAN (on/off) and PWM (speed 0-100%)
ESP32 simulates button presses and monitors hardware status


Bluetooth Architecture
Protocol

Standard: Bluetooth Low Energy (BLE) 5.0
Profile: GATT (Generic Attribute Profile)
Topology: Central (iPhone) ↔ Peripheral (ESP32-S3)
Role: iPhone acts as GATT Central, ESP32 acts as GATT Server

How BLE GATT Works in This Project
The app uses a custom GATT service with multiple characteristics. Each characteristic represents a specific control or status value on the air purifier. The iPhone reads and writes these characteristics to send commands and receive status updates from the ESP32.

Services group related characteristics together
Characteristics are individual data points (power state, fan speed, mode, etc.)
Notifications allow the ESP32 to push real-time status updates to the iPhone without polling
UUIDs uniquely identify each service and characteristic — both sides must use identical UUIDs

Custom GATT Service Definition

Generated UUIDs at uuidgenerator.net
  - Service:   009a5bad-d16d-481f-a049-aebd8138ec44
  - Power:     05102fd8-a90d-416c-9e1e-1214f55f6790
  - FanSpeed:  282b90e2-75f9-4145-9aac-1cb3c2e5ab10
  - Mode:      4b173e51-0150-4a1a-9a75-b035e1846dfe
  - Status:    85eb9e95-c0e0-4aec-9b20-25b653c8f71f
  - Schedule:  96beb0d1-56c5-4c3d-bd20-fa4c6993d33e
  - time:      aba0b55d-52bc-4f1d-b9dd-5d6da4cfc238

Data Encoding

Power Characteristic
Write: [0x00] = Off
Write: [0x01] = On

Fan Speed Characteristic
Write: [0x19] = Low (25%)
Write: [0x32] = Medium (50%)
Write: [0x64] = High (100%)
Mode Characteristic
Write: UTF-8 string → "Auto" | "Sleep" | "Turbo"

Schedule Characteristic
Write: 5 bytes → [enabled, startHour, startMin, endHour, endMin]
Example: [0x01, 0x08, 0x00, 0x12, 0x00] = Enabled, 8:00 AM to 6:00 PM
Status Characteristic (ESP32 → iPhone)
Notify: 3 bytes → [isPowered, fanSpeed, filterStatus]
Example: [0x01, 0x32, 0x01] = On, 50% speed, filter good

BLE Connection Flow
1. iPhone initializes CBCentralManager
2. User taps Connect → app scans for service UUID
3. ESP32 discovered → displayed in device list
4. User selects ESP32 → app connects to peripheral
5. App discovers GATT services
6. App discovers all characteristics
7. App subscribes to Status characteristic notifications
8. Control panel unlocks → user can send commands
9. ESP32 pushes real-time status updates via notifications
10. On disconnect → app returns to connection screen
iOS Bluetooth Permissions Required
NSBluetoothAlwaysUsageDescription
NSBluetoothCentralUsageDescription
NSBluetoothPeripheralUsageDescription
Xcode Capabilities Required
Background Modes → Uses Bluetooth LE accessories

App Architecture (MVVM)
┌─────────────────────────────────────────┐
│                  VIEWS                   │
│  ContentView  DeviceListView  Control-  │
│  (connection) (BLE scanning)  PanelView │
│                               (controls)│
└─────────────────┬───────────────────────┘
                  │ @ObservedObject
┌─────────────────▼───────────────────────┐
│              VIEWMODEL                   │
│         AirPurifierViewModel            │
│  • CBCentralManager (BLE scanning)      │
│  • CBPeripheral (ESP32 connection)      │
│  • Characteristic read/write           │
│  • State management (@Published)        │
│  • Error handling                       │
└─────────────────┬───────────────────────┘
                  │
┌─────────────────▼───────────────────────┐
│                MODEL                     │
│            AirPurifier                  │
│  isPowered, fanSpeed, mode,             │
│  filterStatus, lastUpdated,             │
│  scheduleEnabled, scheduleStart/End     │
└─────────────────────────────────────────┘

Build Progress
Models

 PurifierMode enum (Auto, Sleep, Turbo) with CaseIterable + Identifiable
 AirPurifier struct with all properties
 Schedule properties (enabled, startTime, endTime)

ViewModel

 CBCentralManager initialization
 Bluetooth state handling
 BLE scanning (filtered by service UUID)
 Device discovery + list management
 Peripheral connection + disconnection
 GATT service + characteristic discovery
 Write functions (power, fanSpeed, mode, schedule)
 Status notifications from ESP32
 Schedule byte packing (5-byte payload)
 Error handling + user feedback
 Auto-clear on disconnect

Views

 ContentView — connection screen with status indicator
 DeviceListView — BLE scan results with scanning indicator
 ControlPanelView — full control interface

 Power toggle
 Fan speed picker (Low / Medium / High)
 Mode picker (Auto / Sleep / Turbo)
 Schedule section (enable, start time, end time)
 Filter status display
 Last updated timestamp
 Disconnect button

Setup & Permissions

 CoreBluetooth framework added
 NSBluetoothAlwaysUsageDescription added
 NSBluetoothCentralUsageDescription added
 NSBluetoothPeripheralUsageDescription added
 Background Modes → Uses Bluetooth LE accessories
 Bluetooth permission prompt confirmed working
 Device scanning confirmed working


ESP32 Firmware TODO

 Set up Arduino IDE or ESP-IDF environment
 Install ESP32 BLE library
 Define GATT service with matching UUIDs
 Implement Power characteristic (read/write)
 Implement FanSpeed characteristic (read/write)
 Implement Mode characteristic (read/write)
 Implement Status characteristic (notify)
 Implement Schedule characteristic (read/write)
 PWM fan speed control (0-100%)
 Button simulation via GPIO
 Status byte broadcasting (3-byte format)
 Test with nRF Connect app first


Testing Checklist
iOS App (Complete)

 Bluetooth permission prompt appears on first launch
 App shows in Settings → Privacy → Bluetooth
 Error message appears when Bluetooth is off
 Error clears when Bluetooth is turned on
 Connect button opens DeviceListView
 Scanning indicator appears
 Nearby BLE devices appear in list
 Cancel button dismisses sheet
 Disconnect button returns to connection screen

ESP32 Integration (Pending Hardware)

 Verify ESP32 broadcasts correct service UUID
 Verify all characteristic UUIDs match
 Test connection from iOS app
 Test power on/off command
 Test fan speed control
 Test mode switching
 Test schedule sending
 Test real-time status notifications
 Test unexpected disconnect handling
 Test reconnection flow
 
