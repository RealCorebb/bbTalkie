#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "driver/spi_master.h"
#include "driver/gpio.h"

#include "esp32-spi-ssd1327.h"

void spi_oled_init(struct spi_ssd1327 *spi_ssd1327)
{
    /* {{{ */
    spi_oled_reset(spi_ssd1327);

    /* Turn the display off (datasheet p. 49) */
    spi_oled_send_cmd(spi_ssd1327, 0xAE);

    /* Set Column Bounds */
    spi_oled_send_cmd(spi_ssd1327, 0x15);
    spi_oled_send_cmd(spi_ssd1327, 0x00);
    spi_oled_send_cmd(spi_ssd1327, 0x7F);

    /* Set Row Bounds */
    spi_oled_send_cmd(spi_ssd1327, 0x75);
    spi_oled_send_cmd(spi_ssd1327, 0x00);
    spi_oled_send_cmd(spi_ssd1327, 0x7F);

    /* Set Contrast Control (Effectively brightness control) */
    spi_oled_send_cmd_arg(spi_ssd1327, 0x81, 0xD4); // 0xD4 works, 0xE0 or higher causes flickering */

    /* Set Re-map: (Tell device how to handle data writes)
     * (See datasheet p. 36, 43).
     */
    spi_oled_send_cmd_arg(spi_ssd1327, 0xA0, 0x51);

    /* Set Display Start Line: 0 (See datasheet p. 47) */
    spi_oled_send_cmd_arg(spi_ssd1327, 0xA1, 0x7F);

    /* Set Display Offset: 0 (See datasheet p. 47) */
    spi_oled_send_cmd_arg(spi_ssd1327, 0xA2, 0x00); // = 0

    /* Set the oscillator frequency and the front clock divider ratio */
    spi_oled_send_cmd_arg(spi_ssd1327, 0xB3, 0x51); // = 0 1 -> Fosc=((7/15 * (655kHz - 535kHz)) + 535kHz)? D=0+1=2
    /* spi_oled_send_cmd_arg(spi_ssd1327, 0xB3, 0x00); // = 0 0 -> Fosc=535kHz? D=0+1=2  (default/RESET) */

    /* Set phase 2 period length and the phase 1 period length (Set in DCLK units) */
    spi_oled_send_cmd_arg(spi_ssd1327, 0xB1, 0x11); // = 0001 0001 = 1 DCLKs 1 DCLKS
    /* spi_oled_send_cmd_arg(spi_ssd1327, 0xB1, 0x74); // = 0111 0100 = 8 DCLKS 5 DCLKS  (default/RESET) */

    /* Set phase 3 period length (Set in DCLK units) */
    spi_oled_send_cmd_arg(spi_ssd1327, 0xB6, 0x00); // **** 0000 = 0 DCLKs
    /* spi_oled_send_cmd_arg(spi_ssd1327, 0xB6, 0x04); // **** 0100 = 5 DCLKs  (default/RESET) */

    /* Enable/Disable Vdd regulator */
    spi_oled_send_cmd_arg(spi_ssd1327, 0xAB, 0x01); // 0000 0001 = Enable

    /* Set Vcomh voltage (see datasheet p. 39) */
    spi_oled_send_cmd_arg(spi_ssd1327, 0xBE, 0x07); // 0000 0111 = Vcomh = 0.86 * Vcc
    /* spi_oled_send_cmd_arg(spi_ssd1327, 0xBE, 0x05); // 0000 0101 = Vcomh = 0.82 * Vcc  (default/RESET) */

    /* Set pre-charge voltage (phase 2) (see datasheet p. 39) */
    spi_oled_send_cmd_arg(spi_ssd1327, 0xBC, 0x05); // 0000 0101 = Vp = 0.5 * Vcc  (default/RESET)
    /* spi_oled_send_cmd_arg(spi_ssd1327, 0xBC, 0x08); // 0000 1000 = Vp = Vcomh */
    /* spi_oled_send_cmd_arg(spi_ssd1327, 0xBC, 0x07); // 0000 0111 = Vp = 0.613 * Vcc */

    /* Enable/disable second pre-charge, select external/internal VSL */
    spi_oled_send_cmd_arg(spi_ssd1327, 0xD5, 0x62); // 0110 0010 = 6 2 = enable second precharge, internal VSL
    /* spi_oled_send_cmd_arg(spi_ssd1327, 0xD5, 0x60); // 0110 0010 = 6 0 = disable second precharge, internal VSL */

    /* Turn the display on (datasheet p. 49) */
    spi_oled_send_cmd(spi_ssd1327, 0xAF);
    /* }}} */
}

