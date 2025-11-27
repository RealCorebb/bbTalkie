#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"

// Bluedroid/GATTC Includes
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "esp_gatt_defs.h"
#include "esp_bt_main.h"
#include "esp_bt_defs.h"


static const char *TAG_BLE = "BLE_CLIENT_NIMBLE";

// ===== Configuration Defines =====
#define BLE_DEVICE_NAME "ZV-E1"

// Service UUID: 8000-ff00-ff00-ffff-ffffffffffff (128-bit)
// Note: NimBLE expects UUIDs to be constructed carefully. 
// We will construct this into a ble_uuid128_t later.
static const uint8_t target_service_uuid_bytes[16] = {
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0x80
};

// Characteristic UUID: 0xff01 (16-bit)
#define BLE_CHAR_UUID_16BIT 0xff01

// ===== Command Type Enum & Structs (Unchanged) =====
typedef enum {
    CMD_TYPE_ANIMATION,
    CMD_TYPE_BLE_SEND
} command_type_t;


typedef struct {
    int key;
    spi_oled_animation_t *animation;
    command_type_t cmd_type;
    uint8_t *ble_data;      // Data to send via BLE
    size_t ble_data_len;    // Length of BLE data
} animation_map_entry_t;


spi_oled_animation_t anim_turn_left = {
    .x = 14,
    .y = 14,
    .width = 100,
    .height = 100,
    .frame_count = 22,
    .animation_data = (const uint8_t *)turn_left,
    .frame_delay_ms = 1000 / 30,
    .stop_frame = -1,
    .reverse = false,
    .task_handle = NULL};

spi_oled_animation_t anim_turn_right = {
    .x = 14,
    .y = 14,
    .width = 100,
    .height = 100,
    .frame_count = 22,
    .animation_data = (const uint8_t *)turn_right,
    .frame_delay_ms = 1000 / 30,
    .stop_frame = -1,
    .reverse = false,
    .task_handle = NULL};

spi_oled_animation_t anim_go_straight = {
    .x = 14,
    .y = 14,
    .width = 100,
    .height = 100,
    .frame_count = 22,
    .animation_data = (const uint8_t *)go_straight,
    .frame_delay_ms = 1000 / 30,
    .stop_frame = -1,
    .reverse = false,
    .task_handle = NULL};

spi_oled_animation_t anim_time_out = {
    .x = 14,
    .y = 14,
    .width = 100,
    .height = 100,
    .frame_count = 67,
    .animation_data = (const uint8_t *)time_out,
    .frame_delay_ms = 1000 / 30,
    .stop_frame = -1,
    .reverse = false,
    .task_handle = NULL};

spi_oled_animation_t anim_wait = {
    .x = 14,
    .y = 14,
    .width = 100,
    .height = 100,
    .frame_count = 10,
    .animation_data = (const uint8_t *)wait,
    .frame_delay_ms = 1000 / 15,
    .stop_frame = -1,
    .reverse = false,
    .task_handle = NULL};

spi_oled_animation_t anim_hello = {
    .x = 8,
    .y = 14,
    .width = 100,
    .height = 100,
    .frame_count = 32,
    .animation_data = (const uint8_t *)wave,
    .frame_delay_ms = 1000 / 30,
    .stop_frame = -1,
    .reverse = false,
    .task_handle = NULL};

spi_oled_animation_t anim_check_mark = {
    .x = 14,
    .y = 14,
    .width = 100,
    .height = 100,
    .frame_count = 32,
    .animation_data = (const uint8_t *)check_mark,
    .frame_delay_ms = 1000 / 30,
    .stop_frame = -1,
    .reverse = false,
    .task_handle = NULL};

spi_oled_animation_t anim_help_sos = {
    .x = 14,
    .y = 14,
    .width = 100,
    .height = 100,
    .frame_count = 5,
    .animation_data = (const uint8_t *)help_sos,
    .frame_delay_ms = 1000 / 15,
    .stop_frame = -1,
    .reverse = false,
    .task_handle = NULL};

spi_oled_animation_t anim_up_hill = {
    .x = 14,
    .y = 14,
    .width = 100,
    .height = 100,
    .frame_count = 30,
    .animation_data = (const uint8_t *)up_hill,
    .frame_delay_ms = 1000 / 30,
    .stop_frame = -1,
    .reverse = false,
    .task_handle = NULL};

