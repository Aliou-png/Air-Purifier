//
//  AirPurifierViewModel.swift
//  AirPurifier
//
//  Created by Aliou Tippett on 4/25/26.
//
/*
 What This Does

 - connect() — scans for your ESP32 by service UUID and connects automatically
 - disconnect() — cleanly disconnects
 - Characteristic discovery — finds all 5 characteristics after connecting
 - Status notifications — ESP32 can push real-time updates to the app
 - Schedule packing — sends schedule as a 5-byte payload to ESP32
 - Error handling — surfaces errors to the UI via errorMessage
 
 */

import Foundation
import Combine
import CoreBluetooth


// MARK: - UUIDs
let serviceUUID        = CBUUID(string: "009a5bad-d16d-481f-a049-aebd8138ec44")
let powerUUID          = CBUUID(string: "05102fd8-a90d-416c-9e1e-1214f55f6790")
let fanSpeedUUID       = CBUUID(string: "282b90e2-75f9-4145-9aac-1cb3c2e5ab10")
let modeUUID           = CBUUID(string: "4b173e51-0150-4a1a-9a75-b035e1846dfe")
let statusUUID         = CBUUID(string: "85eb9e95-c0e0-4aec-9b20-25b653c8f71f")
let scheduleUUID       = CBUUID(string: "96beb0d1-56c5-4c3d-bd20-fa4c6993d33e")
let timeUUID = CBUUID(string: "aba0b55d-52bc-4f1d-b9dd-5d6da4cfc238") // for sending current time:

// MARK: - ViewModel
final class AirPurifierViewModel: NSObject, ObservableObject {

    // MARK: Published State
    @Published var purifier = AirPurifier()
    @Published var isConnected: Bool = false
    @Published var isScanning: Bool = false
    @Published var errorMessage: String? = nil
    @Published var discoveredDevices: [CBPeripheral] = [] // allow scanning devices

    // MARK: BLE Properties
    private var centralManager: CBCentralManager!
    private var peripheral: CBPeripheral?

    private var powerCharacteristic: CBCharacteristic?
    private var fanSpeedCharacteristic: CBCharacteristic?
    private var modeCharacteristic: CBCharacteristic?
    private var statusCharacteristic: CBCharacteristic?
    private var scheduleCharacteristic: CBCharacteristic?
    private var timeCharacteristic: CBCharacteristic?

    // MARK: Init
    override init() {
        super.init()
        centralManager = CBCentralManager(
            delegate: self,
            queue: DispatchQueue.main,
            options: [CBCentralManagerOptionShowPowerAlertKey: true]
        )
    }
    // MARK: - Public Functions

    func connect() {
        guard centralManager.state == .poweredOn else {
            errorMessage = "Bluetooth is not available."
            return
        }
        isScanning = true
        // scanning for ESP
         centralManager.scanForPeripherals(withServices: [serviceUUID], options: nil) // only scan airpurifier
        // centralManager.scanForPeripherals(withServices: nil, options: nil) // for testing looks for all devices
    }

    // updats to also clear the deisconverd devices list
    func disconnect() {
        guard let peripheral = peripheral else { return }
        centralManager.cancelPeripheralConnection(peripheral)
        discoveredDevices = []
    }

    func togglePower() {
        let value: UInt8 = purifier.isPowered ? 0 : 1
        write(value: Data([value]), to: powerCharacteristic)
        purifier.isPowered.toggle()
        updateTimestamp()
    }

    func updateFanSpeed(_ speed: Int) {
        let value = UInt8(max(0, min(100, speed)))
        write(value: Data([value]), to: fanSpeedCharacteristic)
        purifier.fanSpeed = speed
        updateTimestamp()
    }

    func updateMode(_ mode: PurifierMode) {
        let value = mode.rawValue.data(using: .utf8) ?? Data()
        write(value: value, to: modeCharacteristic)
        purifier.mode = mode
        updateTimestamp()
    }
    
    func sendCurrentTime() {
        let now = Calendar.current.dateComponents([.hour, .minute, .second], from: Date())
        let payload: [UInt8] = [
            UInt8(now.hour ?? 0),
            UInt8(now.minute ?? 0),
            UInt8(now.second ?? 0)
        ]
        write(value: Data(payload), to: timeCharacteristic)
    }

    func toggleSchedule() {
        purifier.scheduleEnabled.toggle()
        if purifier.scheduleEnabled {
            sendCurrentTime() // re-sync time before schedule starts
        }
        sendSchedule()
        updateTimestamp()
    }

    func updateScheduleStart(_ time: Date) {
        purifier.scheduleStartTime = time
        sendSchedule()
        updateTimestamp()
    }

    func updateScheduleEnd(_ time: Date) {
        purifier.scheduleEndTime = time
        sendSchedule()
        updateTimestamp()
    }
    
    // after serching devices try to look for a device to connect to
    func connectToDevice(_ peripheral: CBPeripheral) {
        self.peripheral = peripheral
        centralManager.stopScan()
        isScanning = false
        centralManager.connect(peripheral, options: nil)
    }

    // MARK: - Private Helpers