void spi_oled_deinit(struct spi_ssd1327 *spi_ssd1327)
{
    /* {{{ */

    /* Clear the display buffer to prevent burn-in */
    spi_oled_send_cmd(spi_ssd1327, 0xA5); // Entire display ON (all pixels at GS15)

    /* Small delay to allow display clearing */
    // Add appropriate delay function here if available
    // delay_ms(10);

    /* Turn the display off (datasheet p. 49) */
    spi_oled_send_cmd(spi_ssd1327, 0xAE);

    /* Disable Vdd regulator to save power */
    spi_oled_send_cmd_arg(spi_ssd1327, 0xAB, 0x00); // 0000 0000 = Disable

    /* Set display to sleep mode by setting all voltages to minimum */
    /* Set Contrast Control to minimum */
    spi_oled_send_cmd_arg(spi_ssd1327, 0x81, 0x00); // Minimum contrast

    /* Set Vcomh voltage to minimum (see datasheet p. 39) */
    spi_oled_send_cmd_arg(spi_ssd1327, 0xBE, 0x00); // 0000 0000 = Vcomh = 0.65 * Vcc (minimum)

    /* Set pre-charge voltage to minimum (phase 2) */
    spi_oled_send_cmd_arg(spi_ssd1327, 0xBC, 0x00); // 0000 0000 = Vp = 0.2 * Vcc (minimum)

    /* Disable second pre-charge */
    spi_oled_send_cmd_arg(spi_ssd1327, 0xD5, 0x60); // 0110 0000 = disable second precharge, internal VSL

    /* Set oscillator frequency to minimum to reduce power consumption */
    spi_oled_send_cmd_arg(spi_ssd1327, 0xB3, 0x00); // Minimum frequency = 535kHz

    /* Set phase periods to minimum */
    spi_oled_send_cmd_arg(spi_ssd1327, 0xB1, 0x11); // Minimum phase 1 and 2 periods
    spi_oled_send_cmd_arg(spi_ssd1327, 0xB6, 0x00); // Minimum phase 3 period

    /* Optional: Assert reset line to ensure complete shutdown */
    /* This would require access to the reset GPIO pin */
    /* gpio_set_low(spi_ssd1327->reset_pin); */

    /* }}} */
}

void spi_oled_reset(struct spi_ssd1327 *spi_ssd1327)
{
    /* {{{ */
    gpio_set_level(spi_ssd1327->rst_pin_num, 1);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    gpio_set_level(spi_ssd1327->rst_pin_num, 0);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    gpio_set_level(spi_ssd1327->rst_pin_num, 1);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    /* }}} */
}

void spi_oled_send_cmd(struct spi_ssd1327 *spi_ssd1327, uint8_t cmd)
{
    /* {{{ */
    spi_device_acquire_bus(*(spi_ssd1327->spi_handle), portMAX_DELAY);
    gpio_set_level(spi_ssd1327->dc_pin_num, 0);

    spi_transaction_t transaction;
    memset(&transaction, 0, sizeof(transaction)); // Zero out the transaction
    transaction.length = 8;
    transaction.tx_buffer = &cmd;

    ESP_ERROR_CHECK(spi_device_transmit(*(spi_ssd1327->spi_handle), &transaction));

    spi_device_release_bus(*(spi_ssd1327->spi_handle));
    /* }}} */
}