spi_oled_animation_t anim_down_hill = {
    .x = 14,
    .y = 14,
    .width = 100,
    .height = 100,
    .frame_count = 30,
    .animation_data = (const uint8_t *)down_hill,
    .frame_delay_ms = 1000 / 30,
    .stop_frame = -1,
    .reverse = false,
    .task_handle = NULL};

spi_oled_animation_t anim_slow = {
    .x = 14,
    .y = 14,
    .width = 100,
    .height = 100,
    .frame_count = 30,
    .animation_data = (const uint8_t *)slow,
    .frame_delay_ms = 1000 / 30,
    .stop_frame = -1,
    .reverse = false,
    .task_handle = NULL};

spi_oled_animation_t anim_attention = {
    .x = 14,
    .y = 14,
    .width = 100,
    .height = 100,
    .frame_count = 30,
    .animation_data = (const uint8_t *)attention,
    .frame_delay_ms = 1000 / 30,
    .stop_frame = -1,
    .reverse = false,
    .task_handle = NULL};

spi_oled_animation_t anim_walk = {
    .x = 14,
    .y = 14,
    .width = 100,
    .height = 100,
    .frame_count = 8,
    .animation_data = (const uint8_t *)walk,
    .frame_delay_ms = 1000 / 15,
    .stop_frame = -1,
    .reverse = false,
    .task_handle = NULL};

spi_oled_animation_t anim_add_oil = {
    .x = 14,
    .y = 14,
    .width = 100,
    .height = 100,
    .frame_count = 57,
    .animation_data = (const uint8_t *)add_oil,
    .frame_delay_ms = 1000 / 30,
    .stop_frame = -1,
    .reverse = false,
    .task_handle = NULL};

spi_oled_animation_t anim_eat = {
    .x = 14,
    .y = 14,
    .width = 100,
    .height = 100,
    .frame_count = 60,
    .animation_data = (const uint8_t *)eat,
    .frame_delay_ms = 1000 / 30,
    .stop_frame = -1,
    .reverse = false,
    .task_handle = NULL};

spi_oled_animation_t anim_drink = {
    .x = 14,
    .y = 14,
    .width = 100,
    .height = 100,
    .frame_count = 8,
    .animation_data = (const uint8_t *)drink,
    .frame_delay_ms = 1000 / 15,
    .stop_frame = -1,
    .reverse = false,
    .task_handle = NULL};

// ===== BLE Data Buffers =====
static uint8_t cmd_focus_down[] = {0x01, 0x07};

static animation_map_entry_t animation_map[] = {
    {1, &anim_turn_left, CMD_TYPE_ANIMATION, NULL, 0},
    {2, &anim_turn_right, CMD_TYPE_ANIMATION, NULL, 0},
    {3, &anim_go_straight, CMD_TYPE_ANIMATION, NULL, 0},
    {4, &anim_time_out, CMD_TYPE_ANIMATION, NULL, 0},
    {5, &anim_wait, CMD_TYPE_ANIMATION, NULL, 0},
    {6, &anim_hello, CMD_TYPE_ANIMATION, NULL, 0},
    {7, &anim_check_mark, CMD_TYPE_ANIMATION, NULL, 0},
    {8, &anim_help_sos, CMD_TYPE_ANIMATION, NULL, 0},
    {9, &anim_up_hill, CMD_TYPE_ANIMATION, NULL, 0},
    {10, &anim_down_hill, CMD_TYPE_ANIMATION, NULL, 0},
    {11, &anim_slow, CMD_TYPE_ANIMATION, NULL, 0},
    {12, &anim_attention, CMD_TYPE_ANIMATION, NULL, 0},
    {13, &anim_walk, CMD_TYPE_ANIMATION, NULL, 0},
    {14, &anim_eat, CMD_TYPE_ANIMATION, NULL, 0},
    {15, &anim_drink, CMD_TYPE_ANIMATION, NULL, 0},
    {16, &anim_add_oil, CMD_TYPE_ANIMATION, NULL, 0},
    {50, NULL, CMD_TYPE_BLE_SEND, cmd_focus_down, sizeof(cmd_focus_down)},
};

// ===== BLE State Variables =====
static bool ble_connected = false;
static uint16_t ble_conn_handle = 0; // NimBLE uses handles, not IDs
static uint16_t ble_char_val_handle = 0; // Handle to write to
static bool target_device_found = false;

// (Keeping your animation structs placeholders for brevity/compilation)
spi_oled_animation_t anim_placeholder = {0}; 

// Forward declarations
static void ble_scan_start(void);
static int ble_gap_event(struct ble_gap_event *event, void *arg);


