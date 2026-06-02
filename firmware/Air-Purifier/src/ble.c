#include "os/os_mbuf.h"
#include "purifier.h"
#include "ble.h" // give access to the init


// ─── Config ────────────────────────────────────────────────────────────────
#define DEVICE_NAME     "AirPurifier"

static uint16_t status_val_handle;
static uint16_t current_conn_handle;  // store this on connect


// ──────────────────────────────────────────────────────────────────────────────────────
//                  UUID (not they are backwards)
// ──────────────────────────────────────────────────────────────────────────────────────

// status UUID: 85eb9e95-c0e0-4aec-9b20-25b653c8f71f
static const ble_uuid128_t STATUS_UUID = BLE_UUID128_INIT
(0x1f, 0xf7, 0xc8, 0x53, 0xb6, 0x25, 0x20, 0x9b, 0xec, 0x4a, 0xe0, 0xc0, 0x95, 0x9e, 0xeb, 0x85);

// power UUID: 05102fd8-a90d-416c-9e1e-1214f55f6790
static const ble_uuid128_t POWER_UUID = BLE_UUID128_INIT
(0x90, 0x67, 0x5f, 0xf5, 0x14, 0x12, 0x1e, 0x9e, 0x6c, 0x41, 0x0d, 0xa9, 0xd8, 0x2f, 0x10, 0x05);

// service UUID: 009a5bad-d16d-481f-a049-aebd8138ec44
static const ble_uuid128_t SERVICE_UUID = BLE_UUID128_INIT
(0x44, 0xec, 0x38, 0x81, 0xbd, 0xae, 0x49, 0xa0, 0x1f, 0x48, 0x6d, 0xd1, 0xad, 0x5b, 0x9a, 0x00);

// schedule UUID: 96beb0d1-56c5-4c3d-bd20-fa4c6993d33e
static const ble_uuid128_t SCHEDULE_UUID = BLE_UUID128_INIT
(0x3e, 0xd3, 0x93, 0x69, 0x4c, 0xfa, 0x20, 0xbd, 0x3d, 0x4c, 0xc5, 0x56, 0xd1, 0xb0, 0xbe, 0x96);

// time UUID: aba0b55d-52bc-4f1d-b9dd-5d6da4cfc238
static const ble_uuid128_t TIME_UUID = BLE_UUID128_INIT
(0x38, 0xc2, 0xcf, 0xa4, 0x6d, 0x5d, 0xdd, 0xb9, 0x1d, 0x4f, 0xbc, 0x52, 0x5d, 0xb5, 0xa0, 0xab);

// fan speed UUID: 282b90e2-75f9-4145-9aac-1cb3c2e5ab10
static const ble_uuid128_t FAN_SPEED_UUID = BLE_UUID128_INIT
(0x10, 0xab, 0xe5, 0xc2, 0xb3, 0x1c, 0xac, 0x9a, 0x45, 0x41, 0xf9, 0x75, 0xe2, 0x90, 0x2b, 0x28);


// ──────────────────────────────────────────────────────────────────────────────────────
//                  START
// ──────────────────────────────────────────────────────────────────────────────────────
static void ble_advertise(void);
void ble_send_status(purifier_status_t *status);

// ─────────────────────────────────────────────────────────────────────
//              NimBLE
// ─────────────────────────────────────────────────────────────────────

// ─── NimBLE host task ───────────────────────────────────────────────────────
static void nimble_host_task(void *param) {
    // NimBLE requires its own task — this is the standard pattern
   // printf("NimBLE host task started");
    nimble_port_run();           // blocks here until nimble_port_stop()
    nimble_port_freertos_deinit();
}

// ─── BLE stack ready callback ───────────────────────────────────────────────
static void on_ble_sync(void) {

    // Verify we have a valid public address
    int rc = ble_hs_util_ensure_addr(0);
    assert(rc == 0);

    ble_advertise();
}

// ─── BLE reset ───────────────────────────────────────────────
static void on_ble_reset(int reason) {
    printf("BLE host reset — reason: %d", reason);
}

