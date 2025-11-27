
// ===== Configuration Defines =====
#define BLE_DEVICE_NAME "ZV-E1"

// Service UUID: 8000-ff00-ff00-ffff-ffffffffffff (128-bit)
static uint8_t target_service_uuid[16] = {
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0x80
};

// Characteristic UUID: 0xff01 (16-bit)
#define BLE_CHAR_UUID_16BIT 0xff01

// ===== Command Type Enum =====
typedef enum {
    CMD_TYPE_ANIMATION,
    CMD_TYPE_BLE_SEND
} command_type_t;

// ===== Enhanced Animation Map =====
typedef struct
{
    int key;
    spi_oled_animation_t *animation;
    command_type_t cmd_type;
    uint8_t *ble_data;      // Data to send via BLE
    size_t ble_data_len;    // Length of BLE data
} animation_map_entry_t;

// ===== BLE State Variables =====
static bool ble_connected = false;
static bool ble_service_found = false;
static uint16_t ble_conn_id = 0;
static uint16_t ble_char_handle = 0;
static uint16_t ble_service_start_handle = 0;
static uint16_t ble_service_end_handle = 0;
static esp_gatt_if_t ble_gattc_if = ESP_GATT_IF_NONE;
static esp_bd_addr_t target_device_addr;
static bool target_device_found = false;

#define GATTC_APP_ID 0

// ===== BLE Data Buffers (examples) =====
static uint8_t cmd_focus_down[] = {0x01, 0x07};

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

// ===== Enhanced Animation Map with BLE Support =====
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
    {20, NULL, CMD_TYPE_BLE_SEND, cmd_focus_down, sizeof(cmd_focus_down)},
};

// Compare 128-bit UUIDs
static bool uuid_128_cmp(uint8_t *uuid1, uint8_t *uuid2)
{
    return memcmp(uuid1, uuid2, 16) == 0;
}

// Send data to BLE characteristic
esp_err_t ble_send_data(uint8_t *data, size_t len)
{
    if (!ble_connected) {
        ESP_LOGW(TAG, "BLE not connected, cannot send data");
        return ESP_ERR_INVALID_STATE;
    }

    if (ble_char_handle == 0) {
        ESP_LOGW(TAG, "BLE characteristic not found, cannot send data");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = esp_ble_gattc_write_char(
        ble_gattc_if,
        ble_conn_id,
        ble_char_handle,
        len,
        data,
        ESP_GATT_WRITE_TYPE_NO_RSP,
        ESP_GATT_AUTH_REQ_NONE
    );

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "BLE write failed: %d", ret);
    } else {
        ESP_LOGI(TAG, "BLE data sent: %d bytes [0x%02X 0x%02X]", len, data[0], data[1]);
    }

    return ret;
}

// GAP Event Handler
static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
        case ESP_GAP_BLE_SCAN_RESULT_EVT: {
            esp_ble_gap_cb_param_t *scan_result = (esp_ble_gap_cb_param_t *)param;
            
            if (scan_result->scan_rst.search_evt == ESP_GAP_SEARCH_INQ_RES_EVT) {
                // Check device name
                uint8_t *adv_name = NULL;
                uint8_t adv_name_len = 0;
                adv_name = esp_ble_resolve_adv_data(scan_result->scan_rst.ble_adv,
                                                     ESP_BLE_AD_TYPE_NAME_CMPL,
                                                     &adv_name_len);
                
                if (adv_name != NULL) {
                    if (adv_name_len == strlen(BLE_DEVICE_NAME) && 
                        memcmp(adv_name, BLE_DEVICE_NAME, adv_name_len) == 0) {
                        
                        ESP_LOGI(TAG, "Found target device: %s", BLE_DEVICE_NAME);
                        
                        // Stop scanning
                        esp_ble_gap_stop_scanning();
                        
                        // Save device address
                        memcpy(target_device_addr, scan_result->scan_rst.bda, sizeof(esp_bd_addr_t));
                        target_device_found = true;
                        
                        // Connect to device
                        if (ble_gattc_if != ESP_GATT_IF_NONE) {
                            esp_ble_gattc_open(ble_gattc_if, target_device_addr, 
                                              scan_result->scan_rst.ble_addr_type, true);
                        }
                    }
                }
            }
            break;
        }
        
        case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
            ESP_LOGI(TAG, "Scan stopped");
            break;
            
        default:
            break;
    }
}