void spi_oled_send_cmd_arg(struct spi_ssd1327 *spi_ssd1327, uint8_t cmd,
                           uint8_t arg)
{

    /* {{{ */
    spi_device_acquire_bus(*(spi_ssd1327->spi_handle), portMAX_DELAY);
    gpio_set_level(spi_ssd1327->dc_pin_num, 0);

    uint8_t cmdarg[2] = {cmd, arg};

    spi_transaction_t transaction;
    memset(&transaction, 0, sizeof(transaction)); // Zero out the transaction
    transaction.length = 16;
    transaction.tx_buffer = &cmdarg[0];

    ESP_ERROR_CHECK(spi_device_transmit(*(spi_ssd1327->spi_handle), &transaction));

    spi_device_release_bus(*(spi_ssd1327->spi_handle));
    /* }}} */
}

void spi_oled_send_data(struct spi_ssd1327 *spi_ssd1327, void *data,
                        uint32_t data_len_bits)
{

    /* {{{ */
    spi_device_acquire_bus(*(spi_ssd1327->spi_handle), portMAX_DELAY);
    gpio_set_level(spi_ssd1327->dc_pin_num, 1);

    spi_transaction_t transaction;
    memset(&transaction, 0, sizeof(transaction)); // Zero out the transaction
    transaction.length = data_len_bits;
    transaction.tx_buffer = data;

    ESP_ERROR_CHECK(spi_device_transmit(*(spi_ssd1327->spi_handle), &transaction));

    spi_device_release_bus(*(spi_ssd1327->spi_handle));
    /* }}} */
}

void spi_oled_draw_square(struct spi_ssd1327 *spi_ssd1327, uint8_t x,
                          uint8_t y, uint8_t width, uint8_t height, ssd1327_gs_t gs)
{

    /* {{{ */
    /* Function: Set Column Address (Tell device we are about to set the
     * column bounds for an upcoming write). (See datasheet p. 36) */
    spi_oled_send_cmd(spi_ssd1327, 0x15);
    /* Set the column start address */
    spi_oled_send_cmd(spi_ssd1327, ((x + 1) / 2));
    /* Set the column end address */
    spi_oled_send_cmd(spi_ssd1327, ((x + 1) / 2) + ((width + 1) / 2) - 1);

    /* Function: Set Row Address (Tell device we are about to set the
     * row bounds for an upcoming write) (See datasheet p. 36) */
    spi_oled_send_cmd(spi_ssd1327, 0x75);
    /* Set the row start address */
    spi_oled_send_cmd(spi_ssd1327, y);
    /* Set the row end address */
    spi_oled_send_cmd(spi_ssd1327, y + height - 1);

    uint8_t rowdata[(width + 1) / 2];

    /* Create a row populated with pixels of the grayscale represented by 'gs' */
    for (int i = 0; i < ((width + 1) / 2); i++)
    {
        rowdata[i] = (gs << 4) | gs;
    }

    /* Draw the in the square, one row at a time */
    for (int j = 0; j < 128; j++)
    {
        spi_oled_send_data(spi_ssd1327, &rowdata[0], 4 * width);
    }
    /* }}} */
}

void spi_oled_draw_circle(struct spi_ssd1327 *spi_ssd1327, uint8_t x,
                          uint8_t y)
{

    /* {{{ */
    /* Function: Set Column Address (Tell device we are about to set the
     * column bounds for an upcoming write). (See datasheet p. 36) */
    spi_oled_send_cmd(spi_ssd1327, 0x15);
    /* Set the column start address */
    spi_oled_send_cmd(spi_ssd1327, ((x + 1) / 2));
    /* Set the column end address */
    spi_oled_send_cmd(spi_ssd1327, ((x + 1) / 2) + (16 / 2) - 1);

    /* Function: Set Row Address (Tell device we are about to set the
     * row bounds for an upcoming write) (See datasheet p. 36) */
    spi_oled_send_cmd(spi_ssd1327, 0x75);
    /* Set the row start address */
    spi_oled_send_cmd(spi_ssd1327, y);
    /* Set the row end address */
    spi_oled_send_cmd(spi_ssd1327, y + 16 - 1);

    /* Draw the 16 pixel diameter circle */
    /* 8 bits per array element * 16 rows * (16 columns / 2 pixels per column) */
    spi_oled_send_data(spi_ssd1327, &whitecircle16[0], 8 * 16 * (16 / 2));
    /* }}} */
}

