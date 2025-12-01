/* BLE */
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "host/ble_store.h"
#include "services/gap/ble_svc_gap.h"

/* Forward declaration - ble_store_config_init is not in header in v5.4 */
void ble_store_config_init(void);

static const char *TAG_BLE = "ZVE1_CENTRAL";

/* Target device name */
#define TARGET_DEVICE_NAME "ZV-E1"

/* Connection handle */
static uint16_t conn_handle = 0;
static bool is_connected = false;
static bool is_encrypted = false;

/* Function declarations */
static void ble_central_scan(void);

// ===== BLE Commands =====
static uint8_t cmd_focus_down[] = {0x01, 0x07};
static uint8_t TAKE_PICTURE[] = {0x01, 0x09};
static uint8_t SHUTTER_RELEASED[] = {0x01, 0x06};
static uint8_t HOLD_FOCUS[] = {0x01, 0x08};

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

/**
 * @brief Check if the discovered device is the one we're looking for
 */
static bool should_connect_to_device(const struct ble_hs_adv_fields *fields)
{
    /* Check if device name matches our target */
    if (fields->name != NULL && fields->name_len > 0) {
        if (memcmp(fields->name, TARGET_DEVICE_NAME, fields->name_len) == 0 &&
            fields->name_len == strlen(TARGET_DEVICE_NAME)) {
            ESP_LOGI(TAG_BLE, "Found target device: %s", TARGET_DEVICE_NAME);
            return true;
        }
    }
    return false;
}

/**
 * @brief Print stored bond information (for debugging)
 */
static void print_bonded_devices(void)
{
    ble_addr_t peer_id_addrs[CONFIG_BT_NIMBLE_MAX_BONDS];
    int num_peers;
    int rc;

    rc = ble_store_util_bonded_peers(&peer_id_addrs[0], &num_peers, 
                                     CONFIG_BT_NIMBLE_MAX_BONDS);
    if (rc != 0) {
        ESP_LOGE(TAG_BLE, "Failed to retrieve bonded peers; rc=%d", rc);
        return;
    }

    if (num_peers == 0) {
        ESP_LOGI(TAG_BLE, "No bonded devices");
        return;
    }

    ESP_LOGI(TAG_BLE, "Bonded devices (%d):", num_peers);
    for (int i = 0; i < num_peers; i++) {
        ESP_LOGI(TAG_BLE, "  %d: %02x:%02x:%02x:%02x:%02x:%02x (type=%d)",
                 i,
                 peer_id_addrs[i].val[0], peer_id_addrs[i].val[1],
                 peer_id_addrs[i].val[2], peer_id_addrs[i].val[3],
                 peer_id_addrs[i].val[4], peer_id_addrs[i].val[5],
                 peer_id_addrs[i].type);
    }
}

/**
 * @brief GAP event handler
 */
