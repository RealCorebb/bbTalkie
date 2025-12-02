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

static uint8_t record_down[] = {0x01, 0x0f};
static uint8_t zoom_in[] = {0x02, 0x45, 0x80};
static uint8_t zoom_out[] = {0x02, 0x47, 0x80};


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
    {50, NULL, CMD_TYPE_BLE_SEND, record_down, sizeof(record_down)},
    {51, NULL, CMD_TYPE_BLE_SEND, zoom_in, sizeof(zoom_in)},
    {52, NULL, CMD_TYPE_BLE_SEND, zoom_out, sizeof(zoom_out)}
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