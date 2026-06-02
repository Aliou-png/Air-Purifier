# Smart Air Purifier — Technical overview

## Project Summary
A BLE-controlled smart air purifier built on ESP32-S3 with a native iOS app. The ESP32 runs FreeRTOS firmware that controls a real air purifier's fan motor via PWM, reads manual button input, and communicates wirelessly with an iPhone over BLE 5.0. The iOS app is written in Swift/SwiftUI with CoreBluetooth.

---

## Architecture Overview

### Firmware (ESP32-S3, ESP-IDF, FreeRTOS)
4 concurrent FreeRTOS tasks:
- BLE Task — NimBLE stack, handles all wireless communication
- Control Task — single owner of all hardware, event-driven FSM
- Scheduler Task — autonomous time-based power control
- Manual Monitor Task — ISR-driven button board input detection

All tasks communicate exclusively via queues to the control task. The control task is the only task that touches hardware (GPIO, PWM). This is called the single hardware owner pattern — it prevents race conditions by design.

### iOS App (Swift, SwiftUI, CoreBluetooth)
MVVM architecture. AirPurifierViewModel owns all BLE logic via CBCentralManager and CBPeripheral. Views are fully decoupled from BLE — they only observe @Published state. 3 views: ContentView (connection), DeviceListView (scanning), ControlPanelView (controls).

---

## BLE GATT Service Design

### Service UUID
009a5bad-d16d-481f-a049-aebd8138ec44

### Characteristics
| Characteristic | UUID | Type | Purpose |
|---|---|---|---|
| Power | 05102fd8-... | Write | On/Off |
| Fan Speed | 282b90e2-... | Write | Low/Medium/High |
| Mode | 4b173e51-... | Write | Auto/Sleep/Turbo (placeholder) |
| Status | 85eb9e95-... | Notify | Push state to iPhone |
| Schedule | 96beb0d1-... | Write | 5-byte schedule payload |
| Time | aba0b55d-... | Write | 3-byte time sync |

### Status Notification Payload (9 bytes)
Byte 0: is_powered
Byte 1: fan_speed
Byte 2: mode
Byte 3: schedule_enabled
Byte 4: start_hour
Byte 5: start_min
Byte 6: end_hour
Byte 7: end_min
Byte 8: filter_status

### Why notify instead of read for status?
Notify is push-based — ESP proactively sends updates when state changes. Read is pull-based — app would have to poll constantly. Notify is more efficient and gives instant UI updates.

### Why send status on subscribe not on connect?
The app needs time after connecting to discover services and subscribe to notifications. If you send on connect the notification fires before the app has subscribed and it gets lost. BLE_GAP_EVENT_SUBSCRIBE with cur_notify=1 is the correct trigger.

---

## FreeRTOS Architecture Decisions

### Why event-driven instead of FSM?
The system has overlapping concerns — BLE connected, schedule running, manual input — that don't fit cleanly into exclusive states. Event-driven with a central queue handles all combinations naturally. Any task puts an event in the queue and the control task decides what to do.

### Why single hardware owner?
If multiple tasks could write to PWM or GPIO directly you need a mutex around every hardware call. Instead, only the control task touches hardware and everything else sends events. Simpler, no hardware mutex needed.

### Queue vs shared buffer?
Queue used for commands (one-directional, serialized, thread-safe by design).
Mutex-protected struct used for shared state (purifier_status_t) that multiple tasks read.

### Scheduler dual-queue pattern
The scheduler task uses one queue for two purposes:
- When idle: blocks forever on the queue waiting for enable signal
- When running: waits on queue with 1 second timeout — if signal arrives handle it, if timeout fires tick the clock and check schedule

This avoids a separate timer task and keeps the scheduler self-contained.

### Why ISR for button board instead of polling?
A polling task wastes CPU waking up periodically even when nothing changed. ISR is hardware-driven — GPIO peripheral watches the pin at silicon level, zero CPU cost. CPU only wakes when pin actually changes state.

---

## Hardware Integration

### Logic Level Conversion
ESP32-S3 runs at 3.3V. Fan motor driver requires 5V signals. A 4-channel bidirectional logic level converter sits between them.

Channel layout:
- CH1: ESP PWM out (3.3V) → fan PWM in (5V)
- CH2: ESP enable out (3.3V) → fan enable in (5V)
- CH3: button board PWM out (5V) → ESP input (3.3V)
- CH4: button board enable out (5V) → ESP input (3.3V)

### PWM Fan Control
LEDC peripheral, Timer 0, Channel 0, 8-bit resolution (0-255), 5kHz frequency.
Speed values:
- Off: duty = 0
- Low: duty = 120 (~47%)
- Medium: duty = 180 (~71%)
- High: duty = 242 (~95%)

Why 95% not 100%? Running a motor at 100% continuously generates more heat. 95% is negligibly different in airflow but better for longevity.

Why 5kHz? Tested empirically — fan ran smoothly at this frequency. No datasheet available for the motor driver.

### Button Board Detection
Button board outputs a digital PWM signal at ~25kHz. Speed is encoded as duty cycle:
- Low: ~25% duty
- Medium: ~50% duty
- High: constant 5V (no PWM, just HIGH)