// ─────────────────────────────────────────────────────────────────────────────────
//              BLE app Commands
// ─────────────────────────────────────────────────────────────────────────────────

// ─── schedule enable received (callback) ───────────────────────────────────────────────
static int schedule_callback(uint16_t conn_handle, uint16_t attr_handle,
    struct ble_gatt_access_ctxt *ctxt, void *arg) {
   // Reads 5 bytes: [enabled, startHour, startMin, endHour, endMin]
       
     if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        uint8_t buf[5];
        // Copy the first 3 bytes into a flat buffer (only expecting 3)
        ble_hs_mbuf_to_flat(ctxt->om, buf, 5, NULL);
        
    printf("schedule set: enabled=%d start=%d:%d end=%d:%d\n", 
       buf[0], buf[1], buf[2], buf[3], buf[4]);
        fflush(stdout);
        // lock for writing:
        if (xSemaphoreTake(status_mutex, portMAX_DELAY)) {
            current_status.schedule_enabled = buf[0];
            current_status.start_hour       = buf[1];
            current_status.start_min        = buf[2];
            current_status.end_hour         = buf[3];
            current_status.end_min          = buf[4];
            xSemaphoreGive(status_mutex); // release lock
        }
        control_event_t event = { // create new update event
            .type = EVT_SCHEDULE_SET,
            .data = 0  // data will be ignored can just read from status
        };
        xQueueSend(control_queue, &event, portMAX_DELAY); // send update  
        
     }
        return 0;
}

// ─── power command received (callback) ───────────────────────────────────────────────
static int power_access(uint16_t conn_handle, uint16_t attr_handle,
    struct ble_gatt_access_ctxt *ctxt, void *arg) {
    
        if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        uint8_t value;
        os_mbuf_copydata(ctxt->om, 0, sizeof(value), &value);
        // lock for writting:
        if (xSemaphoreTake(status_mutex, portMAX_DELAY)) {
            current_status.is_powered = value;
             xSemaphoreGive(status_mutex); // release lock
        }
        control_event_t event = { // create new update event
            .type = EVT_POWER,
            .data = value  // on or off
        };
        xQueueSend(control_queue, &event, portMAX_DELAY); // send update  
        
    }
    return 0;
}

// ─── fan command received (callback) ───────────────────────────────────────────────
static int fan_speed(uint16_t conn_handle, uint16_t attr_handle,
    struct ble_gatt_access_ctxt *ctxt, void *arg) {
    
        if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        uint8_t value;
        os_mbuf_copydata(ctxt->om, 0, sizeof(value), &value);
        // lock for writting:
        int speed = 0;
        if (xSemaphoreTake(status_mutex, portMAX_DELAY)) {
            
            if (value == 0x19)      speed = 1;  // low
            else if (value == 0x32) speed = 2;  // medium
            else if (value == 0x64) speed = 3;  // high
            
            current_status.fan_speed = speed;
             xSemaphoreGive(status_mutex); // release lock
        }
        control_event_t event = { // create new update event
            .type = EVT_FAN_SPEED,
            .data = speed  // on or off
        };
        xQueueSend(control_queue, &event, portMAX_DELAY); // send update  
        
    }
    return 0;
}

// ─── time update received (callback) ───────────────────────────────────────────────
static int time_callback(uint16_t conn_handle, uint16_t attr_handle,
    struct ble_gatt_access_ctxt *ctxt, void *arg) {
     
    
     if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        // [hour, minute, second]
        uint8_t buf[3];
        // Copy the first 3 bytes into a flat buffer (only expecting 3)
        ble_hs_mbuf_to_flat(ctxt->om, buf, 3, NULL);
        
        printf("time stamp command received: %d:%d:%d\n", buf[0], buf[1], buf[2]);
        fflush(stdout);
        // lock for writing:
        if (xSemaphoreTake(status_mutex, portMAX_DELAY)) {
            current_status.stamp_hour = buf[0];
            current_status.stamp_min = buf[1];
            current_status.stamp_sec = buf[2];
            current_status.time_set = 1;
            xSemaphoreGive(status_mutex); // release lock
        }
        control_event_t event = { // create new update event
            .type = EVT_SCHEDULE_SET,
            .data = 0  // data will be ignored can just read from status
        };
        xQueueSend(control_queue, &event, portMAX_DELAY); // send update  
        
     }
        return 0;
}
// ─── status send (callback) ───────────────────────────────────────────────
static int status_access(uint16_t conn_handle, uint16_t attr_handle,
    struct ble_gatt_access_ctxt *ctxt, void *arg) {
    return 0;
}

