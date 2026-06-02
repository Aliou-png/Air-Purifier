#include <stdio.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "purifier.h"

// ─── Scheduler Queue ─────────────────────────────────────────────────────────
QueueHandle_t sched_que;
uint8_t shedq_en = 0;

// ─── Helpers ─────────────────────────────────────────────────────────────────
static uint32_t to_seconds(uint8_t h, uint8_t m, uint8_t s) {
    return (h * 3600) + (m * 60) + s;
}

static bool is_within_schedule(void) {
    purifier_status_t snapshot;
    if (xSemaphoreTake(status_mutex, portMAX_DELAY)) {
        snapshot = current_status;
        xSemaphoreGive(status_mutex);
    }

    if (!snapshot.time_set) return false;

    uint32_t start = to_seconds(snapshot.start_hour, snapshot.start_min, 0);
    uint32_t end   = to_seconds(snapshot.end_hour, snapshot.end_min, 0);
    uint32_t now   = to_seconds(snapshot.stamp_hour, snapshot.stamp_min, snapshot.stamp_sec);

    if (start <= end) {
        return (now >= start && now <= end);
    } else {
        return (now >= start || now <= end);
    }
}

static void tick_clock(void) {
    if (xSemaphoreTake(status_mutex, portMAX_DELAY)) {
        current_status.stamp_sec++;
        if (current_status.stamp_sec >= 60) {
            current_status.stamp_sec = 0;
            current_status.stamp_min++;
            if (current_status.stamp_min >= 60) {
                current_status.stamp_min = 0;
                current_status.stamp_hour++;
                if (current_status.stamp_hour >= 24) {
                    current_status.stamp_hour = 0;
                }
            }
        }
        xSemaphoreGive(status_mutex);
    }
}

// ─── Scheduler Task ──────────────────────────────────────────────────────────
static void scheduler_task(void *arg) {
    printf("Schedule task started\n");
    fflush(stdout);

    bool schedule_on = false;

    while (1) {

        // -------- IDLE --------
        if (!shedq_en) {
            schedule_on = false;
            printf("Schedule task blocked\n");
            fflush(stdout);
            xQueueReceive(sched_que, &shedq_en, portMAX_DELAY);
            printf("Schedule task unblocked\n");
            fflush(stdout);
            continue;
        }

        // -------- RUNNING --------
        if (xQueueReceive(sched_que, &shedq_en, pdMS_TO_TICKS(1000))) {
            if (!shedq_en) continue;
        } else {
            // TIMEOUT — tick and check schedule
            tick_clock();

            if (is_within_schedule() && !schedule_on) {
                printf("Schedule fire ON — %d:%d\n",
                       current_status.stamp_hour, current_status.stamp_min);
                fflush(stdout);
                control_event_t event = { .type = EVT_SCHEDULE_FIRE, .data = 0x01 };
                xQueueSend(control_queue, &event, portMAX_DELAY);
                schedule_on = true;

            } else if (!is_within_schedule() && schedule_on) {
                printf("Schedule fire OFF — %d:%d\n",
                       current_status.stamp_hour, current_status.stamp_min);
                fflush(stdout);
                control_event_t event = { .type = EVT_SCHEDULE_FIRE, .data = 0x00 };
                xQueueSend(control_queue, &event, portMAX_DELAY);
                schedule_on = false;
            }
        }
    }
}

// ─── Init ────────────────────────────────────────────────────────────────────
void scheduler_init(void) {
    sched_que = xQueueCreate(1, sizeof(uint8_t));
    xTaskCreate(scheduler_task, "schedule", 4096, NULL, 5, NULL);
    printf("Scheduler initialized\n");
    fflush(stdout);
}