Detection approach: sample GPIO 100 times with 47µs between samples, count HIGH readings, compute ratio. 47µs chosen to avoid aliasing with 40µs PWM period (non-multiple).

Why not ADC? Tried it — ADC reads average voltage which works but is less reliable than digital sampling for this signal.

Why 47µs not 37µs? 37µs is nearly a perfect multiple of 40µs period causing aliasing (always reading same phase). 47µs is a non-multiple giving true average.

---

## Scheduler Design

### Time Sync
No WiFi, no RTC. App sends current time as 3 bytes [hour, minute, second] on connect. ESP stores it and increments every second via vTaskDelay(1000ms) in the scheduler task timeout path.

### Drift
vTaskDelay is not perfectly precise — drifts slightly over hours. Acceptable for a purifier schedule. Being a few seconds off over 12 hours doesn't matter.

### Schedule Logic
Supports overnight schedules (start > end handled separately).
Fires EVT_SCHEDULE_FIRE ON when entering window, OFF when leaving.
Local schedule_on flag prevents spamming queue on every tick.

### Default speed on schedule fire
Hardcoded to high speed (duty = 242). Can be overridden via app after schedule fires.

---

## Bugs Debugged

### 1. UUID Byte Ordering
Problem: Status characteristic not found by iOS app even though UUID looked correct.
Diagnosis: ble_gatts_show_local() showed the registered UUID didn't match README.
Root cause: BLE_UUID128_INIT takes bytes in reverse order (little-endian). Had the wrong byte at position 1.
Fix: Manually reversed all 16 bytes of the UUID.
Lesson: Always verify UUIDs with ble_gatts_show_local() before debugging anything else.

### 2. Subscribe Timing
Problem: Status notification sent on connect never received by app.
Diagnosis: Added BLE_GAP_EVENT_SUBSCRIBE handler — found cur_notify was 0 and attr_handle didn't match status_val_handle.
Root cause: Sending status on BLE_GAP_EVENT_CONNECT fires before app has subscribed. Also status_val_handle is 0 until after on_ble_sync — can't check it at registration time.
Fix: Send status only on BLE_GAP_EVENT_SUBSCRIBE when cur_notify == 1 and attr_handle == status_val_handle.

### 3. PWM Aliasing
Problem: Digital sampling of button board PWM gave random 0.0 or 1.0 ratios instead of expected 0.25/0.5/1.0.
Diagnosis: Added raw sample prints — confirmed always hitting exactly 0 or 1.
Root cause: 37µs sample interval is nearly a perfect multiple of 40µs PWM period — always sampling same phase.
Fix: Changed to 47µs — non-multiple of period gives true average across multiple cycles.

### 4. sdkconfig Persistence
Problem: BLE config flags added to sdkconfig.defaults not taking effect.
Root cause: PlatformIO uses sdkconfig.rymcu-esp32-s3-devkitc-1 as active config, not sdkconfig.defaults. Once generated it ignores defaults unless deleted.
Fix: Edit sdkconfig.rymcu-esp32-s3-devkitc-1 directly OR delete it and let it regenerate from defaults.

### 5. Advertisement Payload Too Large
Problem: ble_gap_adv_set_fields failed with error code 4 (BLE_HS_EBUSY).
Root cause: BLE advertisement packet is limited to 31 bytes. Trying to fit 128-bit service UUID + device name exceeded limit.
Fix: Put service UUID in advertisement packet, move device name to scan response packet via ble_gap_adv_rsp_set_fields().

### 6. NimBLE Headers Not Found
Problem: fatal error: nimble/nimble_port.h: No such file or directory
Root cause: NimBLE headers not on default include path in PlatformIO ESP-IDF project.
Fix: Added manual -I build_flags paths in platformio.ini pointing to NimBLE include directories under framework-espidf/components/bt/host/nimble/.

---

## Design Decisions and Tradeoffs

### Why NimBLE not Bluedroid?
NimBLE is lighter weight, lower memory footprint, recommended by Espressif for new ESP-IDF projects. Bluedroid supports Classic Bluetooth which we don't need.

### Why ESP-IDF not Arduino?
Already had FreeRTOS/queue/UART experience on ESP-IDF. More control over task priorities, stack sizes, and system configuration. Arduino abstracts too much for a production-style firmware project.

### Why PlatformIO not idf.py?
VS Code integration, easier build configuration, familiar workflow from previous projects.

### Why polling for manual monitor instead of pure ISR?
ISR detects the edge, but debouncing and duty cycle sampling require time-based logic that can't run in an ISR. Pattern: ISR signals a task, task handles the slow work.

### What would you do differently?
- Use hardware RTC instead of software clock for better time accuracy
- Add NVS persistence for schedule (survives power cut)
- Design a proper power distribution board instead of daisy-chaining 5V
- Add a proper 8-channel logic converter from the start
- Use a 2-flip-flop synchronizer for cross-clock domain button board input

---

## Numbers to Remember
- 4 FreeRTOS tasks
- 5 GATT characteristics (+ 1 placeholder mode)
- 9-byte status payload
- 5kHz PWM frequency
- 8-bit PWM resolution (0-255)
- 25kHz button board PWM frequency
- 47µs sample interval
- 100 samples per classification
- 1 second scheduler tick
- 3-byte time sync payload
- 5-byte schedule payload
