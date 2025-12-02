#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

static const char *TAG = "SONY_CAM_BLE";

// Camera BLE name
#define BLE_SERVER_NAME "ZV-E1"

// Command arrays
static uint8_t PRESS_TO_FOCUS[] = {0x01, 0x07};
static uint8_t TAKE_PICTURE[] = {0x01, 0x09};
static uint8_t SHUTTER_RELEASED[] = {0x01, 0x06};
static uint8_t HOLD_FOCUS[] = {0x01, 0x08};

// Sony Camera BLE Service and Characteristics UUIDs
static ble_uuid128_t camera_service_uuid = 
    BLE_UUID128_INIT(0x80, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0xFF,
                     0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF);

static ble_uuid16_t command_char_uuid = BLE_UUID16_INIT(0xFF01);
static ble_uuid16_t notify_char_uuid = BLE_UUID16_INIT(0xFF02);

// Connection state
static bool is_connected = false;
static bool is_scanning = false;
static uint16_t conn_handle = 0;
static uint16_t command_char_val_handle = 0;
static uint16_t notify_char_val_handle = 0;
static ble_addr_t peer_addr;

// Helper function to print hex data
static void print_hex(const uint8_t *data, size_t length) {
    for (size_t i = 0; i < length; i++) {
        printf("0x%02X ", data[i]);
    }
    printf("\n");
}

// Write command to camera
static void write_command(uint8_t *data, size_t len) {
    if (!is_connected || command_char_val_handle == 0) {
        ESP_LOGW(TAG, "Not connected or characteristic not found");
        return;
    }

    struct os_mbuf *om = ble_hs_mbuf_from_flat(data, len);
    if (om == NULL) {
        ESP_LOGE(TAG, "Failed to allocate mbuf");
        return;
    }

    int rc = ble_gattc_write(conn_handle, command_char_val_handle, om,
                             NULL, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to write command: %d", rc);
    }
}

// GATT notification callback
static int ble_gap_notify_cb(uint16_t conn_handle,
                             uint16_t attr_handle,
                             struct ble_gatt_attr *attr,
                             void *arg) {
    ESP_LOGI(TAG, "Notification received from handle %d:", attr_handle);
    if (attr->om->om_len > 0) {
        print_hex(attr->om->om_data, attr->om->om_len);
    }
    return 0;
}

// Characteristic discovery callback
static int ble_chr_disc_cb(uint16_t conn_handle,
                          const struct ble_gatt_error *error,
                          const struct ble_gatt_chr *chr,
                          void *arg) {
    if (error->status == 0) {
        if (chr != NULL) {
            // Check if this is command characteristic
            if (ble_uuid_cmp(&chr->uuid.u, &command_char_uuid.u) == 0) {
                ESP_LOGI(TAG, "Command char found, handle: %d", chr->val_handle);
                command_char_val_handle = chr->val_handle;
                
                // Subscribe to notifications
                uint8_t value[2] = {0x01, 0x00};
                struct os_mbuf *om = ble_hs_mbuf_from_flat(value, sizeof(value));
                if (om != NULL) {
                    ble_gattc_write(conn_handle, chr->val_handle + 1, om, NULL, NULL);
                }
            }
            // Check if this is notify characteristic
            else if (ble_uuid_cmp(&chr->uuid.u, &notify_char_uuid.u) == 0) {
                ESP_LOGI(TAG, "Notify char found, handle: %d", chr->val_handle);
                notify_char_val_handle = chr->val_handle;
                
                // Subscribe to notifications
                uint8_t value[2] = {0x01, 0x00};
                struct os_mbuf *om = ble_hs_mbuf_from_flat(value, sizeof(value));
                if (om != NULL) {
                    ble_gattc_write(conn_handle, chr->val_handle + 1, om, NULL, NULL);
                }
            }
        }
    } else if (error->status == BLE_HS_EDONE) {
        ESP_LOGI(TAG, "Characteristic discovery complete");
        is_connected = true;
    } else {
        ESP_LOGE(TAG, "Characteristic discovery failed: %d", error->status);
    }
    
    return 0;
}