// Initialize frame buffer
ssd1327_framebuffer_t *spi_oled_framebuffer_create(void)
{
    ssd1327_framebuffer_t *fb = malloc(sizeof(ssd1327_framebuffer_t));
    if (!fb)
        return NULL;

    fb->width = 128;
    fb->height = 128;
    fb->bytes_per_row = 64; // 128 pixels / 2 pixels per byte

    fb->buffer = calloc(fb->height * fb->bytes_per_row, sizeof(uint8_t));
    if (!fb->buffer)
    {
        free(fb);
        return NULL;
    }

    return fb;
}

void spi_oled_framebuffer_destroy(ssd1327_framebuffer_t *fb)
{
    if (fb)
    {
        if (fb->buffer)
            free(fb->buffer);
        free(fb);
    }
}

// Clear frame buffer
void spi_oled_framebuffer_clear(ssd1327_framebuffer_t *fb, ssd1327_gs_t gs)
{
    if (!fb || !fb->buffer)
        return;

    uint8_t fill_value = (gs << 4) | gs;
    memset(fb->buffer, fill_value, fb->height * fb->bytes_per_row);
}

// Set a pixel in the frame buffer
void spi_oled_framebuffer_set_pixel(ssd1327_framebuffer_t *fb, uint8_t x, uint8_t y, ssd1327_gs_t gs)
{
    if (!fb || !fb->buffer || x >= fb->width || y >= fb->height)
        return;

    uint16_t byte_idx = y * fb->bytes_per_row + x / 2;
    uint8_t pixel_pos = x % 2;

    if (pixel_pos == 0)
    {
        // Left pixel (high 4 bits)
        fb->buffer[byte_idx] = (fb->buffer[byte_idx] & 0x0F) | (gs << 4);
    }
    else
    {
        // Right pixel (low 4 bits)
        fb->buffer[byte_idx] = (fb->buffer[byte_idx] & 0xF0) | gs;
    }
}

// Draw background image to frame buffer
void spi_oled_framebuffer_draw_image(ssd1327_framebuffer_t *fb,
                                     uint8_t x, uint8_t y,
                                     uint8_t width, uint8_t height,
                                     const uint8_t *image)
{
    if (!fb || !fb->buffer || !image)
        return;

    uint8_t img_bytes_per_row = (width + 1) / 2;

    for (uint8_t row = 0; row < height; ++row)
    {
        if (y + row >= fb->height)
            break;

        uint16_t fb_row_start = (y + row) * fb->bytes_per_row;
        uint16_t img_row_start = row * img_bytes_per_row;

        // Handle pixel-aligned copying
        if (x % 2 == 0)
        {
            // Aligned case - direct copy
            uint8_t bytes_to_copy = img_bytes_per_row;
            if (x / 2 + bytes_to_copy > fb->bytes_per_row)
            {
                bytes_to_copy = fb->bytes_per_row - x / 2;
            }

            memcpy(&fb->buffer[fb_row_start + x / 2],
                   &image[img_row_start],
                   bytes_to_copy);
        }
        else
        {
            // Unaligned case - need to shift pixels
            for (uint8_t col = 0; col < width && x + col < fb->width; ++col)
            {
                uint8_t img_byte_idx = col / 2;
                uint8_t img_pixel_pos = col % 2;
                ssd1327_gs_t pixel_value;

                if (img_pixel_pos == 0)
                {
                    pixel_value = (image[img_row_start + img_byte_idx] >> 4) & 0x0F;
                }
                else
                {
                    pixel_value = image[img_row_start + img_byte_idx] & 0x0F;
                }

                spi_oled_framebuffer_set_pixel(fb, x + col, y + row, pixel_value);
            }
        }
    }
}

