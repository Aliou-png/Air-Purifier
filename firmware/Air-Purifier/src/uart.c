#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"

#define LED_GPIO2 2
#define LED_GPIO3 3

QueueHandle_t led_queue;

typedef enum {
    LED_OFF,
    LED_ON,
    FAN_ON,
    FAN_OFF
} led_ctrl;

void led_task(void *pvParameters)
{
    gpio_reset_pin(LED_GPIO2);
    gpio_set_direction(LED_GPIO2, GPIO_MODE_OUTPUT);
    gpio_reset_pin(LED_GPIO3);
    gpio_set_direction(LED_GPIO3, GPIO_MODE_OUTPUT);

    led_ctrl cmd;

    while (1) {
        xQueueReceive(led_queue, &cmd, portMAX_DELAY);
        switch(cmd) {
            case LED_OFF:
                gpio_set_level(LED_GPIO2, 0);
                gpio_set_level(LED_GPIO3, 0);
                printf("LED + FAN OFF\n");
                break;
            case LED_ON:
                gpio_set_level(LED_GPIO2, 1);
                gpio_set_level(LED_GPIO3, 1);
                printf("LED + FAN ON\n");
                break;
            case FAN_ON:
                gpio_set_level(LED_GPIO3, 1);
                printf("FAN ON\n");
                break;
            case FAN_OFF:
                gpio_set_level(LED_GPIO3, 0);
                printf("FAN OFF\n");
                break;
            default:
                printf("unrecognized command\n");
                break;
        }
    }
}

void uart_task(void *pvParameters)
{
    while (1) {
        int c = fgetc(stdin);
        if (c != EOF) {
            printf("got: %c\n", c);
            led_ctrl cmd;
            switch(c) {
                case '1': cmd = LED_ON;  xQueueSend(led_queue, &cmd, 0); break;
                case '0': cmd = LED_OFF; xQueueSend(led_queue, &cmd, 0); break;
                case '2': cmd = FAN_ON;  xQueueSend(led_queue, &cmd, 0); break;
                case '3': cmd = FAN_OFF; xQueueSend(led_queue, &cmd, 0); break;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void old_app_main(void)
{
    usb_serial_jtag_driver_config_t usb_serial_jtag_config = {
        .rx_buffer_size = 1024,
        .tx_buffer_size = 1024,
    };
    usb_serial_jtag_driver_install(&usb_serial_jtag_config);
    usb_serial_jtag_vfs_use_driver();

    led_queue = xQueueCreate(5, sizeof(led_ctrl));
xTaskCreate(led_task, "led_task", 2048, NULL, 1, NULL);
xTaskCreate(uart_task, "uart_task", 4096, NULL, 1, NULL);  // ← increased
}