#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

static const char *TAG_BLE = "BLE_CAMERA";

#define BLE_DEVICE_NAME "ZV-E1"

// Service UUID: 8000-ff00-ff00-ffff-ffffffffffff
static const uint8_t target_service_uuid_bytes[16] = {
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0x80
};

// Characteristic UUIDs
#define BLE_CHAR_COMMAND_UUID 0xff01
#define BLE_CHAR_NOTIFY_UUID  0xff02

// ===== BLE Commands =====
static uint8_t cmd_focus_down[] = {0x01, 0x07};
static uint8_t TAKE_PICTURE[] = {0x01, 0x09};
static uint8_t SHUTTER_RELEASED[] = {0x01, 0x06};
static uint8_t HOLD_FOCUS[] = {0x01, 0x08};

// ===== State Variables =====
static bool ble_connected = false;
static uint16_t ble_conn_handle = 0;
static uint16_t ble_char_command_handle = 0;
static uint16_t ble_char_notify_handle = 0;
static bool camera_ready_to_pair = false;
static bool should_pair = false;

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

// Forward declarations
static void ble_scan_start(void);

// ===== Notification Callbacks =====
static int ble_on_notify(uint16_t conn_handle, const struct ble_gatt_error *error,
                         struct ble_gatt_attr *attr, void *arg)
{
    if (error->status == 0) {
        ESP_LOGI(TAG_BLE, "Notification received, handle=%d, len=%d", 
                 attr->handle, OS_MBUF_PKTLEN(attr->om));
        
        // Print hex data
        uint8_t data[256];
        uint16_t len = OS_MBUF_PKTLEN(attr->om);
        if (len > sizeof(data)) len = sizeof(data);
        
        os_mbuf_copydata(attr->om, 0, len, data);
        
        ESP_LOG_BUFFER_HEX(TAG_BLE, data, len);
    }
    return 0;
}

// ===== Subscribe to Notifications =====
static void subscribe_to_notifications(void)
{
    if (ble_char_command_handle != 0) {
        ESP_LOGI(TAG_BLE, "Subscribing to command notifications...");
        ble_gattc_indicate(ble_conn_handle, ble_char_command_handle);
        
        // Register for notifications
        ble_gattc_notify(ble_conn_handle, ble_char_command_handle);
    }
    
    if (ble_char_notify_handle != 0) {
        ESP_LOGI(TAG_BLE, "Subscribing to notify characteristic...");
        ble_gattc_notify(ble_conn_handle, ble_char_notify_handle);
    }
}

// ===== Characteristic Discovery Callback =====
static int ble_on_disc_chr(uint16_t conn_handle, const struct ble_gatt_error *error,
                           const struct ble_gatt_chr *chr, void *arg)
{
    if (error->status == 0) {
        uint16_t uuid16 = ble_uuid_u16(&chr->uuid.u);
        
        if (uuid16 == BLE_CHAR_COMMAND_UUID) {
            ESP_LOGI(TAG_BLE, "Command characteristic found! Handle: %d", chr->val_handle);
            ble_char_command_handle = chr->val_handle;
        } else if (uuid16 == BLE_CHAR_NOTIFY_UUID) {
            ESP_LOGI(TAG_BLE, "Notify characteristic found! Handle: %d", chr->val_handle);
            ble_char_notify_handle = chr->val_handle;
        }
    } else if (error->status == BLE_HS_EDONE) {
        ESP_LOGI(TAG_BLE, "Characteristic discovery complete");
        
        // Subscribe to notifications after discovery
        subscribe_to_notifications();
        
        ESP_LOGI(TAG_BLE, "BLE setup complete - ready to send commands!");
    }
    return 0;
}

// ===== Service Discovery Callback =====
static int ble_on_disc_svc(uint16_t conn_handle, const struct ble_gatt_error *error,
                           const struct ble_gatt_svc *service, void *arg)
{
    if (error->status == 0) {
        ble_uuid128_t target_uuid;
        ble_uuid_init_from_buf((ble_uuid_any_t *)&target_uuid, target_service_uuid_bytes, 16);

        if (ble_uuid_cmp(&service->uuid.u, &target_uuid.u) == 0) {
            ESP_LOGI(TAG_BLE, "Target Service Found! Discovering characteristics...");
            
            // Discover ALL characteristics in this service
            ble_gattc_disc_all_chrs(conn_handle, 
                                   service->start_handle, 
                                   service->end_handle,
                                   ble_on_disc_chr, 
                                   NULL);
        }
    } else if (error->status == BLE_HS_EDONE) {
        ESP_LOGD(TAG_BLE, "Service discovery complete");
    }
    return 0;
}

