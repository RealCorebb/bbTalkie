#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "esp_gatt_common_api.h"

static const char *TAG = "SONY_CAM_BLE";

// Camera BLE name
#define BLE_SERVER_NAME "ZV-E10M2"
#define PROFILE_NUM 1
#define PROFILE_APP_ID 0
#define INVALID_HANDLE 0

// Command arrays
static uint8_t PRESS_TO_FOCUS[] = {0x01, 0x07};
static uint8_t TAKE_PICTURE[] = {0x01, 0x09};
static uint8_t SHUTTER_RELEASED[] = {0x01, 0x06};
static uint8_t HOLD_FOCUS[] = {0x01, 0x08};

// Sony Camera BLE Service and Characteristics UUIDs
static uint8_t camera_service_uuid[16] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
                                          0xFF, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0x80};
static uint16_t command_char_uuid = 0xFF01;
static uint16_t notify_char_uuid = 0xFF02;

// Connection state
static bool is_connected = false;
static bool get_service = false;
static esp_gattc_char_elem_t command_char;
static esp_gattc_char_elem_t notify_char;
static esp_bd_addr_t remote_bda;
static esp_gatt_if_t gl_profile_gattc_if = ESP_GATT_IF_NONE;
static uint16_t gl_conn_id = 0;

// Structure to hold profile info
struct gattc_profile_inst {
    esp_gattc_cb_t gattc_cb;
    uint16_t gattc_if;
    uint16_t app_id;
    uint16_t conn_id;
    uint16_t service_start_handle;
    uint16_t service_end_handle;
    esp_bd_addr_t remote_bda;
};

static struct gattc_profile_inst gl_profile_tab[PROFILE_NUM] = {
    [PROFILE_APP_ID] = {
        .gattc_cb = NULL,
        .gattc_if = ESP_GATT_IF_NONE,
    },
};

// Helper function to print hex data
static void print_hex(const uint8_t *data, size_t length) {
    for (size_t i = 0; i < length; i++) {
        printf("0x%02X ", data[i]);
    }
    printf("\n");
}

// Write command to camera
static void write_command(uint8_t *data, size_t len) {
    if (!is_connected || command_char.char_handle == 0) {
        ESP_LOGW(TAG, "Not connected or characteristic not found");
        return;
    }

    esp_err_t ret = esp_ble_gattc_write_char(
        gl_profile_gattc_if,
        gl_conn_id,
        command_char.char_handle,
        len,
        data,
        ESP_GATT_WRITE_TYPE_RSP,
        ESP_GATT_AUTH_REQ_NONE
    );
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write command: %d", ret);
    }
}


// Register notifications for characteristics
static void register_for_notify(esp_gatt_if_t gattc_if, uint16_t conn_id, uint16_t char_handle) {
    esp_err_t ret = esp_ble_gattc_register_for_notify(gattc_if, remote_bda, char_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Register for notify failed: %d", ret);
    }
}