static int ble_gap_event_handler(struct ble_gap_event *event, void *arg)
{
    struct ble_gap_conn_desc desc;
    int rc;

    switch (event->type) {
    
    case BLE_GAP_EVENT_DISC:
        /* A new device was discovered during scanning */
        struct ble_hs_adv_fields fields;
        rc = ble_hs_adv_parse_fields(&fields, event->disc.data, event->disc.length_data);
        if (rc != 0) {
            return 0;
        }

        /* Check if this is our target device */
        if (should_connect_to_device(&fields)) {
            /* Stop scanning before connecting */
            ble_gap_disc_cancel();
            
            /* Attempt to connect */
            ESP_LOGI(TAG_BLE, "Connecting to %s...", TARGET_DEVICE_NAME);
            rc = ble_gap_connect(BLE_OWN_ADDR_PUBLIC, &event->disc.addr,
                                30000, NULL, ble_gap_event_handler, NULL);
            if (rc != 0) {
                ESP_LOGE(TAG_BLE, "Error initiating connection; rc=%d", rc);
                /* Resume scanning */
                ble_central_scan();
            }
        }
        return 0;

    case BLE_GAP_EVENT_CONNECT:
        /* Connection attempt completed */
        if (event->connect.status == 0) {
            /* Connection successful */
            ESP_LOGI(TAG_BLE, "Connection established!");
            
            rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
            if (rc == 0) {
                ESP_LOGI(TAG_BLE, "Connection handle: %d", event->connect.conn_handle);
                ESP_LOGI(TAG_BLE, "Peer address: %02x:%02x:%02x:%02x:%02x:%02x (type=%d)",
                         desc.peer_id_addr.val[0], desc.peer_id_addr.val[1],
                         desc.peer_id_addr.val[2], desc.peer_id_addr.val[3],
                         desc.peer_id_addr.val[4], desc.peer_id_addr.val[5],
                         desc.peer_id_addr.type);
            }
            
            conn_handle = event->connect.conn_handle;
            is_connected = true;
            
            /* Initiate pairing/security procedure */
            ESP_LOGI(TAG_BLE, "Initiating security procedure (pairing)...");
            rc = ble_gap_security_initiate(conn_handle);
            if (rc != 0) {
                ESP_LOGE(TAG_BLE, "Failed to initiate security; rc=%d", rc);
            }
            
        } else {
            /* Connection failed */
            ESP_LOGE(TAG_BLE, "Connection failed; status=%d", event->connect.status);
            /* Resume scanning */
            ble_central_scan();
        }
        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        /* Disconnection event */
        ESP_LOGI(TAG_BLE, "Disconnected; reason=%d", event->disconnect.reason);
        is_connected = false;
        is_encrypted = false;
        conn_handle = 0;
        
        /* Print current bonds */
        print_bonded_devices();
        
        /* Resume scanning to reconnect */
        ble_central_scan();
        return 0;

    case BLE_GAP_EVENT_ENC_CHANGE:
        /* Encryption state changed */
        ESP_LOGI(TAG_BLE, "Encryption change event; status=%d", event->enc_change.status);
        
        rc = ble_gap_conn_find(event->enc_change.conn_handle, &desc);
        if (rc == 0) {
            ESP_LOGI(TAG_BLE, "Connection security status:");
            ESP_LOGI(TAG_BLE, "  encrypted=%d authenticated=%d bonded=%d key_size=%d",
                     desc.sec_state.encrypted,
                     desc.sec_state.authenticated,
                     desc.sec_state.bonded,
                     desc.sec_state.key_size);
            
            if (desc.sec_state.encrypted) {
                is_encrypted = true;
                ESP_LOGI(TAG_BLE, "Connection is now encrypted and secure!");
                
                /* Now you can perform GATT operations on encrypted characteristics */
                /* Example: discover services, read/write encrypted characteristics */
            }
        }
        return 0;

    case BLE_GAP_EVENT_PASSKEY_ACTION:
        /* Passkey action required during pairing */
        ESP_LOGI(TAG_BLE, "Passkey action event; action=%d", event->passkey.params.action);
        
        struct ble_sm_io pkey = {0};
        
        if (event->passkey.params.action == BLE_SM_IOACT_DISP) {
            /* Display passkey */
            pkey.action = event->passkey.params.action;
            pkey.passkey = 123456; /* You can generate a random 6-digit passkey */
            ESP_LOGI(TAG_BLE, "Enter passkey on peer device: %06lu", 
                     (unsigned long)pkey.passkey);
            rc = ble_sm_inject_io(event->passkey.conn_handle, &pkey);
            ESP_LOGI(TAG_BLE, "ble_sm_inject_io result: %d", rc);
            
        } else if (event->passkey.params.action == BLE_SM_IOACT_NUMCMP) {
            /* Numeric comparison */
            ESP_LOGI(TAG_BLE, "Passkey on peer device: %06lu", 
                     (unsigned long)event->passkey.params.numcmp);
            ESP_LOGI(TAG_BLE, "Accept pairing? Confirming YES automatically...");
            pkey.action = event->passkey.params.action;
            pkey.numcmp_accept = 1; /* Auto-accept for this example */
            rc = ble_sm_inject_io(event->passkey.conn_handle, &pkey);
            ESP_LOGI(TAG_BLE, "ble_sm_inject_io result: %d", rc);
            
        } else if (event->passkey.params.action == BLE_SM_IOACT_INPUT) {
            /* Input passkey (displayed on peer) */
            ESP_LOGI(TAG_BLE, "Enter passkey displayed on peer device");
            /* In a real application, get input from user */
            /* For this example, we'll use a fixed passkey */
            pkey.action = event->passkey.params.action;
            pkey.passkey = 0; /* Replace with actual passkey from peer */
            rc = ble_sm_inject_io(event->passkey.conn_handle, &pkey);
            ESP_LOGI(TAG_BLE, "ble_sm_inject_io result: %d", rc);
            
        } else if (event->passkey.params.action == BLE_SM_IOACT_OOB) {
            /* Out of band pairing */
            ESP_LOGI(TAG_BLE, "OOB pairing requested (not implemented)");
        }
        return 0;

    case BLE_GAP_EVENT_REPEAT_PAIRING:
        /* Repeat pairing attempt - delete old bond and retry */
        ESP_LOGI(TAG_BLE, "Repeat pairing event - deleting old bond");
        
        rc = ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc);
        if (rc == 0) {
            /* Delete bond for this peer */
            rc = ble_store_util_delete_peer(&desc.peer_id_addr);
            if (rc == 0) {
                ESP_LOGI(TAG_BLE, "Old bond deleted successfully");
            }
        }
        /* Allow new pairing to proceed */
        return BLE_GAP_REPEAT_PAIRING_RETRY;

    case BLE_GAP_EVENT_DISC_COMPLETE:
        /* Scanning completed */
        ESP_LOGI(TAG_BLE, "Discovery complete; reason=%d", event->disc_complete.reason);
        /* Restart scanning if not connected */
        if (!is_connected) {
            ble_central_scan();
        }
        return 0;

    case BLE_GAP_EVENT_NOTIFY_RX:
        /* Notification received */
        ESP_LOGI(TAG_BLE, "Notification received; conn_handle=%d attr_handle=%d",
                 event->notify_rx.conn_handle, event->notify_rx.attr_handle);
        return 0;

    case BLE_GAP_EVENT_MTU:
        /* MTU update event */
        ESP_LOGI(TAG_BLE, "MTU update; conn_handle=%d mtu=%d",
                 event->mtu.conn_handle, event->mtu.value);
        return 0;

    default:
        ESP_LOGD(TAG_BLE, "Other GAP event: %d", event->type);
        return 0;
    }
}