// ===== Parse Advertisement for Pairing State =====
static bool parse_camera_adv_data(const uint8_t *data, uint8_t len)
{
    // Look for the 0x22 flag that indicates pairing readiness
    for (int i = 1; i < len; i++) {
        if (data[i-1] == 0x22) {
            if ((data[i] & 0x40) == 0x40 && (data[i] & 0x02) == 0x02) {
                ESP_LOGI(TAG_BLE, "Camera is ready for pairing!");
                return true;
            } else {
                ESP_LOGI(TAG_BLE, "Camera found but not ready for pairing");
                return false;
            }
        }
    }
    return false;
}

// ===== GAP Event Handler =====
static int ble_gap_event(struct ble_gap_event *event, void *arg)
{
    struct ble_hs_adv_fields fields;
    int rc;

    switch (event->type) {
        case BLE_GAP_EVENT_DISC:
            rc = ble_hs_adv_parse_fields(&fields, event->disc.data, event->disc.length_data);
            if (rc != 0) return 0;

            if (fields.name_len == strlen(BLE_DEVICE_NAME) &&
                memcmp(fields.name, BLE_DEVICE_NAME, fields.name_len) == 0) {
                
                ESP_LOGI(TAG_BLE, "Found camera: %s", BLE_DEVICE_NAME);
                
                // Parse advertisement data to check pairing state
                camera_ready_to_pair = parse_camera_adv_data(event->disc.data, 
                                                             event->disc.length_data);
                should_pair = camera_ready_to_pair;
                
                ble_gap_disc_cancel();

                // Connect to camera
                rc = ble_gap_connect(BLE_OWN_ADDR_PUBLIC,
                                    &event->disc.addr,
                                    30000, 
                                    NULL,
                                    ble_gap_event, NULL);
                
                if (rc != 0) {
                    ESP_LOGE(TAG_BLE, "Failed to connect, rc=%d", rc);
                    ble_scan_start();
                }
            }
            break;

        case BLE_GAP_EVENT_CONNECT:
            if (event->connect.status == 0) {
                ESP_LOGI(TAG_BLE, "Connected to camera!");
                ble_connected = true;
                ble_conn_handle = event->connect.conn_handle;

                // Only initiate pairing if camera is ready
                if (should_pair) {
                    ESP_LOGI(TAG_BLE, "Initiating pairing...");
                    rc = ble_gap_security_initiate(ble_conn_handle);
                    if (rc != 0) {
                        ESP_LOGE(TAG_BLE, "Security initiate failed: rc=%d", rc);
                    }
                } else {
                    ESP_LOGI(TAG_BLE, "Attempting connection without pairing...");
                }

                // Discover services
                ble_uuid128_t target_uuid;
                ble_uuid_init_from_buf((ble_uuid_any_t *)&target_uuid, 
                                      target_service_uuid_bytes, 16);
                ble_gattc_disc_svc_by_uuid(ble_conn_handle, &target_uuid.u, 
                                          ble_on_disc_svc, NULL);
            } else {
                ESP_LOGE(TAG_BLE, "Connection failed; status=%d", event->connect.status);
                ble_scan_start();
            }
            break;

        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI(TAG_BLE, "Disconnected. Reason=%d", event->disconnect.reason);
            ble_connected = false;
            ble_char_command_handle = 0;
            ble_char_notify_handle = 0;
            
            vTaskDelay(pdMS_TO_TICKS(1000));
            ble_scan_start();
            break;

        case BLE_GAP_EVENT_ENC_CHANGE:
            ESP_LOGI(TAG_BLE, "Encryption change event; status=%d", 
                    event->enc_change.status);
            if (event->enc_change.status == 0) {
                ESP_LOGI(TAG_BLE, "Encryption enabled successfully");
            }
            break;

        case BLE_GAP_EVENT_PASSKEY_ACTION:
            ESP_LOGI(TAG_BLE, "Passkey action event");
            
            if (event->passkey.params.action == BLE_SM_IOACT_NUMCMP) {
                ESP_LOGI(TAG_BLE, "Numeric comparison: confirm=%d", 
                        event->passkey.params.numcmp);
                // Auto-confirm pairing
                struct ble_sm_io pk = {0};
                pk.action = event->passkey.params.action;
                pk.numcmp_accept = 1;  // Accept the pairing
                ble_sm_inject_io(event->passkey.conn_handle, &pk);
            } else if (event->passkey.params.action == BLE_SM_IOACT_NONE) {
                ESP_LOGI(TAG_BLE, "Pairing in progress...");
            }
            break;

        case BLE_GAP_EVENT_MTU:
            ESP_LOGI(TAG_BLE, "MTU update: conn_handle=%d mtu=%d",
                    event->mtu.conn_handle, event->mtu.value);
            break;

        case BLE_GAP_EVENT_NOTIFY_RX:
            ble_on_notify(event->notify_rx.conn_handle, 
                         &(struct ble_gatt_error){.status = 0},
                         &(struct ble_gatt_attr){
                             .handle = event->notify_rx.attr_handle,
                             .om = event->notify_rx.om
                         }, NULL);
            break;

        default:
            break;
    }
    return 0;
}