// Draw text to frame buffer (transparent background)
void spi_oled_framebuffer_draw_text(ssd1327_framebuffer_t *fb,
                                    uint8_t x, uint8_t y,
                                    const variable_font_t *font,
                                    ssd1327_gs_t gs,
                                    const char *text)
{
    if (!fb || !fb->buffer || !text || !*text || !font)
        return;

    uint16_t char_x = x;
    const char *str = text;

    while (*str)
    {
        char c = *str++;
        if (c < 32 || c > 126)
            continue;

        uint8_t char_idx = c - 32;
        uint8_t char_width = font->widths[char_idx];
        uint16_t data_offset = font->offsets[char_idx];
        uint8_t bytes_per_row = (char_width + 7) / 8;

        // Render character
        for (uint8_t row = 0; row < font->height; ++row)
        {
            if (y + row >= fb->height)
                break;

            for (uint8_t col = 0; col < char_width; ++col)
            {
                if (char_x + col >= fb->width)
                    break;

                uint8_t byte_idx = col / 8;
                uint8_t bit_idx = col % 8;
                uint8_t font_byte = font->data[data_offset + row * bytes_per_row + byte_idx];

                if (font_byte & (1 << (7 - bit_idx)))
                {
                    spi_oled_framebuffer_set_pixel(fb, char_x + col, y + row, gs);
                }
            }
        }

        char_x += char_width + 1; // Character spacing
    }
}

// Update entire display from frame buffer
void spi_oled_framebuffer_update_display(struct spi_ssd1327 *spi_ssd1327,
                                         ssd1327_framebuffer_t *fb)
{
    if (!spi_ssd1327 || !fb || !fb->buffer)
        return;

    // Set full display window
    spi_oled_send_cmd(spi_ssd1327, 0x15);
    spi_oled_send_cmd(spi_ssd1327, 0x00); // Start column
    spi_oled_send_cmd(spi_ssd1327, 0x3F); // End column (63)

    spi_oled_send_cmd(spi_ssd1327, 0x75);
    spi_oled_send_cmd(spi_ssd1327, 0x00); // Start row
    spi_oled_send_cmd(spi_ssd1327, 0x7F); // End row (127)

    // Send entire frame buffer
    spi_oled_send_data(spi_ssd1327, fb->buffer, fb->height * fb->bytes_per_row * 8);
}

// Update partial display region from frame buffer
void spi_oled_framebuffer_update_region(struct spi_ssd1327 *spi_ssd1327,
                                        ssd1327_framebuffer_t *fb,
                                        uint8_t x, uint8_t y,
                                        uint8_t width, uint8_t height)
{
    if (!spi_ssd1327 || !fb || !fb->buffer)
        return;

    uint8_t start_col = x / 2;
    uint8_t end_col = (x + width - 1) / 2;
    uint8_t bytes_per_update_row = end_col - start_col + 1;

    spi_oled_send_cmd(spi_ssd1327, 0x15);
    spi_oled_send_cmd(spi_ssd1327, start_col);
    spi_oled_send_cmd(spi_ssd1327, end_col);

    spi_oled_send_cmd(spi_ssd1327, 0x75);
    spi_oled_send_cmd(spi_ssd1327, y);
    spi_oled_send_cmd(spi_ssd1327, y + height - 1);

    // Send region data row by row
    for (uint8_t row = 0; row < height; ++row)
    {
        if (y + row >= fb->height)
            break;

        uint16_t fb_offset = (y + row) * fb->bytes_per_row + start_col;
        spi_oled_send_data(spi_ssd1327, &fb->buffer[fb_offset], bytes_per_update_row * 8);
    }
}

// Usage example:
/*
// Initialize frame buffer
ssd1327_framebuffer_t *fb = spi_oled_framebuffer_create();

// Draw background image
spi_oled_framebuffer_draw_image(fb, 0, 0, 128, 128, background_data);

// Draw text on top (with transparent background)
spi_oled_framebuffer_draw_text(fb, 10, 20, &my_font, 15, "Hello World!");

// Update display
spi_oled_framebuffer_update_display(&my_spi_ssd1327, fb);

// Clean up
spi_oled_framebuffer_destroy(fb);
*/