// ─── Send Status ───────────────────────────────────────────────────────────────
void ble_send_status(purifier_status_t *status) {
    
    printf("ble_send_status called — conn_handle: %d\n", current_conn_handle);
    fflush(stdout);
    
   
    if (current_conn_handle == BLE_HS_CONN_HANDLE_NONE) {
        printf("No connection, skipping status send\n");
        fflush(stdout);
        return;
    }
    purifier_status_t snapshot;
    if (xSemaphoreTake(status_mutex, portMAX_DELAY)) {
        snapshot = *status;
        xSemaphoreGive(status_mutex);
    }
    
    uint8_t speed =  25;
    if(snapshot.fan_speed == 0 || snapshot.fan_speed ==  1) { speed = 25;}
    else if(snapshot.fan_speed == 2 ) { speed = 50;}
    else if(snapshot.fan_speed == 3 ) { speed = 100;}
    uint8_t buf[] = {
    snapshot.is_powered,        
    speed,         
    snapshot.mode,
    snapshot.schedule_enabled,  
    snapshot.start_hour,        
    snapshot.start_min,         
    snapshot.end_hour,          
    snapshot.end_min,           
    snapshot.filter_status,     
    };

    struct os_mbuf *om = ble_hs_mbuf_from_flat(&buf, sizeof(buf));
    ble_gatts_notify_custom(current_conn_handle, status_val_handle, om);
    
    printf("Status sent: powered=%d speed=%d filter=%d schedule=%d start=%d:%d end=%d:%d\n",
           snapshot.is_powered, snapshot.fan_speed, snapshot.filter_status,
           snapshot.schedule_enabled, snapshot.start_hour, snapshot.start_min,
           snapshot.end_hour, snapshot.end_min);
    fflush(stdout);
}

// ─────────────────────────────────────────────────────────────────────────────────
//             Service Characteristics
// ─────────────────────────────────────────────────────────────────────────────────

// ─── characteristic def ───────────────────────────────────────────────
static const struct ble_gatt_svc_def gatt_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &SERVICE_UUID.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid = &POWER_UUID.u,
                .access_cb = power_access,
                .flags = BLE_GATT_CHR_F_WRITE,
            },
            {    // status characteristics 
                .uuid = &STATUS_UUID.u,
                .access_cb = status_access, // empty place holder
                .flags = BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &status_val_handle,
             }, 
            { // schedule characteristic
                .uuid = &SCHEDULE_UUID.u,
                .access_cb = schedule_callback,
                .flags = BLE_GATT_CHR_F_WRITE, // write no notify
            },
            { // time characteristic
                .uuid = &TIME_UUID.u,
                .access_cb = time_callback,
                .flags = BLE_GATT_CHR_F_WRITE, // write no notify
            },
            { // fan speed characteristic
                .uuid = &FAN_SPEED_UUID.u,
                .access_cb = fan_speed,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_READ,
            },
            { 0 }, // terminator
        },
    },
    { 0 }, // terminator
};