// Callback when a Characteristic is discovered
static int ble_on_disc_chr(uint16_t conn_handle, const struct ble_gatt_error *error,
                           const struct ble_gatt_chr *chr, void *arg)
{
    if (error->status == 0) {
        // Found a characteristic
        if (ble_uuid_u16(&chr->uuid.u) == BLE_CHAR_UUID_16BIT) {
            ESP_LOGI(TAG_BLE, "Target characteristic found! Handle: %d", chr->val_handle);
            ble_char_val_handle = chr->val_handle;
            // Setup complete
            ESP_LOGI(TAG_BLE, "BLE setup complete - ready to send commands!");
        }
    } else if (error->status == BLE_HS_EDONE) {
        // Discovery complete
        ESP_LOGD(TAG_BLE, "Characteristic discovery complete");
    }
    return 0;
}

// Callback when a Service is discovered
static int ble_on_disc_svc(uint16_t conn_handle, const struct ble_gatt_error *error,
                           const struct ble_gatt_svc *service, void *arg)
{
    if (error->status == 0) {
        // Service found, now verify UUID match
        ble_uuid128_t target_uuid;
        ble_uuid_init_from_buf((ble_uuid_any_t *)&target_uuid, target_service_uuid_bytes, 16);

        if (ble_uuid_cmp(&service->uuid.u, &target_uuid.u) == 0) {
            ESP_LOGI(TAG_BLE, "Target Service Found! Discovering characteristics...");
            
            // Search for the specific 16-bit characteristic inside this service
            ble_uuid16_t target_chr_uuid;
            ble_uuid_init_from_buf((ble_uuid_any_t *)&target_chr_uuid, 
                                  (const uint8_t[]){(BLE_CHAR_UUID_16BIT) & 0xFF, (BLE_CHAR_UUID_16BIT >> 8) & 0xFF}, 2);

            ble_gattc_disc_chrs_by_uuid(conn_handle, 
                                      service->start_handle, 
                                      service->end_handle, 
                                      &target_chr_uuid.u, 
                                      ble_on_disc_chr, 
                                      NULL);
        }
    } else if (error->status == BLE_HS_EDONE) {
        ESP_LOGD(TAG_BLE, "Service discovery complete");
    }
    return 0;
}

// ==========================================================
// ===== GAP Event Handler (Connection & Scanning) ==========
// ==========================================================

static int ble_gap_event(struct ble_gap_event *event, void *arg)
{
    struct ble_hs_adv_fields fields;
    int rc;

    switch (event->type) {
        case BLE_GAP_EVENT_DISC:
            // Event triggered for each advertisement packet received
            rc = ble_hs_adv_parse_fields(&fields, event->disc.data, event->disc.length_data);
            if (rc != 0) return 0;

            // Check if name matches
            if (fields.name_len == strlen(BLE_DEVICE_NAME) &&
                memcmp(fields.name, BLE_DEVICE_NAME, fields.name_len) == 0) {
                
                ESP_LOGI(TAG_BLE, "Found target device: %s", BLE_DEVICE_NAME);
                
                // Stop scanning explicitly
                ble_gap_disc_cancel();

                // Connect
                rc = ble_gap_connect(BLE_OWN_ADDR_PUBLIC, // Use your own address type
                     &event->disc.addr,
                     30000, 
                     NULL,
                     ble_gap_event, NULL);
                
                if (rc != 0) {
                    ESP_LOGE(TAG_BLE, "Failed to initiate connection, rc=%d", rc);
                    // Restart scan if connect failed immediately
                    ble_scan_start();
                }
            }
            break;

        case BLE_GAP_EVENT_CONNECT:
            if (event->connect.status == 0) {
                ESP_LOGI(TAG_BLE, "Connected to device!");
                ble_connected = true;
                ble_conn_handle = event->connect.conn_handle;

                ESP_LOGI(TAG_BLE, "Initiating Security/Pairing...");
                int rc = ble_gap_security_initiate(ble_conn_handle);
                if (rc != 0) {
                    ESP_LOGE(TAG_BLE, "Security initiate failed: rc=%d", rc);
                }

                // Discover target service (128-bit)
                ble_uuid128_t target_uuid;
                ble_uuid_init_from_buf((ble_uuid_any_t *)&target_uuid, target_service_uuid_bytes, 16);

                ble_gattc_disc_svc_by_uuid(ble_conn_handle, &target_uuid.u, ble_on_disc_svc, NULL);
            } else {
                ESP_LOGE(TAG_BLE, "Connection failed; status=%d", event->connect.status);
                ble_scan_start();
            }
            break;

        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI(TAG_BLE, "Disconnected. Reason=%d", event->disconnect.reason);
            ble_connected = false;
            ble_char_val_handle = 0;
            
            // Auto-reconnect
            vTaskDelay(pdMS_TO_TICKS(1000));
            ble_scan_start();
            break;
            
        case BLE_GAP_EVENT_MTU:
             ESP_LOGI(TAG_BLE, "mtu update event; conn_handle=%d cid=%d mtu=%d",
                 event->mtu.conn_handle,
                 event->mtu.channel_id,
                 event->mtu.value);
            break;

        default:
            break;
    }
    return 0;
}

