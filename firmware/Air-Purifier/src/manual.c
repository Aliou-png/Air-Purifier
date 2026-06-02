#include <stdio.h>
#include "driver/gpio.h"
#include "purifier.h"
#include "rom/ets_sys.h"

#define BUTTON_PWM_PIN  GPIO_NUM_12 // USING GPIO12
#define SAMPLE_COUNT    50
#define SAMPLE_DELAY_MS 67

// internal queue — ISR signals the monitor task
static QueueHandle_t button_event_queue;

// ─── ISR ────────────────────────────────────────────────────────────────────
static void IRAM_ATTR button_isr(void *arg) {
    uint8_t signal = 1;
    BaseType_t woken = pdFALSE;
    xQueueSendFromISR(button_event_queue, &signal, &woken);
    portYIELD_FROM_ISR(woken);
}

// ─── Speed Classification ────────────────────────────────────────────────────
static uint8_t sample_speed(void) {
    int high_count = 0;
    for (int i = 0; i < SAMPLE_COUNT; i++) {
        high_count += gpio_get_level(BUTTON_PWM_PIN);
        // vTaskDelay(pdMS_TO_TICKS(SAMPLE_DELAY_MS));
        ets_delay_us(47);
    }
    float ratio = (float)high_count / SAMPLE_COUNT;

    if (ratio < 0.15f)       return 0;   // off
    else if (ratio < 0.35f) return 1;   // low
    else if (ratio < 0.7f) return 2;   // medium
    else                    return 3;   // high
}

// ─── Monitor Task ───────────────────────────────────────────────────────────
static void manual_monitor_task(void *param) {
    uint8_t signal;
    uint8_t last_speed = 0;

    while (1) {
        // block until ISR detects a change
        xQueueReceive(button_event_queue, &signal, portMAX_DELAY);

        // debounce — small delay before sampling
        vTaskDelay(pdMS_TO_TICKS(50));

        // drain any extra ISR signals that fired during debounce
        while (xQueueReceive(button_event_queue, &signal, 0) == pdTRUE);

        // sample to classify speed
        uint8_t speed = sample_speed();

        // only send event if state actually changed
        if (speed != last_speed) {
            last_speed = speed;

            control_event_t event = {
                .type = EVT_FAN_SPEED,
                .data = speed
            };
            xQueueSend(control_queue, &event, portMAX_DELAY);

            printf("Manual input detected — speed: %d\n", speed);
            fflush(stdout);
        }
    }
}

// ─── Init ────────────────────────────────────────────────────────────────────
void manual_init(void) {
    // create internal queue
    button_event_queue = xQueueCreate(5, sizeof(uint8_t));

    // configure GPIO
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << BUTTON_PWM_PIN),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_ANYEDGE,
    };
    gpio_config(&io);

    // install ISR service and register handler
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BUTTON_PWM_PIN, button_isr, NULL);

    // start monitor task
    xTaskCreate(manual_monitor_task, "manual_monitor", 4096, NULL, 4, NULL);

    printf("Manual monitor initialized\n");
    fflush(stdout);
}