void spi_oled_drawTextWithShadow(
    struct spi_ssd1327 *spi_ssd1327,
    uint8_t x, uint8_t y,
    const variable_font_t *font,
    ssd1327_gs_t gs,
    ssd1327_gs_t gs_shadow,
    int8_t offset_x,
    int8_t offset_y,
    const char *text)
{
    if (!text || !*text)
        return;

    // 计算文本整体尺寸
    uint16_t total_width = 0;
    const char *temp_text = text;
    while (*temp_text)
    {
        char c = *temp_text++;
        if (c >= 32 && c <= 126)
        {
            uint8_t char_idx = c - 32;
            total_width += font->widths[char_idx] + 1; // +1 for spacing
        }
    }
    if (total_width > 0)
        total_width--; // 移除最后一个字符后的间距

    // 计算包含阴影的总边界框
    int16_t min_x = (offset_x < 0) ? offset_x : 0;
    int16_t min_y = (offset_y < 0) ? offset_y : 0;
    int16_t max_x = (offset_x > 0) ? total_width + offset_x : total_width;
    int16_t max_y = (offset_y > 0) ? font->height + offset_y : font->height;

    uint16_t buffer_width = max_x - min_x;
    uint16_t buffer_height = max_y - min_y;
    uint16_t buffer_bytes_per_row = (buffer_width + 1) / 2;

    // 创建缓冲区（初始化为0）
    uint8_t *buffer = calloc(buffer_height * buffer_bytes_per_row, sizeof(uint8_t));
    if (!buffer)
        return;

    // 设置渲染上下文
    render_context_t ctx = {
        .buffer = buffer,
        .buffer_width = buffer_width,
        .buffer_height = buffer_height,
        .buffer_bytes_per_row = buffer_bytes_per_row,
        .min_x = min_x,
        .min_y = min_y
    };

    // 先渲染阴影（如果阴影灰度值大于0）
    if (gs_shadow > 0)
    {
        render_text(&ctx, font, text, offset_x, offset_y, gs_shadow);
    }

    // 再渲染主文本
    render_text(&ctx, font, text, 0, 0, gs);

    // 计算在显示器上的实际绘制位置和尺寸
    int16_t draw_x = x + min_x;
    int16_t draw_y = y + min_y;

    // 边界检查
    if (draw_x >= 128 || draw_y >= 128 ||
        draw_x + buffer_width <= 0 || draw_y + buffer_height <= 0)
    {
        free(buffer);
        return;
    }

    // 调整绘制区域以适应显示器边界
    uint16_t skip_left = 0, skip_top = 0;
    uint16_t draw_width = buffer_width, draw_height = buffer_height;

    if (draw_x < 0)
    {
        skip_left = -draw_x;
        draw_width -= skip_left;
        draw_x = 0;
    }
    if (draw_y < 0)
    {
        skip_top = -draw_y;
        draw_height -= skip_top;
        draw_y = 0;
    }
    if (draw_x + draw_width > 128)
    {
        draw_width = 128 - draw_x;
    }
    if (draw_y + draw_height > 128)
    {
        draw_height = 128 - draw_y;
    }

    // 发送到显示器
    uint8_t start_col = draw_x / 2;
    uint8_t end_col = (draw_x + draw_width - 1) / 2;

    spi_oled_send_cmd(spi_ssd1327, 0x15);
    spi_oled_send_cmd(spi_ssd1327, start_col);
    spi_oled_send_cmd(spi_ssd1327, end_col);

    spi_oled_send_cmd(spi_ssd1327, 0x75);
    spi_oled_send_cmd(spi_ssd1327, draw_y);
    spi_oled_send_cmd(spi_ssd1327, draw_y + draw_height - 1);

    // 发送每一行数据
    uint16_t bytes_to_send = end_col - start_col + 1;
    for (uint16_t row = 0; row < draw_height; ++row)
    {
        uint16_t src_row = row + skip_top;
        uint16_t src_offset = src_row * buffer_bytes_per_row + skip_left / 2;

        // 处理奇数起始位置的像素对齐
        if (skip_left % 2 == 1)
        {
            // 需要重新打包数据
            uint8_t *row_data = malloc(bytes_to_send);
            for (uint16_t i = 0; i < bytes_to_send; ++i)
            {
                uint8_t left_pixel, right_pixel;

                // 左像素来自当前字节的右半部分
                left_pixel = buffer[src_offset + i] & 0x0F;

                // 右像素来自下一个字节的左半部分（如果存在）
                if (src_offset + i + 1 < buffer_height * buffer_bytes_per_row)
                {
                    right_pixel = (buffer[src_offset + i + 1] >> 4) & 0x0F;
                }
                else
                {
                    right_pixel = 0;
                }

                row_data[i] = (left_pixel << 4) | right_pixel;
            }
            spi_oled_send_data(spi_ssd1327, row_data, bytes_to_send * 8);
            free(row_data);
        }
        else
        {
            // 直接发送数据
            spi_oled_send_data(spi_ssd1327, &buffer[src_offset], bytes_to_send * 8);
        }
    }

    free(buffer);
}