/**
 * @brief Start BLE scanning
 */
static void ble_central_scan(void)
{
    uint8_t own_addr_type;
    struct ble_gap_disc_params disc_params;
    int rc;

    /* Determine own address type */
    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG_BLE, "Error determining address type; rc=%d", rc);
        return;
    }

    /* Set scan parameters */
    memset(&disc_params, 0, sizeof(disc_params));
    disc_params.filter_duplicates = 1;
    disc_params.passive = 0;           /* Active scanning */
    disc_params.itvl = 0;              /* Use default interval */
    disc_params.window = 0;            /* Use default window */
    disc_params.filter_policy = 0;     /* No filter */
    disc_params.limited = 0;           /* General discovery */

    /* Start scanning */
    rc = ble_gap_disc(own_addr_type, BLE_HS_FOREVER, &disc_params,
                      ble_gap_event_handler, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG_BLE, "Error initiating GAP discovery; rc=%d", rc);
        return;
    }

    ESP_LOGI(TAG_BLE, "Scanning for %s...", TARGET_DEVICE_NAME);
}

/**
 * @brief Callback when NimBLE host syncs with controller
 */
static void ble_on_sync(void)
{
    int rc;

    /* Ensure we have an identity address */
    rc = ble_hs_util_ensure_addr(0);
    if (rc != 0) {
        ESP_LOGE(TAG_BLE, "Error ensuring address; rc=%d", rc);
        return;
    }

    /* Print currently bonded devices */
    print_bonded_devices();

    /* Start scanning */
    ble_central_scan();
}

/**
 * @brief Callback when NimBLE host resets
 */
static void ble_on_reset(int reason)
{
    ESP_LOGE(TAG_BLE, "BLE host reset; reason=%d", reason);
}

/**
 * @brief NimBLE host configuration
 */
static void ble_host_config(void)
{
    int rc;
    
    /* Set GAP callbacks */
    ble_hs_cfg.reset_cb = ble_on_reset;
    ble_hs_cfg.sync_cb = ble_on_sync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    
    /* Security Manager (SM) configuration */
    ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_DISP_YES_NO;  /* Can display and input */
    ble_hs_cfg.sm_bonding = 1;                          /* Enable bonding */
    ble_hs_cfg.sm_mitm = 1;                             /* Man-in-the-middle protection */
    ble_hs_cfg.sm_sc = 1;                               /* Secure Connections (LE SC) */
    
    /* Key distribution - specify which keys to distribute */
    ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    
    /* Set device name */
    rc = ble_svc_gap_device_name_set("ESP32-Central");
    if (rc != 0) {
        ESP_LOGE(TAG_BLE, "Error setting device name; rc=%d", rc);
    }
    
    /* Initialize bond storage */
    ble_store_config_init();
}

/**
 * @brief NimBLE host task
 */
static void ble_host_task(void *param)
{
    ESP_LOGI(TAG_BLE, "BLE Host Task Started");
    
    /* This function will return only when nimble_port_stop() is executed */
    nimble_port_run();
    
    nimble_port_freertos_deinit();
}

static void ble_client_init()
{
    int rc;
    esp_err_t ret;
    /* Initialize NimBLE host stack */
    ret = nimble_port_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG_BLE, "Failed to init nimble; error=%d", ret);
        return;
    }

    /* Configure NimBLE host */
    ble_host_config();

    /* Start NimBLE host task */
    nimble_port_freertos_init(ble_host_task);
}