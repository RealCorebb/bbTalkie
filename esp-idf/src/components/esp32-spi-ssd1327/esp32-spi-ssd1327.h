#include <inttypes.h>

#include "driver/spi_master.h"

struct spi_ssd1327
{
    uint8_t dc_pin_num;
    uint8_t rst_pin_num;
    spi_device_handle_t *spi_handle;
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

void spi_oled_init(struct spi_ssd1327 *spi_ssd1327);

void spi_oled_deinit(struct spi_ssd1327 *spi_ssd1327);

void spi_oled_reset(struct spi_ssd1327 *spi_ssd1327);

void spi_oled_send_cmd(struct spi_ssd1327 *spi_ssd1327, uint8_t cmd);

void spi_oled_send_cmd_arg(struct spi_ssd1327 *spi_ssd1327, uint8_t cmd, uint8_t arg);

void spi_oled_send_data(struct spi_ssd1327 *spi_ssd1327, void *data, uint32_t data_len_bits);

void spi_oled_draw_square(struct spi_ssd1327 *spi_ssd1327, uint8_t x, uint8_t y, uint8_t width, uint8_t height, ssd1327_gs_t gs);

void spi_oled_draw_circle(struct spi_ssd1327 *spi_ssd1327, uint8_t x, uint8_t y);

// Draw a text string on the OLED using a font
void spi_oled_drawText(
    struct spi_ssd1327 *spi_ssd1327,
    uint8_t x_start, uint8_t y_start,
    const variable_font_t *font,
    ssd1327_gs_t gs_main,
    const char *text,
    bool with_shadow);

// Draw a grayscale image (4bpp) on the OLED
void spi_oled_drawImage(
    struct spi_ssd1327 *spi_ssd1327,
    uint8_t x,
    uint8_t y,
    uint8_t width,
    uint8_t height,
    const uint8_t *image // packed 4bpp: (width+1)/2 per row
);

typedef struct {
    struct spi_ssd1327 *spi_ssd1327;
    uint8_t x, y, width, height;
    uint8_t frame_count;
    const uint8_t *animation_data;
    uint32_t frame_delay_ms;
    TaskHandle_t task_handle;
} spi_oled_animation_t;