// GATT client event handler
static void gattc_profile_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param) {
    switch (event) {
    case ESP_GATTC_REG_EVT:
        ESP_LOGI(TAG, "GATTC_REG_EVT");
        gl_profile_gattc_if = gattc_if;
        
        // Set scan parameters
        esp_ble_gap_set_scan_params(&(esp_ble_scan_params_t){
            .scan_type = BLE_SCAN_TYPE_ACTIVE,
            .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
            .scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL,
            .scan_interval = 0x50,
            .scan_window = 0x30,
            .scan_duplicate = BLE_SCAN_DUPLICATE_DISABLE
        });
        break;

    case ESP_GATTC_OPEN_EVT:
        if (param->open.status != ESP_GATT_OK) {
            ESP_LOGE(TAG, "Open failed, status %d", param->open.status);
            break;
        }
        ESP_LOGI(TAG, "Connected to camera");
        gl_conn_id = param->open.conn_id;
        memcpy(gl_profile_tab[PROFILE_APP_ID].remote_bda, param->open.remote_bda, sizeof(esp_bd_addr_t));

        ESP_LOGI(TAG, "Initiating pairing...");
        esp_ble_set_encryption(param->open.remote_bda, ESP_BLE_SEC_ENCRYPT_MITM);
        
        // Search for camera service
        esp_ble_gattc_search_service(gattc_if, param->open.conn_id, NULL);
        break;

    case ESP_GATTC_SEARCH_RES_EVT:
        ESP_LOGI(TAG, "Service search result");
        if (memcmp(param->search_res.srvc_id.uuid.uuid.uuid128, camera_service_uuid, 16) == 0) {
            ESP_LOGI(TAG, "Camera service found!");
            get_service = true;
            gl_profile_tab[PROFILE_APP_ID].service_start_handle = param->search_res.start_handle;
            gl_profile_tab[PROFILE_APP_ID].service_end_handle = param->search_res.end_handle;
        }
        break;

    case ESP_GATTC_SEARCH_CMPL_EVT:
        if (param->search_cmpl.status != ESP_GATT_OK) {
            ESP_LOGE(TAG, "Search complete error, status %d", param->search_cmpl.status);
            break;
        }
        if (get_service) {
            ESP_LOGI(TAG, "Getting characteristics...");
            
            // Get command characteristic
            uint16_t count = 1;
            esp_gatt_status_t status = esp_ble_gattc_get_char_by_uuid(
                gattc_if,
                param->search_cmpl.conn_id,
                gl_profile_tab[PROFILE_APP_ID].service_start_handle,
                gl_profile_tab[PROFILE_APP_ID].service_end_handle,
                (esp_bt_uuid_t){
                    .len = ESP_UUID_LEN_16,
                    .uuid = {.uuid16 = command_char_uuid}
                },
                &command_char,
                &count
            );
            
            if (status == ESP_GATT_OK && count > 0) {
                ESP_LOGI(TAG, "Command char found, handle: %d", command_char.char_handle);
                register_for_notify(gattc_if, param->search_cmpl.conn_id, command_char.char_handle);
            }
            
            // Get notify characteristic
            count = 1;
            status = esp_ble_gattc_get_char_by_uuid(
                gattc_if,
                param->search_cmpl.conn_id,
                gl_profile_tab[PROFILE_APP_ID].service_start_handle,
                gl_profile_tab[PROFILE_APP_ID].service_end_handle,
                (esp_bt_uuid_t){
                    .len = ESP_UUID_LEN_16,
                    .uuid = {.uuid16 = notify_char_uuid}
                },
                &notify_char,
                &count
            );
            
            if (status == ESP_GATT_OK && count > 0) {
                ESP_LOGI(TAG, "Notify char found, handle: %d", notify_char.char_handle);
                register_for_notify(gattc_if, param->search_cmpl.conn_id, notify_char.char_handle);
            }
            
            is_connected = true;
        }
        break;

    case ESP_GATTC_REG_FOR_NOTIFY_EVT:
        ESP_LOGI(TAG, "Register for notify, status: %d", param->reg_for_notify.status);
        if (param->reg_for_notify.status == ESP_GATT_OK) {
            // Write descriptor to enable notifications
            uint16_t notify_en = 1;
            esp_ble_gattc_write_char_descr(
                gattc_if,
                gl_conn_id,
                param->reg_for_notify.handle + 1, // CCCD is typically handle + 1
                sizeof(notify_en),
                (uint8_t *)&notify_en,
                ESP_GATT_WRITE_TYPE_RSP,
                ESP_GATT_AUTH_REQ_NONE
            );
        }
        break;

    case ESP_GATTC_NOTIFY_EVT:
        ESP_LOGI(TAG, "Notification received from handle %d:", param->notify.handle);
        if (param->notify.value_len > 0) {
            print_hex(param->notify.value, param->notify.value_len);
        }
        break;

    case ESP_GATTC_WRITE_CHAR_EVT:
        if (param->write.status != ESP_GATT_OK) {
            ESP_LOGE(TAG, "Write char failed, status %d", param->write.status);
        }
        break;

    case ESP_GATTC_DISCONNECT_EVT:
        ESP_LOGI(TAG, "Disconnected, reason: %d", param->disconnect.reason);
        is_connected = false;
        get_service = false;
        gl_conn_id = 0;
        memset(&command_char, 0, sizeof(command_char));
        memset(&notify_char, 0, sizeof(notify_char));
        
        // Restart scanning
        esp_ble_gap_start_scanning(30);
        break;

    default:
        break;
    }
}