// GATT Client Event Handler
static void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param)
{
    switch (event) {
        case ESP_GATTC_REG_EVT:
            ESP_LOGI(TAG, "GATT client registered, app_id: %04x", param->reg.app_id);
            ble_gattc_if = gattc_if;
            
            // If we already found the device during scanning, connect now
            if (target_device_found) {
                esp_ble_gattc_open(ble_gattc_if, target_device_addr, BLE_ADDR_TYPE_PUBLIC, true);
            }
            break;

        case ESP_GATTC_OPEN_EVT:
            if (param->open.status == ESP_GATT_OK) {
                ble_connected = true;
                ble_conn_id = param->open.conn_id;
                ESP_LOGI(TAG, "BLE Connected, conn_id: %d", ble_conn_id);
                
                // Configure MTU
                esp_ble_gattc_send_mtu_req(gattc_if, ble_conn_id);
            } else {
                ESP_LOGE(TAG, "BLE connection failed, status: %d", param->open.status);
                target_device_found = false;
            }
            break;

        case ESP_GATTC_CFG_MTU_EVT:
            if (param->cfg_mtu.status == ESP_GATT_OK) {
                ESP_LOGI(TAG, "MTU configured: %d", param->cfg_mtu.mtu);
            }
            // Start service search
            esp_ble_gattc_search_service(gattc_if, ble_conn_id, NULL);
            break;

        case ESP_GATTC_SEARCH_RES_EVT: {
            esp_gatt_srvc_id_t *srvc_id = &param->search_res.srvc_id;
            
            // Check if this is our target service (128-bit UUID)
            if (srvc_id->id.uuid.len == ESP_UUID_LEN_128) {
                if (uuid_128_cmp(srvc_id->id.uuid.uuid.uuid128, target_service_uuid)) {
                    ESP_LOGI(TAG, "Target service found!");
                    ble_service_found = true;
                    ble_service_start_handle = param->search_res.start_handle;
                    ble_service_end_handle = param->search_res.end_handle;
                    
                    ESP_LOGI(TAG, "Service handles: start=0x%04x, end=0x%04x", 
                             ble_service_start_handle, ble_service_end_handle);
                }
            }
            break;
        }

        case ESP_GATTC_SEARCH_CMPL_EVT:
            ESP_LOGI(TAG, "Service search complete");
            
            if (ble_service_found) {
                // Get all characteristics in the service
                esp_ble_gattc_get_char_by_uuid(gattc_if, ble_conn_id,
                                               ble_service_start_handle,
                                               ble_service_end_handle,
                                               esp_ble_uuid_from_uint16(BLE_CHAR_UUID_16BIT),
                                               NULL, NULL);
            } else {
                ESP_LOGW(TAG, "Target service not found!");
            }
            break;

        case ESP_GATTC_GET_CHAR_EVT:
            if (param->get_char.status == ESP_GATT_OK) {
                // Check if this is our target characteristic (0xff01)
                if (param->get_char.char_id.uuid.len == ESP_UUID_LEN_16 &&
                    param->get_char.char_id.uuid.uuid.uuid16 == BLE_CHAR_UUID_16BIT) {
                    
                    ble_char_handle = param->get_char.char_handle;
                    ESP_LOGI(TAG, "Target characteristic 0x%04x found, handle: 0x%04x", 
                             BLE_CHAR_UUID_16BIT, ble_char_handle);
                    
                    ESP_LOGI(TAG, "BLE setup complete - ready to send commands!");
                }
            }
            break;

        case ESP_GATTC_WRITE_CHAR_EVT:
            if (param->write.status == ESP_GATT_OK) {
                ESP_LOGI(TAG, "Write successful");
            } else {
                ESP_LOGE(TAG, "Write failed, status: %d", param->write.status);
            }
            break;

        case ESP_GATTC_DISCONNECT_EVT:
            ble_connected = false;
            ble_service_found = false;
            ble_char_handle = 0;
            target_device_found = false;
            ESP_LOGI(TAG, "BLE Disconnected, reason: %d", param->disconnect.reason);
            
            // Auto-reconnect after 5 seconds
            vTaskDelay(pdMS_TO_TICKS(5000));
            ble_connect_to_device();
            break;

        default:
            break;
    }
}

// Initialize BLE Client
void ble_client_init(void)
{
    esp_err_t ret;

    // Initialize NVS
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize Bluetooth
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret) {
        ESP_LOGE(TAG, "Bluetooth controller init failed: %d", ret);
        return;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret) {
        ESP_LOGE(TAG, "Bluetooth controller enable failed: %d", ret);
        return;
    }

    ret = esp_bluedroid_init();
    if (ret) {
        ESP_LOGE(TAG, "Bluedroid init failed: %d", ret);
        return;
    }

    ret = esp_bluedroid_enable();
    if (ret) {
        ESP_LOGE(TAG, "Bluedroid enable failed: %d", ret);
        return;
    }

    // Register callbacks
    esp_ble_gap_register_callback(gap_event_handler);
    esp_ble_gattc_register_callback(gattc_event_handler);
    
    // Register GATT client app
    esp_ble_gattc_app_register(GATTC_APP_ID);

    ESP_LOGI(TAG, "BLE Client initialized");
}

// Start scanning and connect to device
void ble_connect_to_device(void)
{
    if (ble_connected) {
        ESP_LOGI(TAG, "Already connected to BLE device");
        return;
    }

    // Scan parameters
    static esp_ble_scan_params_t scan_params = {
        .scan_type = BLE_SCAN_TYPE_ACTIVE,
        .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
        .scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL,
        .scan_interval = 0x50,  // 50ms
        .scan_window = 0x30,    // 30ms
        .scan_duplicate = BLE_SCAN_DUPLICATE_DISABLE
    };

    esp_ble_gap_set_scan_params(&scan_params);
    esp_ble_gap_start_scanning(30); // Scan for 30 seconds
    ESP_LOGI(TAG, "Scanning for BLE device: %s", BLE_DEVICE_NAME);
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