static void ble_scan_start(void)
{
    if (ble_connected) return;

    ESP_LOGI(TAG_BLE, "Starting scan for %s...", BLE_DEVICE_NAME);

    struct ble_gap_disc_params disc_params;
    memset(&disc_params, 0, sizeof(disc_params));
    
    disc_params.filter_duplicates = 1;
    disc_params.passive = 0; // Active scanning
    disc_params.itvl = 0;    // Default scan interval
    disc_params.window = 0;  // Default scan window

    int rc = ble_gap_disc(BLE_OWN_ADDR_PUBLIC, BLE_HS_FOREVER, &disc_params, ble_gap_event, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG_BLE, "Error initiating gap discovery; rc=%d", rc);
    }
}

// ==========================================================
// ===== Public Helper Functions ============================
// ==========================================================

// Send data to BLE characteristic
esp_err_t ble_send_data(uint8_t *data, size_t len)
{
    if (!ble_connected || ble_char_val_handle == 0) {
        ESP_LOGW(TAG_BLE, "Cannot send: BLE not connected or char not found");
        return ESP_ERR_INVALID_STATE;
    }

    // write_no_rsp_flat is equivalent to WRITE_TYPE_NO_RSP
    int rc = ble_gattc_write_no_rsp_flat(ble_conn_handle, ble_char_val_handle, data, len);

    if (rc != 0) {
        ESP_LOGE(TAG_BLE, "BLE write failed: %d", rc);
        return ESP_FAIL;
    } else {
        ESP_LOGI(TAG_BLE, "BLE data sent: %d bytes", len);
    }
    return ESP_OK;
}

// NimBLE Host Task
void ble_host_task(void *param)
{
    ESP_LOGI(TAG_BLE, "BLE Host Task Started");
    // This function will return only when nimble_port_stop() is executed
    nimble_port_run();
    nimble_port_freertos_deinit();
}

// Callback when the stack is synced (ready)
void ble_on_sync(void)
{
    int rc = ble_hs_util_ensure_addr(0); // Ensure we have a random address if no public
    if (rc != 0) ESP_LOGE(TAG_BLE, "Device has no address");
    
    // Start scanning automatically on sync
    ble_scan_start();
}

// Initialize BLE Client
void ble_client_init(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 1. Initialize NimBLE port
    nimble_port_init();
    
    // 3. Start the FreeRTOS task for NimBLE
    nimble_port_freertos_init(ble_host_task);
}

// Re-connect helper (Maps to your old function)
void ble_connect_to_device(void)
{
    // In NimBLE, we just restart scanning to find and connect
    if (!ble_connected) {
        ble_scan_start();
    }
}

// ===== Accessor Functions (Minor safety fix added) =====
spi_oled_animation_t *get_animation_by_key(int key)
{
    // Added safety check for SENTINEL (-1) to prevent overflow
    for (int i = 0; animation_map[i].key != -1; i++)
    {
        if (animation_map[i].key == key)
        {
            if (animation_map[i].animation) {
                printf("map found: %d , width: %d , height: %d \n", animation_map[i].key,
                       animation_map[i].animation->width,
                       animation_map[i].animation->height);
                return animation_map[i].animation;
            }
        }
    }
    printf("map not found for key %d\n", key);
    return NULL; 
}

animation_map_entry_t *get_command_entry_by_key(int key)
{
    for (int i = 0; animation_map[i].key != -1; i++)
    {
        if (animation_map[i].key == key)
        {
            printf("Command found: %d, type: %d\n", 
                   animation_map[i].key, animation_map[i].cmd_type);
            return &animation_map[i];
        }
    }
    printf("Command not found: %d\n", key);
    return NULL;
}