// purifier.h
#ifndef PURIFIER_H
#define PURIFIER_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>
//  RTOS 
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

// nvs
#include "nvs_flash.h"
#include "esp_log.h"

// nimble
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"

#include "host/ble_hs.h"
#include "host/util/util.h"

#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

typedef struct {
    uint8_t is_powered;
    uint8_t fan_speed;
    uint8_t mode;
    uint8_t filter_status;
    uint8_t schedule_enabled;
    uint8_t start_hour;
    uint8_t start_min;
    uint8_t end_hour;
    uint8_t end_min;
    uint8_t time_set;
    uint8_t stamp_hour;
    uint8_t stamp_min;
    uint8_t stamp_sec;
} purifier_status_t;

extern purifier_status_t current_status;

// ─── Event Types ────────────────────────────────────────────────────────────
typedef enum {
    EVT_POWER,
    EVT_FAN_SPEED,
    EVT_MODE,
    EVT_BLE_CONNECTED,
    EVT_BLE_DISCONNECTED,
    EVT_SCHEDULE_SET,
    EVT_SCHEDULE_FIRE,
   // EVT_MANUAL_INPUT,
} event_type_t;

typedef struct {
    event_type_t type;
    uint8_t data;        // generic payload — power value, speed value, etc
} control_event_t;

// queue handle — extern so any file can send events
extern QueueHandle_t control_queue;
/* sending update events
    control_event_t event = {
        .type = EVT_POWER,
        .data = 0x01,   // on
    };
    xQueueSend(control_queue, &event, portMAX_DELAY);
*/


// declare the mutex for updating status
extern SemaphoreHandle_t status_mutex;
/* 
Writing:
    if (xSemaphoreTake(status_mutex, portMAX_DELAY)) {
    current_status.start_hour = payload[1];
    current_status.start_min  = payload[2];
    current_status.end_hour   = payload[3];
    current_status.end_min    = payload[4];
    xSemaphoreGive(status_mutex);
    }
Reading:
    // reading just one field — fine without lock
    uint8_t powered = current_status.is_powered;

    // reading multiple fields together — lock to get consistent snapshot
    purifier_status_t snapshot;
    if (xSemaphoreTake(status_mutex, portMAX_DELAY)) {
        snapshot = current_status;
        xSemaphoreGive(status_mutex);
    }
*/


    // Schedule task:
    extern QueueHandle_t sched_que;
    extern uint8_t shedq_en;
#endif