// Service discovery callback
static int ble_svc_disc_cb(uint16_t conn_handle,
                          const struct ble_gatt_error *error,
                          const struct ble_gatt_svc *service,
                          void *arg) {
    if (error->status == 0) {
        if (service != NULL) {
            // Check if this is the camera service
            if (ble_uuid_cmp(&service->uuid.u, &camera_service_uuid.u) == 0) {
                ESP_LOGI(TAG, "Camera service found!");
                
                // Discover characteristics
                ble_gattc_disc_all_chrs(conn_handle,
                                       service->start_handle,
                                       service->end_handle,
                                       ble_chr_disc_cb,
                                       NULL);
            }
        }
    } else if (error->status == BLE_HS_EDONE) {
        ESP_LOGI(TAG, "Service discovery complete");
    } else {
        ESP_LOGE(TAG, "Service discovery failed: %d", error->status);
    }
    
    return 0;
}

// GAP event handler
static int ble_gap_event(struct ble_gap_event *event, void *arg) {
    struct ble_gap_conn_desc desc;
    int rc;

    switch (event->type) {
    case BLE_GAP_EVENT_DISC:
        // Check device name
        if (event->disc.length_data > 0) {
            struct ble_hs_adv_fields fields;
            rc = ble_hs_adv_parse_fields(&fields, event->disc.data, 
                                        event->disc.length_data);
            
            if (rc == 0) {
                // Check complete name
                if (fields.name != NULL && fields.name_len == strlen(BLE_SERVER_NAME)) {
                    if (memcmp(fields.name, BLE_SERVER_NAME, fields.name_len) == 0) {
                        ESP_LOGI(TAG, "Camera found!");
                        
                        // Stop scanning and connect
                        ble_gap_disc_cancel();
                        is_scanning = false;
                        
                        memcpy(&peer_addr, &event->disc.addr, sizeof(ble_addr_t));
                        
                        rc = ble_gap_connect(BLE_OWN_ADDR_PUBLIC, &event->disc.addr,
                                           30000, NULL, ble_gap_event, NULL);
                        if (rc != 0) {
                            ESP_LOGE(TAG, "Failed to connect: %d", rc);
                        }
                    }
                }
            }
        }
        break;

    case BLE_GAP_EVENT_DISC_COMPLETE:
        ESP_LOGI(TAG, "Scan complete");
        is_scanning = false;
        if (!is_connected) {
            // Restart scan if not connected
            vTaskDelay(pdMS_TO_TICKS(1000));
            ble_gap_disc(BLE_OWN_ADDR_PUBLIC, 30000, NULL, ble_gap_event, NULL);
            is_scanning = true;
        }
        break;

    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            ESP_LOGI(TAG, "Connected to camera");
            conn_handle = event->connect.conn_handle;
            
            rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
            if (rc == 0) {
                ESP_LOGI(TAG, "Connection params: itvl=%d, latency=%d, timeout=%d",
                        desc.conn_itvl, desc.conn_latency, desc.supervision_timeout);
            }
            
            // Initiate pairing
            ESP_LOGI(TAG, "Initiating pairing...");
            rc = ble_gap_security_initiate(conn_handle);
            if (rc != 0) {
                ESP_LOGW(TAG, "Failed to initiate security: %d", rc);
            }
            
            // Discover services
            ble_gattc_disc_all_svcs(conn_handle, ble_svc_disc_cb, NULL);
        } else {
            ESP_LOGE(TAG, "Connection failed, status: %d", event->connect.status);
            // Restart scanning
            vTaskDelay(pdMS_TO_TICKS(1000));
            ble_gap_disc(BLE_OWN_ADDR_PUBLIC, 30000, NULL, ble_gap_event, NULL);
            is_scanning = true;
        }
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "Disconnected, reason: %d", event->disconnect.reason);
        is_connected = false;
        conn_handle = 0;
        command_char_val_handle = 0;
        notify_char_val_handle = 0;
        
        // Restart scanning
        vTaskDelay(pdMS_TO_TICKS(1000));
        ble_gap_disc(BLE_OWN_ADDR_PUBLIC, 30000, NULL, ble_gap_event, NULL);
        is_scanning = true;
        break;

    case BLE_GAP_EVENT_NOTIFY_RX:
        ESP_LOGI(TAG, "Notification received from handle %d:", 
                event->notify_rx.attr_handle);
        if (event->notify_rx.om->om_len > 0) {
            print_hex(event->notify_rx.om->om_data, 
                     event->notify_rx.om->om_len);
        }
        break;

    case BLE_GAP_EVENT_ENC_CHANGE:
        if (event->enc_change.status == 0) {
            ESP_LOGI(TAG, "Encryption established");
        } else {
            ESP_LOGE(TAG, "Encryption failed, status: %d", event->enc_change.status);
        }
        break;

    case BLE_GAP_EVENT_PASSKEY_ACTION:
        ESP_LOGI(TAG, "Passkey action event");
        struct ble_sm_io pkey = {0};
        
        if (event->passkey.params.action == BLE_SM_IOACT_NUMCMP) {
            ESP_LOGI(TAG, "Numeric comparison: %06" PRIu32, 
                    event->passkey.params.numcmp);
            pkey.action = event->passkey.params.action;
            pkey.numcmp_accept = 1;
            rc = ble_sm_inject_io(event->passkey.conn_handle, &pkey);
            if (rc != 0) {
                ESP_LOGE(TAG, "Failed to inject IO: %d", rc);
            }
        } else if (event->passkey.params.action == BLE_SM_IOACT_NONE) {
            pkey.action = event->passkey.params.action;
            rc = ble_sm_inject_io(event->passkey.conn_handle, &pkey);
        }
        break;

    case BLE_GAP_EVENT_MTU:
        ESP_LOGI(TAG, "MTU update: %d", event->mtu.value);
        break;

    default:
        break;
    }

    return 0;
}

