#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "driver/spi_master.h"
#include "driver/gpio.h"

#include "esp32-spi-ssd1327.h"

uint8_t whitecircle16[128] = {0x00, 0x00, 0x0f, 0xff, 0xff, 0xf0, 0x00, 0x00,
                              0x00, 0x0f, 0xff, 0xff, 0xff, 0xff, 0xf0, 0x00,
                              0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00,
                              0x0f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf0,
                              0x0f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf0,
                              0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                              0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                              0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                              0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                              0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                              0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                              0x0f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf0,
                              0x0f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf0,
                              0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00,
                              0x00, 0x0f, 0xff, 0xff, 0xff, 0xff, 0xf0, 0x00,
                              0x00, 0x00, 0x0f, 0xff, 0xff, 0xf0, 0x00, 0x00};

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
     * It's worth noting here that to resolve the issue I was experiencing
     * where whatever was drawn was interlaced and spread over twice its actual
     * height is resolved by ensuring that some of these A[x] vars "in line".
     * In particular, look at A[6] */
    spi_oled_send_cmd_arg(spi_ssd1327, 0xA0, 0x51);

    /* Set Display Start Line: 0 (See datasheet p. 47) */
    spi_oled_send_cmd_arg(spi_ssd1327, 0xA1, 0x7F);

    /* Set Display Offset: 0 (See datasheet p. 47) */
    spi_oled_send_cmd_arg(spi_ssd1327, 0xA2, 0x00); // = 0

    /**
     * Phase 1: the pixel is reset to Vlss (relative low) in order to discharge
     *          previous data (higher capacitance of the OLED = phase 1's
     *          period should be longer)
     * Phase 2: first pre-charge: the pixel receives voltage going to Vp
     *          (higher capacitance of the OLED = phase 2's period should be
     *          longer)
     * Phase 3: second pre-charge: the pixel receives voltage to get from Vp to
     *          Vcomh
     * Phase 4: PWM: the pixel holds Vcomh voltage for a given amount of time
     *          to maintain a given brightness
     */

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


/*
const uint8_t font8x8_basic[95][8] = { };

font_t font8x8 = {
    .width = 8,
    .height = 8,
    .data = (const uint8_t *)font8x8_basic,
};
*/

void spi_oled_drawText(
    struct spi_ssd1327 *spi_ssd1327,
    uint8_t x, uint8_t y,
    const font_t *font,
    ssd1327_gs_t gs, // Grayscale (0–15)
    const char *text)
{
    while (*text)
    {
        char c = *text++;
        // ASCII 32..126 supported
        if (c < 32 || c > 126)
            continue;
        uint16_t offset = (c - 32) * font->height;

        // Render each row of the character
        for (uint8_t row = 0; row < font->height; ++row)
        {
            spi_oled_send_cmd(spi_ssd1327, 0x15); // Set col
            spi_oled_send_cmd(spi_ssd1327, (x) / 2);
            spi_oled_send_cmd(spi_ssd1327, (x + font->width - 1) / 2);
            spi_oled_send_cmd(spi_ssd1327, 0x75); // Set row
            spi_oled_send_cmd(spi_ssd1327, y + row);
            spi_oled_send_cmd(spi_ssd1327, y + row);

            // Compose pixel data for row
            uint8_t byte = font->data[offset + row];
            uint8_t rowdata[(font->width + 1) / 2];
            for (int i = 0; i < font->width; i += 2)
            {
                uint8_t p1 = (byte & (1 << (7 - i))) ? gs : 0;
                uint8_t p2 = (i + 1 < font->width) && (byte & (1 << (7 - (i + 1)))) ? gs : 0;
                rowdata[i / 2] = (p1 << 4) | p2;
            }
            spi_oled_send_data(spi_ssd1327, rowdata, (font->width + 1) / 2);
        }
        x += font->width;
    }
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
    if (width > 0 && image_bytes_per_row == 0) {
        image_bytes_per_row = 1;
    }
    
    uint8_t end_col_byte_addr;
    if (width == 0) { // Handle zero width case
        end_col_byte_addr = start_col_byte_addr;
    } else {
        // The end column address for the *write window* is based on how many bytes we send.
        end_col_byte_addr = start_col_byte_addr + image_bytes_per_row - 1;
    }


    // Clamp addresses to valid GDDRAM range (0-63 for columns, 0-127 for rows)
    if (start_col_byte_addr > 63) start_col_byte_addr = 63;
    if (end_col_byte_addr > 63) end_col_byte_addr = 63;
    if (y > 127) y = 127;
    uint8_t end_row_addr = y + height - 1;
    if (end_row_addr > 127) end_row_addr = 127;
    if (height == 0) { // If height is 0, nothing to draw.
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
        if (y + r > end_row_addr) break; // Stop if current row exceeds the calculated end_row_addr
        if (image_bytes_per_row > 0) { // Only send if there's data for the row
            spi_oled_send_data(spi_ssd1327, &image[r * image_bytes_per_row], (uint32_t)image_bytes_per_row * 8);
        }
    }
}

// Animation: assumes all frames same w/h
void spi_oled_drawAnimation(
    struct spi_ssd1327 *spi_ssd1327,
    uint8_t x, uint8_t y,
    uint8_t w, uint8_t h,
    const uint8_t *frames[], // [frame][data]
    uint8_t frame_count,
    uint8_t frame_idx)
{
    if (frame_idx < frame_count)
        spi_oled_drawImage(spi_ssd1327, x, y, w, h, frames[frame_idx]);
}

/*

// Animated: 4 frames, 32x16 px
const uint8_t anim[4][16][16] = {
    // [frame][row][8 bytes per row]
    // frame 0 ...
    // frame 1 ...
    // ...
};
const uint8_t *frames[4] = {
    (const uint8_t *)anim[0],
    (const uint8_t *)anim[1],
    (const uint8_t *)anim[2],
    (const uint8_t *)anim[3]
};
*/