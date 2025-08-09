typedef struct {
    int key;
    spi_oled_animation_t* animation;
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
    .task_handle = NULL
};

spi_oled_animation_t anim_turn_right = {
    .x = 14,
    .y = 14,
    .width = 100,
    .height = 100,
    .frame_count = 22,
    .animation_data = (const uint8_t *)turn_left,
    .frame_delay_ms = 1000 / 30,
    .stop_frame = -1,
    .reverse = false,
    .task_handle = NULL
};


spi_oled_animation_t anim_go_straight = {
    .x = 14,
    .y = 14,
    .width = 100,
    .height = 100,
    .frame_count = 22,
    .animation_data = (const uint8_t *)turn_left,
    .frame_delay_ms = 1000 / 30,
    .stop_frame = -1,
    .reverse = false,
    .task_handle = NULL
};


static animation_map_entry_t animation_map[] = {
    {1, &anim_turn_left},
    {2, &anim_turn_right},
    {3, &anim_go_straight},
    {-1, NULL} // Sentinel to mark end of array
};

spi_oled_animation_t* get_animation_by_key(int key) {
    for (int i = 0; animation_map[i].key != -1; i++) {
        if (animation_map[i].key == key) {
            printf("map found: %d , width: %d , height: %d \n", animation_map[i].key,
                   animation_map[i].animation->width,
                   animation_map[i].animation->height);
            return animation_map[i].animation;
        }
    }
    printf("map not found");
    return NULL; // Not found
}