// GAP event handler
static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    switch (event) {
    case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT:
        ESP_LOGI(TAG, "Scan parameters set, starting scan...");
        esp_ble_gap_start_scanning(30);
        break;

    case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
        if (param->scan_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(TAG, "Scan start failed, status %d", param->scan_start_cmpl.status);
        } else {
            ESP_LOGI(TAG, "Scan started successfully");
        }
        break;

    case ESP_GAP_BLE_SCAN_RESULT_EVT: {
        esp_ble_gap_cb_param_t *scan_result = param;
        switch (scan_result->scan_rst.search_evt) {
        case ESP_GAP_SEARCH_INQ_RES_EVT:
            // Check device name
            uint8_t *adv_name = NULL;
            uint8_t adv_name_len = 0;
            adv_name = esp_ble_resolve_adv_data(scan_result->scan_rst.ble_adv,
                                               ESP_BLE_AD_TYPE_NAME_CMPL,
                                               &adv_name_len);
            
            if (adv_name != NULL && adv_name_len == strlen(BLE_SERVER_NAME)) {
                if (strncmp((char *)adv_name, BLE_SERVER_NAME, adv_name_len) == 0) {
                    ESP_LOGI(TAG, "Camera found!");
                    
                    // Check pairing status from advertising data
                    for (int i = 1; i < scan_result->scan_rst.adv_data_len; i++) {
                        if (scan_result->scan_rst.ble_adv[i-1] == 0x22) {
                            if ((scan_result->scan_rst.ble_adv[i] & 0x40) && 
                                (scan_result->scan_rst.ble_adv[i] & 0x02)) {
                                ESP_LOGI(TAG, "Camera is ready for pairing");
                            } else {
                                ESP_LOGI(TAG, "Camera not in pairing mode, will try to connect");
                            }
                            break;
                        }
                    }
                    
                    // Stop scanning and connect
                    esp_ble_gap_stop_scanning();
                    memcpy(remote_bda, scan_result->scan_rst.bda, sizeof(esp_bd_addr_t));
                    esp_ble_gattc_open(gl_profile_gattc_if, remote_bda, 
                                      scan_result->scan_rst.ble_addr_type, true);
                }
            }
            break;

        case ESP_GAP_SEARCH_INQ_CMPL_EVT:
            ESP_LOGI(TAG, "Scan complete");
            if (!is_connected) {
                // Restart scan if not connected
                vTaskDelay(pdMS_TO_TICKS(1000));
                esp_ble_gap_start_scanning(30);
            }
            break;

        default:
            break;
        }
        break;
    }

    case ESP_GAP_BLE_AUTH_CMPL_EVT:
        if (param->ble_security.auth_cmpl.success) {
            ESP_LOGI(TAG, "Authentication success");
        } else {
            ESP_LOGE(TAG, "Authentication failed, fail reason: 0x%x", 
                    param->ble_security.auth_cmpl.fail_reason);
        }
        break;

    case ESP_GAP_BLE_PASSKEY_REQ_EVT:
        ESP_LOGI(TAG, "Passkey request");
        esp_ble_passkey_reply(param->ble_security.ble_req.bd_addr, true, 0);
        break;

    case ESP_GAP_BLE_NC_REQ_EVT:
        ESP_LOGI(TAG, "Numeric comparison request: %06" PRIu32, param->ble_security.key_notif.passkey);
        esp_ble_confirm_reply(param->ble_security.key_notif.bd_addr, true);
        break;

    case ESP_GAP_BLE_SEC_REQ_EVT:
        ESP_LOGI(TAG, "Security request");
        esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true);
        break;

    default:
        break;
    }
}

