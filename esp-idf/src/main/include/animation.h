typedef struct
{
    uint8_t x, y, width, height;
    uint8_t frame_count;
    const uint8_t *animation_data;
    uint32_t frame_delay_ms;
    bool is_playing;
    int stop_frame;
    bool reverse;
    TaskHandle_t task_handle;
} spi_oled_animation_t;


spi_oled_animation_t anim = {
    .x = 10,
    .y = 35,
    .width = 54,
    .height = 41,
    .frame_count = 14,
    .animation_data = (const uint8_t *)idle_single,
    .frame_delay_ms = 1000 / 5,
    .stop_frame = -1,
    .reverse = false,
    .task_handle = NULL
};

spi_oled_animation_t anim_idleBar = {
    .x = 10,
    .y = 95,
    .width = 101,
    .height = 12,
    .frame_count = 14,
    .animation_data = (const uint8_t *)idle_bar,
    .frame_delay_ms = 1000 / 15,
    .stop_frame = -1,
    .reverse = false,
    .task_handle = NULL
};

spi_oled_animation_t anim_waveBar = {
    .x = 1,
    .y = 85,
    .width = 126,
    .height = 40,
    .frame_count = 29,
    .animation_data = (const uint8_t *)wave_bar,
    .frame_delay_ms = 1000 / 30,
    .stop_frame = 3,
    .reverse = false,
    .task_handle = NULL
};

spi_oled_animation_t anim_podcast = {
    .x = 74,
    .y = 38,
    .width = 36,
    .height = 36,
    .frame_count = 24,
    .animation_data = (const uint8_t *)podcast,
    .frame_delay_ms = 1000 / 30,
    .stop_frame = -1,
    .reverse = false,
    .task_handle = NULL
};

spi_oled_animation_t anim_speaker = {
    .x = 74,
    .y = 38,
    .width = 36,
    .height = 36,
    .frame_count = 46,
    .animation_data = (const uint8_t *)speaker,
    .frame_delay_ms = 1000 / 30,
    .stop_frame = -1,
    .reverse = false,
    .task_handle = NULL
};

spi_oled_animation_t anim_byebye = {
    .x = 0,
    .y = 0,
    .width = 128,
    .height = 128,
    .frame_count = 7,
    .animation_data = (const uint8_t *)byebye_image,
    .frame_delay_ms = 1000 / 15,
    .stop_frame = -1,
    .reverse = false,
    .task_handle = NULL
};

//custom map

