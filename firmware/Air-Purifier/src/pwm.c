#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

// for pwm
#include "driver/gpio.h"
#include "driver/ledc.h"

#include "esp_err.h"

#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"

#define FAN_PIN 3
 
void app_main_two(void)
{

    // configure the timer:
    ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT, // 8 bit resolution (0-255)
        .timer_num = LEDC_TIMER_0, // using timer zero
        .freq_hz = 5000, 
        .clk_cfg = LEDC_AUTO_CLK
    };

    ledc_timer_config(&timer); // inilize it

    // configure the challe
    ledc_channel_config_t channel = {
        .gpio_num = 2, //-------------------------------- using gpio 2
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel =  LEDC_TIMER_0, // must match timer 0
        .duty = 0,
        .hpoint = 0 // 
    };

    ledc_channel_config(&channel);
    ledc_fade_func_install(0);

    int max_duty = 127;  // 50% safety limit for motor testing

    while(1)
    {
        // Turn FAN ON
        gpio_set_level(FAN_PIN, 0);
       
        gpio_set_level(4, 0);
        printf("FAN ON\n");

        // Ramp up to 50%
        printf("Ramping up...\n");
        for (int i = 0; i <= max_duty; i++)
        {
            ledc_set_duty_and_update(LEDC_LOW_SPEED_MODE, 0, i, 0);
            printf("Duty: %d%%\n", (i * 100) / 255);
            vTaskDelay(pdMS_TO_TICKS(20));
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000));  // Hold at 50% for 1 second
        
        // Ramp down to 0
        printf("Ramping down...\n");
        for (int i = max_duty; i >= 0; i--)
        {
            ledc_set_duty_and_update(LEDC_LOW_SPEED_MODE, 0, i, 0);
            printf("Duty: %d%%\n", (i * 100) / 255);
            vTaskDelay(pdMS_TO_TICKS(20));
        }

        // Turn FAN OFF
        gpio_set_level(FAN_PIN, 0);
        printf("FAN OFF\n");
        
        vTaskDelay(pdMS_TO_TICKS(2000));  // Wait 2 seconds before repeating
    }

}