// GATT client callback
static void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param) {
    if (event == ESP_GATTC_REG_EVT) {
        if (param->reg.status == ESP_GATT_OK) {
            gl_profile_tab[param->reg.app_id].gattc_if = gattc_if;
        } else {
            ESP_LOGE(TAG, "Reg app failed, app_id %04x, status %d",
                    param->reg.app_id, param->reg.status);
            return;
        }
    }

    // Call profile event handler
    for (int idx = 0; idx < PROFILE_NUM; idx++) {
        if (gattc_if == ESP_GATT_IF_NONE || gattc_if == gl_profile_tab[idx].gattc_if) {
            if (gl_profile_tab[idx].gattc_cb) {
                gl_profile_tab[idx].gattc_cb(event, gattc_if, param);
            }
        }
    }
}


void ble_client_init(void) {
    ESP_LOGI(TAG, "Sony Camera BLE Remote Controller - ESP-IDF v5.4 (Bluedroid)");
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Release BT Classic memory (we only need BLE)
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    // Initialize Bluetooth controller
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret) {
        ESP_LOGE(TAG, "Bluetooth controller init failed: %s", esp_err_to_name(ret));
        return;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret) {
        ESP_LOGE(TAG, "Bluetooth controller enable failed: %s", esp_err_to_name(ret));
        return;
    }

    // Initialize Bluedroid
    ret = esp_bluedroid_init();
    if (ret) {
        ESP_LOGE(TAG, "Bluedroid init failed: %s", esp_err_to_name(ret));
        return;
    }

    ret = esp_bluedroid_enable();
    if (ret) {
        ESP_LOGE(TAG, "Bluedroid enable failed: %s", esp_err_to_name(ret));
        return;
    }

    // Register callbacks
    ret = esp_ble_gap_register_callback(gap_event_handler);
    if (ret) {
        ESP_LOGE(TAG, "GAP callback register failed: %s", esp_err_to_name(ret));
        return;
    }

    ret = esp_ble_gattc_register_callback(gattc_event_handler);
    if (ret) {
        ESP_LOGE(TAG, "GATTC callback register failed: %s", esp_err_to_name(ret));
        return;
    }

    // Register GATT client app
    ret = esp_ble_gattc_app_register(PROFILE_APP_ID);
    if (ret) {
        ESP_LOGE(TAG, "GATTC app register failed: %s", esp_err_to_name(ret));
        return;
    }

    // Set up security
    esp_ble_auth_req_t auth_req = ESP_LE_AUTH_REQ_SC_MITM_BOND;
    esp_ble_io_cap_t iocap = ESP_IO_CAP_NONE;
    uint8_t key_size = 16;
    uint8_t init_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    uint8_t rsp_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    uint32_t passkey = 123456;
    uint8_t auth_option = ESP_BLE_ONLY_ACCEPT_SPECIFIED_AUTH_DISABLE;
    uint8_t oob_support = ESP_BLE_OOB_DISABLE;

    esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &key_size, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &init_key, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &rsp_key, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_STATIC_PASSKEY, &passkey, sizeof(uint32_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_ONLY_ACCEPT_SPECIFIED_SEC_AUTH, &auth_option, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_OOB_SUPPORT, &oob_support, sizeof(uint8_t));

    // Set device name
    esp_ble_gap_set_device_name("nkg RC demo v0.1");

    // Register profile callback
    gl_profile_tab[PROFILE_APP_ID].gattc_cb = gattc_profile_event_handler;

    ESP_LOGI(TAG, "Initialization complete");
}