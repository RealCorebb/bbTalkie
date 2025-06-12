#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "driver/spi_master.h"
#include "driver/gpio.h"

#include "esp32-spi-ssd1327.h"

#define MAX_DISPLAY_WIDTH_PX 128
#define MAX_ROW_DATA_BYTES (MAX_DISPLAY_WIDTH_PX / 2) // Each byte holds 2 pixels

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
    uint8_t x_start, uint8_t y_start,
    const variable_font_t *font,
    ssd1327_gs_t gs_main,
    const char *text,
    bool with_shadow) // New parameter
{
    // Ensure grayscale values are within 0-15
    gs_main &= 0x0F;
    ssd1327_gs_t gs_shadow = (gs_main / 2) & 0x0F; // Half grayscale for shadow

    // Calculate total width of the text to determine the buffer size needed
    uint16_t total_text_width = 0;
    const char *temp_text = text;
    while (*temp_text)
    {
        char c = *temp_text++;
        if (c < 32 || c > 126)
            continue;
        uint8_t char_idx = c - 32;
        total_text_width += font->widths[char_idx] + 1; // Character width + spacing
    }
    if (total_text_width > 0)
    {
        total_text_width -= 1; // Remove last spacing
    }

    // Determine the actual drawing width, limited by display and starting position
    // We need to account for both main text and shadow positions if shadow is enabled
    uint16_t draw_start_x;
    if (with_shadow)
    {
        draw_start_x = x_start - 1; // Shadow starts one pixel to the left
        if (draw_start_x > x_start)
            draw_start_x = 0; // Handle underflow if x_start is 0
    }
    else
    {
        draw_start_x = x_start;
    }

    uint16_t draw_end_x = x_start + total_text_width - 1;
    // Ensure draw_end_x doesn't go beyond display width
    if (draw_end_x >= MAX_DISPLAY_WIDTH_PX)
    {
        draw_end_x = MAX_DISPLAY_WIDTH_PX - 1;
    }

    // The actual number of bytes we will send per row
    // Each byte holds two pixels (4-bit grayscale)
    uint16_t num_pixels_to_draw = (draw_end_x - draw_start_x) + 1;
    uint16_t num_bytes_to_send_per_row = (num_pixels_to_draw + 1) / 2; // +1 for odd pixel count

    // Ensure the byte count does not exceed the buffer size
    if (num_bytes_to_send_per_row > MAX_ROW_DATA_BYTES)
    {
        num_bytes_to_send_per_row = MAX_ROW_DATA_BYTES;
    }

    // Buffer to hold the combined pixel data for one row
    uint8_t row_data_buffer[MAX_ROW_DATA_BYTES];

    // Render each row of the text (height of the font)
    for (uint8_t row = 0; row < font->height; ++row)
    {
        // Initialize row data buffer to all zeros (black)
        memset(row_data_buffer, 0, num_bytes_to_send_per_row);

        uint8_t current_x_main = x_start;
        uint8_t current_x_shadow = x_start - 1; // Shadow position

        const char *char_ptr = text;
        while (*char_ptr)
        {
            char c = *char_ptr++;
            if (c < 32 || c > 126)
                continue;

            uint8_t char_idx = c - 32;
            uint8_t char_width = font->widths[char_idx];
            uint16_t data_offset = font->offsets[char_idx];
            uint8_t bytes_per_font_row = (char_width + 7) / 8;

            // Iterate through each pixel column of the current character
            for (uint8_t col = 0; col < char_width; ++col)
            {
                uint8_t byte_in_font = font->data[data_offset + row * bytes_per_font_row + (col / 8)];
                uint8_t bit_in_byte = 7 - (col % 8); // Fonts usually store MSB first

                uint8_t is_pixel_set = (byte_in_font >> bit_in_byte) & 0x01;

                // --- Render Shadow Pixel (if enabled) ---
                if (with_shadow && is_pixel_set)
                {
                    int16_t abs_col_shadow = current_x_shadow + col;
                    // Check if shadow pixel is within the active drawing area
                    if (abs_col_shadow >= draw_start_x && abs_col_shadow <= draw_end_x)
                    {
                        uint8_t byte_idx_buffer = (abs_col_shadow - draw_start_x) / 2;
                        if (byte_idx_buffer < num_bytes_to_send_per_row)
                        {
                            if (abs_col_shadow % 2 == 0)
                            { // Even pixel (left nibble)
                                row_data_buffer[byte_idx_buffer] = (row_data_buffer[byte_idx_buffer] & 0x0F) | (gs_shadow << 4);
                            }
                            else
                            { // Odd pixel (right nibble)
                                row_data_buffer[byte_idx_buffer] = (row_data_buffer[byte_idx_buffer] & 0xF0) | gs_shadow;
                            }
                        }
                    }
                }

                // --- Render Main Text Pixel ---
                if (is_pixel_set)
                { // Always draw main text
                    int16_t abs_col_main = current_x_main + col;
                    // Check if main pixel is within the active drawing area
                    if (abs_col_main >= draw_start_x && abs_col_main <= draw_end_x)
                    {
                        uint8_t byte_idx_buffer = (abs_col_main - draw_start_x) / 2;
                        if (byte_idx_buffer < num_bytes_to_send_per_row)
                        {
                            if (abs_col_main % 2 == 0)
                            { // Even pixel (left nibble)
                                row_data_buffer[byte_idx_buffer] = (row_data_buffer[byte_idx_buffer] & 0x0F) | (gs_main << 4);
                            }
                            else
                            { // Odd pixel (right nibble)
                                row_data_buffer[byte_idx_buffer] = (row_data_buffer[byte_idx_buffer] & 0xF0) | gs_main;
                            }
                        }
                    }
                }
            }
            current_x_main += char_width + 1;   // Move to next character position (main text)
            current_x_shadow += char_width + 1; // Move to next character position (shadow)
        }

        // After processing all characters for the current row, send the combined data
        // Set column address window for the entire drawing width
        spi_oled_send_cmd(spi_ssd1327, 0x15);
        spi_oled_send_cmd(spi_ssd1327, draw_start_x / 2);
        // The end column command takes the physical column address, not the byte index.
        // It's (end_pixel_column + 1) / 2 - 1 according to some docs, or just end_pixel_column / 2.
        // Let's ensure it's `(actual_end_pixel_column_on_display) / 2`.
        spi_oled_send_cmd(spi_ssd1327, draw_end_x / 2);

        // Set row address window for the current row
        spi_oled_send_cmd(spi_ssd1327, 0x75);
        spi_oled_send_cmd(spi_ssd1327, y_start + row);
        spi_oled_send_cmd(spi_ssd1327, y_start + row);

        spi_oled_send_data(spi_ssd1327, row_data_buffer, num_bytes_to_send_per_row);
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