    private func updateTimestamp() {
        purifier.lastUpdated = Date()
    }
    
    private func timeFrom(hour: UInt8, minute: UInt8) -> Date {
        var components = Calendar.current.dateComponents([.year, .month, .day], from: Date())
        components.hour = Int(hour)
        components.minute = Int(minute)
        return Calendar.current.date(from: components) ?? Date()
    }

    private func write(value: Data, to characteristic: CBCharacteristic?) {
        guard let peripheral = peripheral,
              let characteristic = characteristic else {
            errorMessage = "Not connected or characteristic not found."
            return
        }
        peripheral.writeValue(value, for: characteristic, type: .withResponse)
    }

    private func sendSchedule() {
        // Pack schedule as: [enabled(1 byte), startHour, startMin, endHour, endMin]
        let calendar = Calendar.current
        let start = calendar.dateComponents([.hour, .minute], from: purifier.scheduleStartTime)
        let end = calendar.dateComponents([.hour, .minute], from: purifier.scheduleEndTime)

        let payload: [UInt8] = [
            purifier.scheduleEnabled ? 1 : 0,
            UInt8(start.hour ?? 0),
            UInt8(start.minute ?? 0),
            UInt8(end.hour ?? 0),
            UInt8(end.minute ?? 0)
        ]
        write(value: Data(payload), to: scheduleCharacteristic)
    }
}

// MARK: - CBCentralManagerDelegate
extension AirPurifierViewModel: CBCentralManagerDelegate {

    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        
        if central.state != .poweredOn {
            isConnected = false
            errorMessage = "Bluetooth is unavailable. Please enable Bluetooth."
        } else {
            errorMessage = nil
        }
    }
    
    // updated to be able to scan all devices
    func centralManager(_ central: CBCentralManager,
                        didDiscover peripheral: CBPeripheral,
                        advertisementData: [String: Any],
                        rssi RSSI: NSNumber) {
        if !discoveredDevices.contains(peripheral) {
            discoveredDevices.append(peripheral)
        }
    }

    func centralManager(_ central: CBCentralManager,
                        didConnect peripheral: CBPeripheral) {
        isConnected = true
        peripheral.delegate = self
        peripheral.discoverServices([serviceUUID])
    }

    func centralManager(_ central: CBCentralManager,
                        didDisconnectPeripheral peripheral: CBPeripheral,
                        error: Error?) {
        isConnected = false
        self.peripheral = nil
        powerCharacteristic = nil
        fanSpeedCharacteristic = nil
        modeCharacteristic = nil
        statusCharacteristic = nil
        scheduleCharacteristic = nil
        timeCharacteristic = nil
    }

    func centralManager(_ central: CBCentralManager,
                        didFailToConnect peripheral: CBPeripheral,
                        error: Error?) {
        isConnected = false
        errorMessage = "Failed to connect. Please try again."
    }
}

// MARK: - CBPeripheralDelegate
extension AirPurifierViewModel: CBPeripheralDelegate {

    func peripheral(_ peripheral: CBPeripheral,
                    didDiscoverServices error: Error?) {
        guard let services = peripheral.services else { return }
        for service in services {
            peripheral.discoverCharacteristics(
                [powerUUID, fanSpeedUUID, modeUUID, statusUUID, scheduleUUID, timeUUID],
                for: service
            )
        }
    }

    func peripheral(_ peripheral: CBPeripheral,
                    didDiscoverCharacteristicsFor service: CBService,
                    error: Error?) {
        guard let characteristics = service.characteristics else { return }
        for characteristic in characteristics {
            switch characteristic.uuid {
            case powerUUID:
                powerCharacteristic = characteristic
            case fanSpeedUUID:
                fanSpeedCharacteristic = characteristic
            case modeUUID:
                modeCharacteristic = characteristic
            case statusUUID:
                statusCharacteristic = characteristic
                // Subscribe to status notifications from ESP32
                peripheral.setNotifyValue(true, for: characteristic)
            case scheduleUUID:
                scheduleCharacteristic = characteristic
            case timeUUID:
                timeCharacteristic = characteristic
            default:
                break
            }
        }
    }

    func peripheral(_ peripheral: CBPeripheral,
                    didUpdateValueFor characteristic: CBCharacteristic,
                    error: Error?) {
        guard characteristic.uuid == statusUUID,
              let data = characteristic.value else { return }

        // Parse status bytes from ESP32
        if data.count >= 9 {
            self.purifier.isPowered = data[0] == 1
            self.purifier.fanSpeed = Int(data[1])
            switch data[2] {
            case 0: self.purifier.mode = .auto
            case 1: self.purifier.mode = .sleep
            case 2: self.purifier.mode = .turbo
            default: self.purifier.mode = .auto
            }
            self.purifier.scheduleEnabled = data[3] == 1
            self.purifier.scheduleStartTime = self.timeFrom(hour: data[4], minute: data[5])
            self.purifier.scheduleEndTime = self.timeFrom(hour: data[6], minute: data[7])
            self.purifier.filterStatus = data[8] == 1 ? "Good" : "Replace Filter"
            self.updateTimestamp()
        }
    }
}
