#include <inttypes.h>
#include "driver/spi_master.h"

// Display dimensions
#define SSD1327_WIDTH 128
#define SSD1327_HEIGHT 128
#define SSD1327_BUFFER_SIZE ((SSD1327_WIDTH * SSD1327_HEIGHT) / 2) // 4bpp = 0.5 bytes per pixel
#define MAX_BITS_PER_TRANSFER 4096

struct spi_ssd1327
{
    uint8_t dc_pin_num;
    uint8_t rst_pin_num;
    spi_device_handle_t *spi_handle;
    uint8_t *framebuffer; // Frame buffer for 4bpp grayscale (8192 bytes)
    bool auto_refresh;    // Auto refresh display after drawing operations
};

typedef enum
{
    SSD1327_GS_0 = 0,
    SSD1327_GS_1 = 1,
    SSD1327_GS_2 = 2,
    SSD1327_GS_3 = 3,
    SSD1327_GS_4 = 4,
    SSD1327_GS_5 = 5,
    SSD1327_GS_6 = 6,
    SSD1327_GS_7 = 7,
    SSD1327_GS_8 = 8,
    SSD1327_GS_9 = 9,
    SSD1327_GS_10 = 10,
    SSD1327_GS_11 = 11,
    SSD1327_GS_12 = 12,
    SSD1327_GS_13 = 13,
    SSD1327_GS_14 = 14,
    SSD1327_GS_15 = 15,
} ssd1327_gs_t;

typedef struct
{
    uint8_t height;
    const uint8_t *widths;
    const uint16_t *offsets;
    const uint8_t *data;
} variable_font_t;

// Core initialization and communication functions
void spi_oled_init(struct spi_ssd1327 *spi_ssd1327);
void spi_oled_deinit(struct spi_ssd1327 *spi_ssd1327);
void spi_oled_reset(struct spi_ssd1327 *spi_ssd1327);
void spi_oled_send_cmd(struct spi_ssd1327 *spi_ssd1327, uint8_t cmd);
void spi_oled_send_cmd_arg(struct spi_ssd1327 *spi_ssd1327, uint8_t cmd, uint8_t arg);
void spi_oled_send_data(struct spi_ssd1327 *spi_ssd1327, void *data, uint32_t data_len_bits);

// Frame buffer management functions
bool spi_oled_framebuffer_init(struct spi_ssd1327 *spi_ssd1327);
void spi_oled_framebuffer_free(struct spi_ssd1327 *spi_ssd1327);
void spi_oled_framebuffer_clear(struct spi_ssd1327 *spi_ssd1327, ssd1327_gs_t color);
void spi_oled_framebuffer_refresh(struct spi_ssd1327 *spi_ssd1327);
void spi_oled_framebuffer_refresh_region(struct spi_ssd1327 *spi_ssd1327,
                                         uint8_t x, uint8_t y,
                                         uint8_t width, uint8_t height);

// Drawing functions (all operate on frame buffer)
void spi_oled_set_pixel(struct spi_ssd1327 *spi_ssd1327, uint8_t x, uint8_t y, ssd1327_gs_t gs);
ssd1327_gs_t spi_oled_get_pixel(struct spi_ssd1327 *spi_ssd1327, uint8_t x, uint8_t y);
void spi_oled_clear_region(struct spi_ssd1327 *spi_ssd1327, uint8_t x, uint8_t y,
                           uint8_t width, uint8_t height);
void spi_oled_draw_square(struct spi_ssd1327 *spi_ssd1327, uint8_t x, uint8_t y,
                          uint8_t width, uint8_t height, ssd1327_gs_t gs);
void spi_oled_draw_circle(struct spi_ssd1327 *spi_ssd1327, uint8_t x, uint8_t y,
                          uint8_t radius, ssd1327_gs_t gs);
void spi_oled_draw_line(struct spi_ssd1327 *spi_ssd1327, uint8_t x0, uint8_t y0,
                        uint8_t x1, uint8_t y1, ssd1327_gs_t gs);

// Text drawing functions
void spi_oled_drawText(struct spi_ssd1327 *spi_ssd1327, uint8_t x, uint8_t y,
                       const variable_font_t *font, ssd1327_gs_t gs, const char *text);

// Simplified shadow text - now just draws text twice with offset
void spi_oled_drawTextWithShadow(struct spi_ssd1327 *spi_ssd1327, uint8_t x, uint8_t y,
                                 const variable_font_t *font, ssd1327_gs_t gs,
                                 ssd1327_gs_t gs_shadow, int8_t offset_x, int8_t offset_y,
                                 const char *text);

// Image drawing function
void spi_oled_drawImage(struct spi_ssd1327 *spi_ssd1327, uint8_t x, uint8_t y,
                        uint8_t width, uint8_t height, const uint8_t *image);

// Utility functions
void spi_oled_set_auto_refresh(struct spi_ssd1327 *spi_ssd1327, bool auto_refresh);
uint16_t spi_oled_get_text_width(const variable_font_t *font, const char *text);

// Animation structure (unchanged)
typedef struct
{
    struct spi_ssd1327 *spi_ssd1327;
    uint8_t x, y, width, height;
    uint8_t frame_count;
    const uint8_t *animation_data;
    uint32_t frame_delay_ms;
    bool is_playing;
    uint8_t stop_frame;
    bool reverse;
    TaskHandle_t task_handle;
} spi_oled_animation_t;