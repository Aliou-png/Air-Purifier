
#include "purifier.h"
#include "ble.h" // give access to the init
#include "manual.h" // give access to the init
#include "scheduler.h" // give access to the init

// for pwm
#include "driver/gpio.h"
#include "driver/ledc.h"

#include "esp_err.h"

#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"

#define FAN_EN_PIN 4 // GPIO1
#define PWM_PIN 2 // GPIO2

#define LOW 120     // ~47%
#define MEDIUM 180  // ~71%
#define HIGH 242    // ~95% of 255

purifier_status_t current_status = {
    .is_powered = 0,
    .fan_speed = 0,
    .mode = 0,
    .filter_status = 1,
    .schedule_enabled = 0,
    .start_hour = 0,
    .start_min = 0,
    .end_hour = 0,
    .end_min = 0,
    .time_set = 0,
    .stamp_hour = 0,
    .stamp_min = 0,
    .stamp_sec = 0
};

// Implicite declarations

 void set_pwm(int speed);
 void pwm_init();

// ─── lock ────────────────────────────────────────────────────────────────── 
SemaphoreHandle_t status_mutex; 

 bool BLE_connected;
 int MAX_DUTY = 0;
// ─── Queue ──────────────────────────────────────────────────────────────────
// sending updates to the main control to start something
QueueHandle_t control_queue;


// ─── Control Task ───────────────────────────────────────────────────────────
static void control_task(void *param) {
    control_event_t event;
    BLE_connected = false;
    // set up pwm
    pwm_init();
    vTaskDelay(pdMS_TO_TICKS(100)); // small delay

    gpio_set_level(FAN_EN_PIN, 0); // disable fan
    set_pwm(0); // pwm = 0;

    while (1) {
        xQueueReceive(control_queue, &event, portMAX_DELAY);

        switch (event.type) {
            case EVT_POWER: // fan power on/ off --> only ble will set this
                u_int8_t fan_speed = 0;
                if (event.data == 1) {
                    if (xSemaphoreTake(status_mutex, portMAX_DELAY)) {
                        current_status.is_powered = 1;
                        fan_speed = current_status.fan_speed;
                        xSemaphoreGive(status_mutex);
                    }
                    gpio_set_level(FAN_EN_PIN, 1);
                    set_pwm(fan_speed);
                    if (BLE_connected) { ble_send_status(&current_status); }
                    
                } else {
                        purifier_status_t snapshot;
                        if (xSemaphoreTake(status_mutex, portMAX_DELAY)) {
                            current_status.is_powered = 0;
                            snapshot = current_status;
                            xSemaphoreGive(status_mutex);
                        }
                        snapshot.fan_speed = 0;
                    if (BLE_connected) { ble_send_status(&snapshot);}
                    gpio_set_level(FAN_EN_PIN, 0);
                    set_pwm(0);
                }
                
                break;
            case EVT_FAN_SPEED: // fan speed --> update current_status, send to fan task
                gpio_set_level(FAN_EN_PIN, 1); // enable fan
                // lock for writing:
                set_pwm(event.data);
                if (xSemaphoreTake(status_mutex, portMAX_DELAY)) 
                {
                    current_status.is_powered = (event.data != 0) ? 1: 0;
                    current_status.fan_speed = event.data;
                    xSemaphoreGive(status_mutex); // release lock
                }

                set_pwm(event.data);

                if (BLE_connected) // send up date
                {
                    ble_send_status(&current_status);
                }
                break;
            case EVT_MODE:
                // TODO: handle mode change
                break;
            case EVT_BLE_CONNECTED:
                BLE_connected = true;
                break;
            case EVT_BLE_DISCONNECTED:
                BLE_connected = false;
                break;
            case EVT_SCHEDULE_SET:
                if(current_status.schedule_enabled && current_status.time_set)
                {
                    shedq_en = 1;
                    xQueueSend(sched_que, &shedq_en, 0);
                }else{
                    shedq_en = 0;
                    xQueueSend(sched_que, &shedq_en, 0); 
                }
                break;
            case EVT_SCHEDULE_FIRE:
                if (event.data == 0x01) {
                    // schedule turning on — default to high speed
                    if (xSemaphoreTake(status_mutex, portMAX_DELAY)) {
                        current_status.is_powered = 1;
                        current_status.fan_speed = 3;  // high
                        xSemaphoreGive(status_mutex);
                    }
                    gpio_set_level(FAN_EN_PIN, 1);
                    set_pwm(3);
                } else {
                    // schedule turning off
                    if (xSemaphoreTake(status_mutex, portMAX_DELAY)) {
                        current_status.is_powered = 0;
                        xSemaphoreGive(status_mutex);
                    }
                    gpio_set_level(FAN_EN_PIN, 0);
                    set_pwm(0);
                }
                if (BLE_connected) {
                    ble_send_status(&current_status);
                }
                break;
        }
    }
}

 void set_pwm(int speed)
{
    switch(speed)
    {
        case 0: // off
            ledc_set_duty_and_update(LEDC_LOW_SPEED_MODE, 0, 0, 0);
            printf("main set speed zero/off \n");
            fflush(stdout);
            vTaskDelay(pdMS_TO_TICKS(20));
        break;

        case 1: // low
            ledc_set_duty_and_update(LEDC_LOW_SPEED_MODE, 0, LOW, 0);
            printf("main set speed low \n");
            fflush(stdout);
            vTaskDelay(pdMS_TO_TICKS(20));
        break;

        case 2: // medium
            ledc_set_duty_and_update(LEDC_LOW_SPEED_MODE, 0, MEDIUM, 0);
            printf("main set speed: medium \n");
            fflush(stdout);
            vTaskDelay(pdMS_TO_TICKS(20));
        break;
        
        case 3: // High
            ledc_set_duty_and_update(LEDC_LOW_SPEED_MODE, 0, HIGH, 0);
            printf("main set speed high \n");
            fflush(stdout);
            vTaskDelay(pdMS_TO_TICKS(20));
        break;

        default: // turn off
            ledc_set_duty_and_update(LEDC_LOW_SPEED_MODE, 0, 0, 0);
            vTaskDelay(pdMS_TO_TICKS(20));
        break;
    }
}

// ─── pwm setup ───────────────────────────────────────────────────────────
void pwm_init(void) {
    // configure enable pin as output
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << FAN_EN_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io);
    gpio_set_level(FAN_EN_PIN, 1);  // enable fan driver

    // configure the timer
    ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ledc_timer_config(&timer);

    // configure the channel
    ledc_channel_config_t channel = {
        .gpio_num = PWM_PIN,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0
    };
    ledc_channel_config(&channel);
    ledc_fade_func_install(0);

    MAX_DUTY = 127;
}
// ─── App Main ───────────────────────────────────────────────────────────────
void app_main(void) {
    vTaskDelay(pdMS_TO_TICKS(3000));

    // create shared resources first
    status_mutex = xSemaphoreCreateMutex();
    // create controll task:
    control_queue = xQueueCreate(10, sizeof(control_event_t));

    // main control task start // 
    xTaskCreate(control_task, "control", 4096, NULL, 5, NULL);
    
    // Schedule task start // 
    scheduler_init(); 
    
    // BLE task start // 
    ble_init(); // ble task

    // Manual Task start //
    manual_init(); // manual monitor task
}