// Start scanning
static void ble_start_scan(void) {
    if (!is_scanning) {
        struct ble_gap_disc_params disc_params = {
            .filter_duplicates = 0,
            .passive = 0,
            .itvl = 0,
            .window = 0,
            .filter_policy = 0,
            .limited = 0,
        };

        int rc = ble_gap_disc(BLE_OWN_ADDR_PUBLIC, 30000, &disc_params, 
                             ble_gap_event, NULL);
        if (rc != 0) {
            ESP_LOGE(TAG, "Failed to start scan: %d", rc);
        } else {
            ESP_LOGI(TAG, "Scan started");
            is_scanning = true;
        }
    }
}

// NimBLE host task
static void ble_host_task(void *param) {
    ESP_LOGI(TAG, "BLE Host Task Started");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

// Sync callback
static void ble_on_sync(void) {
    ESP_LOGI(TAG, "BLE stack synced");
    
    // Set device name
    int rc = ble_svc_gap_device_name_set("nkg RC demo v0.1");
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to set device name: %d", rc);
    }
    
    // Start scanning
    ble_start_scan();
}

// Reset callback
static void ble_on_reset(int reason) {
    ESP_LOGE(TAG, "BLE stack reset, reason: %d", reason);
}

void ble_client_init(void) {
    ESP_LOGI(TAG, "Sony Camera BLE Remote Controller - ESP-IDF v5.4 (NimBLE)");
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize NimBLE
    ret = nimble_port_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init nimble: %s", esp_err_to_name(ret));
        return;
    }

    // Configure NimBLE host
    ble_hs_cfg.sync_cb = ble_on_sync;
    ble_hs_cfg.reset_cb = ble_on_reset;

    // Set security parameters
    ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_NO_IO;
    ble_hs_cfg.sm_bonding = 1;
    ble_hs_cfg.sm_mitm = 1;
    ble_hs_cfg.sm_sc = 1;
    ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;

    // Initialize GAP and GATT services
    ble_svc_gap_init();
    ble_svc_gatt_init();

    // Start NimBLE host task
    nimble_port_freertos_init(ble_host_task);

    ESP_LOGI(TAG, "Initialization complete");
}