// ─── GAP Event Handler (connection) ──────────────────────────────────────────────
static int gap_event_handler(struct ble_gap_event *event, void *arg) {
    switch (event->type) {

        case BLE_GAP_EVENT_CONNECT:
            if (event->connect.status == 0) {
                if (event->connect.status == 0) {
                    current_conn_handle = event->connect.conn_handle;
                    // dont sent status until phone accepts
                    printf("Client connected. "); 
                    fflush(stdout);

                control_event_t event = {
                .type = EVT_BLE_CONNECTED,
                .data = 0,
                };
                xQueueSend(control_queue, &event, portMAX_DELAY);
                
            }

            } else {
                printf("Connection failed, resuming advertising...");
                ble_advertise();
            }
            return 0;

        case BLE_GAP_EVENT_DISCONNECT: {
            printf("Client disconnected — reason: %d\n", event->disconnect.reason);
            fflush(stdout);

            control_event_t evt = {
                .type = EVT_BLE_DISCONNECTED,
                .data = 0,
            };
            xQueueSend(control_queue, &evt, portMAX_DELAY);
            ble_advertise();
            return 0;
}

        case BLE_GAP_EVENT_ADV_COMPLETE:
            printf("Advertising complete, restarting...");
            ble_advertise();
            return 0;
        
 case BLE_GAP_EVENT_SUBSCRIBE:
    if (event->subscribe.cur_notify && 
        event->subscribe.attr_handle == status_val_handle) {
        ble_send_status(&current_status);
    }
    return 0;

        default:
            return 0;
    }
}

// ─── GATT (Advertise) ─────────────────────────────────────────────────────────────
static void ble_advertise(void) {
    struct ble_hs_adv_fields fields = {0};

    // Advertisement packet: flags + service UUID only
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.uuids128             = &SERVICE_UUID;
    fields.num_uuids128         = 1;
    fields.uuids128_is_complete = 1;

    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        printf("ble_gap_adv_set_fields failed: %d\n", rc);
        fflush(stdout);
        return;
    }

    // Scan response packet: device name
    struct ble_hs_adv_fields rsp_fields = {0};
    rsp_fields.name             = (uint8_t *)DEVICE_NAME;
    rsp_fields.name_len         = strlen(DEVICE_NAME);
    rsp_fields.name_is_complete = 1;

    rc = ble_gap_adv_rsp_set_fields(&rsp_fields);
    if (rc != 0) {
        printf("ble_gap_adv_rsp_set_fields failed: %d\n", rc);
        fflush(stdout);
        return;
    }

    struct ble_gap_adv_params adv_params = {0};
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    rc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER,
                           &adv_params, gap_event_handler, NULL);
    if (rc != 0) {
        printf("ble_gap_adv_start failed: %d\n", rc);
        fflush(stdout);
        return;
    }

    printf("Advertising as \"%s\"\n", DEVICE_NAME);
    fflush(stdout);
}

// ─────────────────────────────────────────────────────────────────────
//              main
// ─────────────────────────────────────────────────────────────────────
// ─── App main ───────────────────────────────────────────────────────────────
void ble_init(void) {
    
    vTaskDelay(pdMS_TO_TICKS(3000));  // move it here temporarily
    

    // NVS is required by the BLE stack:
    // (paired device keys) so it persists across reboots.
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Init NimBLE
    nimble_port_init();

    // Register callbacks
    ble_hs_cfg.sync_cb  = on_ble_sync; // tell NimBLE what to do when connecting
    ble_hs_cfg.reset_cb = on_ble_reset; // tell NimBle what to do on a reset

    // Init GAP + GATT services (required even at this stage)
   
    /* GAP: handles device discovery, advertising, 

        and connection establishment (Peripheral/Central roles). */ 
    ble_svc_gap_init();

    /* GATT: handles structured data exchange 
        (Client/Server roles) once connected, 
        organizing data into services and characteristics. */ 
    ble_svc_gatt_init();

    // Set the device name that GAP broadcasts
    ble_svc_gap_device_name_set(DEVICE_NAME);


    ble_gatts_count_cfg(gatt_svcs); // allocate memory for the service
    ble_gatts_add_svcs(gatt_svcs); // tell nimBle what charecteristics to look for
    
    /* Start the NimBLE host task: 
    
     NimBLE's host stack needs to run continuously in the 
     background processing BLE events (connections, disconnections, 
     incoming data, etc). It can't just be a function you call once:
      — it needs its own dedicated FreeRTOS task. nimble_port_run() is the event 
      loop that blocks forever processing those events. 
    */ 
    nimble_port_freertos_init(nimble_host_task);

}