// 保留原始函数作为无阴影版本
void spi_oled_drawText(
    struct spi_ssd1327 *spi_ssd1327,
    uint8_t x, uint8_t y,
    const variable_font_t *font,
    ssd1327_gs_t gs,
    const char *text)
{
    spi_oled_drawTextWithShadow(spi_ssd1327, x, y, font, gs, 0, 0, 0, text);
}

void spi_oled_drawImage(
    struct spi_ssd1327 *spi_ssd1327,
    uint8_t x, uint8_t y,
    uint8_t width, uint8_t height,
    const uint8_t *image // packed 4bpp: (width+1)/2 bytes per row
)
{
    // Calculate GDDRAM column addresses (0-63 range for SSD1327)
    // Each GDDRAM column byte stores two 4-bit pixels.
    uint8_t start_col_byte_addr = x / 2;
    uint8_t image_bytes_per_row = (width + 1) / 2; // Number of bytes for 'width' pixels in the image data

    // Ensure image_bytes_per_row is at least 1 if width > 0
    if (width > 0 && image_bytes_per_row == 0)
    {
        image_bytes_per_row = 1;
    }

    uint8_t end_col_byte_addr;
    if (width == 0)
    { // Handle zero width case
        end_col_byte_addr = start_col_byte_addr;
    }
    else
    {
        // The end column address for the *write window* is based on how many bytes we send.
        end_col_byte_addr = start_col_byte_addr + image_bytes_per_row - 1;
    }

    // Clamp addresses to valid GDDRAM range (0-63 for columns, 0-127 for rows)
    if (start_col_byte_addr > 63)
        start_col_byte_addr = 63;
    if (end_col_byte_addr > 63)
        end_col_byte_addr = 63;
    if (y > 127)
        y = 127;
    uint8_t end_row_addr = y + height - 1;
    if (end_row_addr > 127)
        end_row_addr = 127;
    if (height == 0)
    { // If height is 0, nothing to draw.
        // Or handle as error. If y > end_row_addr due to height being 0,
        // the loop for r won't run anyway.
        return;
    }

    spi_oled_send_cmd(spi_ssd1327, 0x15);
    spi_oled_send_cmd(spi_ssd1327, start_col_byte_addr);
    spi_oled_send_cmd(spi_ssd1327, end_col_byte_addr);

    spi_oled_send_cmd(spi_ssd1327, 0x75);
    spi_oled_send_cmd(spi_ssd1327, y);
    spi_oled_send_cmd(spi_ssd1327, end_row_addr);

    // Send image data row by row
    for (uint8_t r = 0; r < height; ++r)
    {
        // Check if current row is within display bounds (y+r)
        if (y + r > end_row_addr)
            break; // Stop if current row exceeds the calculated end_row_addr
        if (image_bytes_per_row > 0)
        { // Only send if there's data for the row
            spi_oled_send_data(spi_ssd1327, &image[r * image_bytes_per_row], (uint32_t)image_bytes_per_row * 8);
        }
    }
}