// ===== Start Scanning =====
static void ble_scan_start(void)
{
    if (ble_connected) return;

    ESP_LOGI(TAG_BLE, "Starting scan for %s...", BLE_DEVICE_NAME);

    struct ble_gap_disc_params disc_params = {0};
    disc_params.filter_duplicates = 1;
    disc_params.passive = 0;
    disc_params.itvl = 0;
    disc_params.window = 0;

    int rc = ble_gap_disc(BLE_OWN_ADDR_PUBLIC, BLE_HS_FOREVER, 
                         &disc_params, ble_gap_event, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG_BLE, "Error initiating discovery; rc=%d", rc);
    }
}

// ===== Send BLE Command =====
esp_err_t ble_send_command(uint8_t *data, size_t len)
{
    if (!ble_connected || ble_char_command_handle == 0) {
        ESP_LOGW(TAG_BLE, "Cannot send: not connected");
        return ESP_ERR_INVALID_STATE;
    }

    int rc = ble_gattc_write_no_rsp_flat(ble_conn_handle, 
                                        ble_char_command_handle, 
                                        data, len);
    if (rc != 0) {
        ESP_LOGE(TAG_BLE, "Write failed: %d", rc);
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG_BLE, "Command sent: %d bytes", len);
    return ESP_OK;
}

// ===== NimBLE Host Task =====
void ble_host_task(void *param)
{
    ESP_LOGI(TAG_BLE, "BLE Host Task Started");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

// ===== On Sync Callback =====
void ble_on_sync(void)
{
    int rc = ble_hs_util_ensure_addr(0);
    if (rc != 0) {
        ESP_LOGE(TAG_BLE, "Error ensuring address; rc=%d", rc);
        return;
    }

    // Set device name
    ble_svc_gap_device_name_set("ESP32-Camera-Remote");
    
    ble_scan_start();
}

// ===== On Reset Callback =====
void ble_on_reset(int reason)
{
    ESP_LOGE(TAG_BLE, "BLE reset, reason=%d", reason);
}

// ===== Initialize BLE =====
void ble_client_init(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize NimBLE
    nimble_port_init();

    // Configure security
    ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_NO_IO;  // No input/output capability
    ble_hs_cfg.sm_bonding = 1;                   // Enable bonding
    ble_hs_cfg.sm_mitm = 0;                      // No MITM protection
    ble_hs_cfg.sm_sc = 1;                        // Secure connections
    ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;

    // Set callbacks
    ble_hs_cfg.sync_cb = ble_on_sync;
    ble_hs_cfg.reset_cb = ble_on_reset;

    // Initialize services
    ble_svc_gap_init();
    ble_svc_gatt_init();

    // Start the task
    nimble_port_freertos_init(ble_host_task);
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