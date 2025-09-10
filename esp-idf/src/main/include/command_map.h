typedef struct
{
    int key;
    spi_oled_animation_t *animation;
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
    {1, &anim_turn_left},
    {2, &anim_turn_right},
    {3, &anim_go_straight},
    {4, &anim_time_out},
    {5, &anim_wait},
    {6, &anim_hello},
    {7, &anim_check_mark},
    {8, &anim_help_sos},
    {9, &anim_up_hill},
    {10, &anim_down_hill},
    {11, &anim_slow},
    {12, &anim_attention},
    {13, &anim_walk},
    {14, &anim_eat},
    {15, &anim_drink},
    {16, &anim_add_oil},
    {-1, NULL} // Sentinel to mark end of array
};

spi_oled_animation_t *get_animation_by_key(int key)
{
    for (int i = 0; animation_map[i].key != -1; i++)
    {
        if (animation_map[i].key == key)
        {
            printf("map found: %d , width: %d , height: %d \n", animation_map[i].key,
                   animation_map[i].animation->width,
                   animation_map[i].animation->height);
            return animation_map[i].animation;
        }
    }
    printf("map not found");
    return NULL; // Not found
}
