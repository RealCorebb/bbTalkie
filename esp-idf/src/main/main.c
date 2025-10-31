/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_wn_iface.h"
#include "esp_wn_models.h"
#include "esp_vadn_models.h"
#include "esp_afe_sr_models.h"
#include "esp_mn_iface.h"
#include "esp_mn_models.h"
#include "esp_board_init.h"
#include "model_path.h"
#include <math.h> // Add this for sin() function
#include <string.h>
#include "driver/gpio.h"
#include "include/sound.h"
#include "esp_audio_enc.h"
#include "esp_audio_enc_reg.h"
#include "esp_audio_enc_default.h"
#include "esp_audio_dec_default.h"
#include "esp_audio_dec.h"
#include "esp_adpcm_enc.h"
#include "esp_adpcm_dec.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "esp_system.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_timer.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "freertos/stream_buffer.h"

#include "esp32-spi-ssd1327.h"
#include <inttypes.h>
#include "driver/spi_master.h"
#include "include/images/logo.h"
#include "include/images/battery.h"
#include "include/images/mic.h"
#include "include/images/volume.h"
#include "include/images/home.h"
#include "include/images/custom.h"

#include "include/animation.h"
#include "include/command_map.h"

#include "include/fonts/fusion_pixel.h"
#include "include/fonts/fusion_pixel_30.h"

#include "driver/adc.h"
#include "soc/adc_channel.h"

#include "include/agc.h"
#include "include/led.h"
#include "esp_sleep.h"
#include "driver/rtc_io.h"
#include "iot_button.h"
#include "button_gpio.h"

#define SAMPLE_RATE 16000
#define BIT_DEPTH 16
#define ENCODED_BUF_SIZE 10240
#define PLAY_RING_BUFFER_SIZE 8192
#define PLAY_CHUNK_SIZE 2048
#define ESP_NOW_PACKET_SIZE 512

#define SPI_MOSI_PIN_NUM 14
#define SPI_SCK_PIN_NUM 13
#define SPI_CS_PIN_NUM 10
#define DC_PIN_NUM 12
#define RST_PIN_NUM 11
#define SPI_HOST_TAG SPI2_HOST

#define GPIO_WAKEUP_1 GPIO_NUM_4 // Charger CHRG
#define GPIO_WAKEUP_2 GPIO_NUM_8 // Button
#define GPIO_WAKEUP_3 GPIO_NUM_5 // Charger STDBY

// Button configuration
#define BUTTON_GPIO 8           // Boot button on most ESP32 boards
#define BUTTON_ACTIVE_LEVEL 0   // Active low (pressed = 0)
#define LONG_PRESS_TIME_MS 2000 // 2 seconds for long press

#define PING_MAGIC "PING"
#define PING_MAGIC_LEN 4
#define MAX_MAC_TRACK 9
#define MAC_TIMEOUT_MS 20000
int macCount = 1;
int lastMacCount = 0;
spi_oled_animation_t *anim_currentCommand = NULL;

typedef struct
{
    uint8_t mac[ESP_NOW_ETH_ALEN]; // 6 bytes
    int64_t last_seen_ms;          // Timestamp in ms
    bool valid;
} mac_track_entry_t;

static mac_track_entry_t mac_track_list[MAX_MAC_TRACK];

const variable_font_t font_10 = {
    .height = 10,
    .widths = font_10_widths,
    .offsets = font_10_offsets,
    .data = font_10_data};

const variable_font_t font_30 = {
    .height = 30,
    .widths = font_30_widths,
    .offsets = font_30_offsets,
    .data = font_30_data};

static esp_afe_sr_iface_t *afe_handle = NULL;
srmodel_list_t *models = NULL;
StreamBufferHandle_t play_stream_buf;
static QueueHandle_t s_recv_queue = NULL;
static uint8_t broadcast_mac[ESP_NOW_ETH_ALEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}; // Broadcast MAC address (all ones)
volatile bool is_receiving = false;
volatile bool is_speaking = false;
bool isShutdown = false;
bool isMicOff = false;
bool isMute = false;
bool is_command = false;
int state = 0; // 0: idle, 1: speaking, 2: receiving, 3: command
int lastState = -1;
static button_handle_t btn = NULL;
static bool button_initialized = false;
static const char *TAG = "bbTalkie";
spi_device_handle_t oled_dev_handle;
struct spi_ssd1327 spi_ssd1327 = {
    .dc_pin_num = DC_PIN_NUM,
    .rst_pin_num = RST_PIN_NUM,
    .spi_handle = &oled_dev_handle,
};
SemaphoreHandle_t spi_mutex;

// AGC playback
// Initialize AGC
agc_t agc_custom = {
    .current_gain = 1.0f,
    .target_rms = TARGET_RMS,
    .attack_rate = AGC_ATTACK,
    .release_rate = AGC_RELEASE
};

// Calculate RMS of audio buffer
float calculate_rms(int16_t* buffer, size_t samples) {
    float sum = 0.0f;
    for (size_t i = 0; i < samples; i++) {
        float sample = (float)buffer[i];
        sum += sample * sample;
    }
    return sqrtf(sum / samples);
}

// Apply AGC to audio buffer
void apply_agc(int16_t* buffer, size_t samples, agc_t* agc) {
    // Calculate current RMS level
    float current_rms = calculate_rms(buffer, samples);
    
    // Avoid division by zero
    if (current_rms < 1.0f) current_rms = 1.0f;
    
    // Calculate desired gain
    float desired_gain = agc->target_rms / current_rms;
    
    // Apply attack/release smoothing
    float rate = (desired_gain < agc->current_gain) ? agc->attack_rate : agc->release_rate;
    agc->current_gain += rate * (desired_gain - agc->current_gain);
    
    // Clamp gain to reasonable limits
    if (agc->current_gain < MIN_GAIN) agc->current_gain = MIN_GAIN;
    if (agc->current_gain > MAX_GAIN) agc->current_gain = MAX_GAIN;
    
    // Apply gain to all samples
    for (size_t i = 0; i < samples; i++) {
        float sample = (float)buffer[i] * agc->current_gain;
        
        // Clamp to prevent overflow
        if (sample > 32767.0f) sample = 32767.0f;
        if (sample < -32768.0f) sample = -32768.0f;
        
        buffer[i] = (int16_t)sample;
    }
}

// Structure to hold received data
typedef struct
{
    uint8_t data[ESP_NOW_MAX_DATA_LEN_V2];
    size_t data_len;
} esp_now_recv_data_t;

// Callback function called when data is sent
static void esp_now_send_cb(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    if (status == ESP_NOW_SEND_SUCCESS)
    {
        // ESP_LOGI(TAG, "ESP-NOW data sent successfully");
    }
    else
    {
        ESP_LOGW(TAG, "ESP-NOW data send failed");
    }
}

void send_data_esp_now(const uint8_t *data, size_t len)
{
    size_t offset = 0;

    while (offset < len)
    {
        size_t chunk_size = len - offset > ESP_NOW_PACKET_SIZE ? ESP_NOW_PACKET_SIZE : len - offset;

        esp_err_t ret = esp_now_send(broadcast_mac, data + offset, chunk_size);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "ESP-NOW send failed at offset %zu: %s", offset, esp_err_to_name(ret));
        }
        else
        {
            // ESP_LOGI(TAG, "Sent %zu bytes of data via ESP-NOW (offset %zu)", chunk_size, offset);
        }

        offset += chunk_size;
        vTaskDelay(pdMS_TO_TICKS(16));
    }
}

// Get received data (non-blocking)
bool get_esp_now_data(esp_now_recv_data_t *recv_data)
{
    if (s_recv_queue == NULL || recv_data == NULL)
    {
        return false;
    }

    return (xQueueReceive(s_recv_queue, recv_data, 0) == pdTRUE);
}

// Fade in drawText
void fade_in_drawCount(void *arg)
{
    const char *input_text = (const char *)arg;
    if (input_text == NULL || strlen(input_text) == 0)
    {
        printf("Invalid text\n");
        vTaskDelete(NULL);
        return;
    }
    for (int i = 0; i < 15; i++)
    {
        if (state == 0)
        {
            spi_oled_drawText(&spi_ssd1327, 86, 46, &font_30, i / 2, input_text, 0);
            spi_oled_drawText(&spi_ssd1327, 85, 45, &font_30, i, input_text, 0);
        }
        else
        {
            vTaskDelete(NULL);
        }
        vTaskDelay(pdMS_TO_TICKS(1000 / 15));
    }
    vTaskDelete(NULL);
}

// Animation task function
static void animation_task(void *pvParameters)
{
    spi_oled_animation_t *anim = (spi_oled_animation_t *)pvParameters;
    if (anim == NULL)
    {
        // Handle error - invalid key
        printf("Invalid animation parameters\n");
        vTaskDelete(NULL);
        return;
    }
    int current_frame = 0;
    uint8_t bytes_per_row = (anim->width + 1) / 2; // 4bpp packing
    while ((anim->stop_frame == -1 && anim->is_playing == true) ||
           (anim->stop_frame != -1 && (anim->is_playing == true || current_frame != anim->stop_frame)))
    {
        if (anim->stop_frame == -1 && anim->is_playing == false)
        {
            break;
        }
        if (anim->stop_frame != -1 && anim->is_playing == false && (state == 3 || current_frame == anim->stop_frame))
            break; // Force break when state is 3
        // Calculate frame data offset
        const uint8_t *frame_data = anim->animation_data +
                                    (current_frame * anim->height * bytes_per_row);

        // Lock SPI access
        xSemaphoreTake(spi_mutex, portMAX_DELAY);

        // Draw current frame
        spi_oled_drawImage(&spi_ssd1327,
                           anim->x, anim->y,
                           anim->width, anim->height,
                           frame_data, SSD1327_GS_15);

        // Release SPI access
        xSemaphoreGive(spi_mutex);

        // Move to next frame
        if (anim->reverse == true)
        {
            current_frame--;
        }
        else
            current_frame++;
        if (current_frame >= anim->frame_count)
        {
            current_frame = 0;
        }
        else if (current_frame < 0)
        {
            current_frame = anim->frame_count - 1;
        }

        vTaskDelay(pdMS_TO_TICKS(anim->frame_delay_ms));
    }
    vTaskDelete(NULL);
}

void stopAllAnimation()
{
    anim_waveBar.is_playing = false;
    anim.is_playing = false;
    anim_speaking.is_playing = false;
    anim_receiving.is_playing = false;
    anim_idleWaveBar.is_playing = false;
    anim_idleBar.is_playing = false;
    anim_podcast.is_playing = false;
    anim_speaker.is_playing = false;
     if (anim_currentCommand != NULL)
                anim_currentCommand->is_playing = false;
}

void bubble_text_task(void *arg)
{
    const char *input_text = (const char *)arg;
    if (input_text == NULL || strlen(input_text) == 0)
    {
        printf("Invalid text for bubble task\n");
        vTaskDelete(NULL);
        return;
    }

    // Create a local copy to prevent corruption
    char text[64]; // Adjust size as needed
    strncpy(text, input_text, sizeof(text) - 1);
    text[sizeof(text) - 1] = '\0';

    for (int i = -6; i < 0; i++)
    {
        spi_oled_drawImage(&spi_ssd1327, 17, i, 93, 11, (const uint8_t *)text_bubble, SSD1327_GS_15);
        spi_oled_drawText(&spi_ssd1327, 18, i, &font_10, SSD1327_GS_1, text, 86);
        vTaskDelay(pdMS_TO_TICKS(1000 / 15));
    }
    vTaskDelete(NULL);
}

// ESP-NOW receive callback
static void esp_now_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int data_len)
{
    if (recv_info == NULL || data == NULL || data_len <= 0)
    {
        return;
    }

    // Log reception
    
    ESP_ERROR_CHECK(esp_wifi_connectionless_module_set_wake_interval(0));
    ESP_LOGI(TAG, "Received %d bytes, RSSI: %d dBm", data_len, recv_info->rx_ctrl->rssi);

    if (data_len == PING_MAGIC_LEN && memcmp(data, PING_MAGIC, PING_MAGIC_LEN) == 0) // PING MSG
    {
        int64_t now = esp_timer_get_time() / 1000; // ms
        bool found = false;
        ESP_LOGI(TAG, "Received PING from %02x:%02x:%02x:%02x:%02x:%02x",
                 recv_info->src_addr[0], recv_info->src_addr[1],
                 recv_info->src_addr[2], recv_info->src_addr[3],
                 recv_info->src_addr[4], recv_info->src_addr[5]);
        for (int i = 0; i < MAX_MAC_TRACK; ++i)
        {
            if (mac_track_list[i].valid &&
                memcmp(mac_track_list[i].mac, recv_info->src_addr, ESP_NOW_ETH_ALEN) == 0)
            {
                mac_track_list[i].last_seen_ms = now;
                found = true;
                break;
            }
        }

        if (!found)
        {
            for (int i = 0; i < MAX_MAC_TRACK; ++i)
            {
                if (!mac_track_list[i].valid)
                {
                    memcpy(mac_track_list[i].mac, recv_info->src_addr, ESP_NOW_ETH_ALEN);
                    mac_track_list[i].last_seen_ms = now;
                    mac_track_list[i].valid = true;
                    ESP_LOGI(TAG, "Added MAC");
                    macCount += 1;
                    break;
                }
            }
        }
    }
    // CMD: prefix handling
    else if (data_len >= 4 && memcmp(data, "CMD:", 4) == 0)
    {
        if (data_len >= 5)
        {
            // Extract the command parameter after "CMD:"
            char cmd_param[32]; // Adjust size as needed
            int param_len = data_len - 4;
            if (param_len >= sizeof(cmd_param))
            {
                param_len = sizeof(cmd_param) - 1;
            }
            memcpy(cmd_param, data + 4, param_len);
            cmd_param[param_len] = '\0';

            // Convert to integer and handle animation
            int cmd_value = atoi(cmd_param);
            ESP_LOGI(TAG, "Processed CMD: %d", cmd_value);

            // Handle animation for received command
            printf("Playing animation for received command_id: %d\n", cmd_value);
            if (anim_currentCommand != NULL)
                anim_currentCommand->is_playing = false;
            anim_currentCommand = get_animation_by_key(cmd_value);
            lastState = -1;
            is_command = true;
        }
    }
    // MSG: prefix handling
    else if (data_len >= 4 && memcmp(data, "MSG:", 4) == 0)
    {
        if (data_len >= 5)
        {
            // Extract the message content after "MSG:"
            char msg_content[ESP_NOW_MAX_DATA_LEN_V2]; // Adjust size as needed
            int content_len = data_len - 4;
            if (content_len >= sizeof(msg_content))
            {
                content_len = sizeof(msg_content) - 1;
            }
            memcpy(msg_content, data + 4, content_len);
            msg_content[content_len] = '\0';
            ESP_LOGI(TAG, "Processed MSG: %s", msg_content);

            // Create bubble text task for received message
            char *msg_copy = malloc(strlen(msg_content) + 1);
            if (msg_copy != NULL)
            {
                strcpy(msg_copy, msg_content);
                xTaskCreate(bubble_text_task, "bubbleText", 4096, msg_copy, 5, NULL);
                ESP_LOGI(TAG, "Created bubble_text_task for received message: %s", msg_content);
            }
            else
            {
                ESP_LOGE(TAG, "Failed to allocate memory for bubble text message");
            }
        }
    }
    // Others MSG, Store in queue if available
    else if (s_recv_queue != NULL)
    {
        is_receiving = true;
        esp_now_recv_data_t recv_data;

        // Copy data (with size check)
        size_t copy_len = (data_len > ESP_NOW_MAX_DATA_LEN_V2) ? ESP_NOW_MAX_DATA_LEN_V2 : data_len;
        memcpy(recv_data.data, data, copy_len);
        recv_data.data_len = copy_len;

        // Send to queue, don't block if full
        xQueueSend(s_recv_queue, &recv_data, 0);
    }
}

// Initialize ESP-NOW
bool init_esp_now()
{
    // Initialize WiFi
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "WiFi init failed: %s", esp_err_to_name(ret));
        return false;
    }
    
    // Set WiFi mode
    ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "WiFi set mode failed: %s", esp_err_to_name(ret));
        return false;
    }

    // Start WiFi
    ret = esp_wifi_start();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "WiFi start failed: %s", esp_err_to_name(ret));
        return false;
    }

    // Initialize ESP-NOW
    ret = esp_now_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "ESP-NOW init failed: %s", esp_err_to_name(ret));
        return false;
    }

    // Register callbacks
    esp_now_register_send_cb(esp_now_send_cb);
    esp_now_register_recv_cb(esp_now_recv_cb);

    // Add broadcast peer
    esp_now_peer_info_t peer_info = {0};
    memcpy(peer_info.peer_addr, broadcast_mac, ESP_NOW_ETH_ALEN);
    peer_info.channel = 0; // Use the same channel as AP
    peer_info.encrypt = false;

    ret = esp_now_add_peer(&peer_info);
    if (ret != ESP_OK && ret != ESP_ERR_ESPNOW_EXIST)
    {
        ESP_LOGE(TAG, "Add broadcast peer failed: %s", esp_err_to_name(ret));
        return false;
    }

    ESP_ERROR_CHECK(esp_now_set_wake_window(25));
    ESP_ERROR_CHECK(esp_wifi_connectionless_module_set_wake_interval(100));

    // Create receive queue (holds up to 10 messages)
    s_recv_queue = xQueueCreate(10, sizeof(esp_now_recv_data_t));

    ESP_LOGI(TAG, "ESP-NOW initialized successfully");
    return true;
}

void feed_Task(void *arg)
{
    esp_afe_sr_data_t *afe_data = arg;
    int audio_chunksize = afe_handle->get_feed_chunksize(afe_data);
    int nch = afe_handle->get_feed_channel_num(afe_data);
    int feed_channel = esp_get_feed_channel();
    printf("feed chunksize:%d, channel:%d\n", audio_chunksize, nch);
    assert(nch == feed_channel);
    int16_t *i2s_buff = malloc(audio_chunksize * sizeof(int16_t) * feed_channel);
    assert(i2s_buff);

    while (1)
    {
        esp_get_feed_data(true, i2s_buff, audio_chunksize * sizeof(int16_t) * feed_channel);
        afe_handle->feed(afe_data, i2s_buff);
    }
    if (i2s_buff)
    {
        free(i2s_buff);
        i2s_buff = NULL;
    }
    vTaskDelete(NULL);
}

void encode_adpcm(const int16_t *pcm_data, size_t pcm_len_bytes, uint8_t *adpcm_output, size_t *adpcm_len)
{
    // Register adpcm encoder
    esp_audio_enc_register_default();
    // Config
    esp_adpcm_enc_config_t adpcm_cfg = {
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = BIT_DEPTH,
        .channel = 1};

    esp_audio_enc_config_t enc_cfg = {
        .type = ESP_AUDIO_TYPE_ADPCM,
        .cfg = &adpcm_cfg,
        .cfg_sz = sizeof(adpcm_cfg)};

    esp_audio_enc_handle_t encoder;
    esp_audio_enc_open(&enc_cfg, &encoder);

    // Prepare I/O frames with aligned length
    esp_audio_enc_in_frame_t in_frame = {
        .buffer = (uint8_t *)pcm_data,
        .len = pcm_len_bytes};

    esp_audio_enc_out_frame_t out_frame = {
        .buffer = adpcm_output,
        .len = pcm_len_bytes / 4};

    // Encode
    esp_audio_enc_process(encoder, &in_frame, &out_frame);
    *adpcm_len = out_frame.encoded_bytes;
}

void decode_adpcm(const uint8_t *adpcm_data, size_t adpcm_len, uint8_t *pcm_output, size_t *pcm_len)
{
    // Register adpcm decoder
    esp_audio_dec_register_default();
    // Config
    esp_adpcm_dec_cfg_t adpcm_cfg = {
        .channel = 1};

    esp_audio_dec_cfg_t dec_cfg = {
        .type = ESP_AUDIO_TYPE_ADPCM,
        .cfg = &adpcm_cfg,
        .cfg_sz = sizeof(adpcm_cfg)};

    esp_audio_dec_handle_t decoder;
    esp_audio_dec_open(&dec_cfg, &decoder);

    // Prepare I/O frames with aligned length
    esp_audio_dec_in_raw_t raw = {
        .buffer = adpcm_data,
        .len = adpcm_len};

    esp_audio_dec_out_frame_t out_frame = {
        .buffer = (uint8_t *)pcm_output,
        .len = adpcm_len * 4}; // Assuming 16-bit PCM

    // Decode
    esp_audio_dec_process(decoder, &raw, &out_frame);
    *pcm_len = out_frame.len;
}

void decode_Task(void *arg)
{
    uint8_t *pcm_buffer = malloc(ENCODED_BUF_SIZE);
    size_t pcm_len = 0;

    TickType_t last_recv_time = xTaskGetTickCount();

    while (1)
    {
        esp_now_recv_data_t recv_data;
        if (get_esp_now_data(&recv_data))
        {
            is_receiving = true;
            last_recv_time = xTaskGetTickCount();

            //printf("Received %d bytes\n", recv_data.data_len);
            decode_adpcm(recv_data.data, recv_data.data_len, pcm_buffer, &pcm_len);
            size_t sent = xStreamBufferSend(play_stream_buf, pcm_buffer, pcm_len, 0);
        }
        else if (xTaskGetTickCount() - last_recv_time > pdMS_TO_TICKS(128))
        {
            is_receiving = false;

            ESP_ERROR_CHECK(esp_now_set_wake_window(25));
            ESP_ERROR_CHECK(esp_wifi_connectionless_module_set_wake_interval(100));

            const int silence_samples = 512;                                        // Adjust this number as needed
            int16_t *silence_buffer = calloc(silence_samples * 2, sizeof(int16_t)); // Already all zeros

            if (silence_buffer != NULL)
            {
                esp_err_t ret = esp_audio_play(silence_buffer, silence_samples, portMAX_DELAY);
                if (ret != ESP_OK)
                {
                    printf("Failed to end audio: %s", esp_err_to_name(ret));
                }
                free(silence_buffer);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(16));
    }
}

void detect_Task(void *arg)
{
    esp_afe_sr_data_t *afe_data = arg;
    int afe_chunksize = afe_handle->get_fetch_chunksize(afe_data);
    char *mn_name = esp_srmodel_filter(models, ESP_MN_PREFIX, ESP_MN_CHINESE);
    printf("multinet:%s\n", mn_name);
    esp_mn_iface_t *multinet = esp_mn_handle_from_name(mn_name);
    model_iface_data_t *model_data = multinet->create(mn_name, 1488);
    int mu_chunksize = multinet->get_samp_chunksize(model_data);
    printf("mu chunksize:%d, afe chunksize:%d\n", mu_chunksize, afe_chunksize);
    assert(mu_chunksize == afe_chunksize);
    int16_t *buff = malloc(afe_chunksize * sizeof(int16_t));
    multinet->print_active_speech_commands(model_data);
    printf("------------detect start------------\n");
    assert(buff);
    printf("------------vad start------------\n");

    uint8_t *adpcm_output = malloc(ENCODED_BUF_SIZE);
    esp_mn_state_t mn_state = ESP_MN_STATE_DETECTING;
    while (1)
    {
        afe_fetch_result_t *res = afe_handle->fetch(afe_data);
        if (!res || res->ret_value == ESP_FAIL)
        {
            printf("fetch error!\n");
            break;
        }

        // printf("vad state: %s\n",res->vad_state == VAD_SILENCE ? "noise" : "speech");

        // save speech data
        if (res->vad_state != VAD_SILENCE && !is_receiving && !isMicOff)
        {
            is_speaking = true;
            // Define a buffer for adpcm output
            size_t adpcm_len = 0;

            if (adpcm_output == NULL)
            {
                printf("Failed to allocate adpcm buffer\n");
                return;
            }

            if (res->vad_cache_size > 0)
            {
                // Make sure we have enough data for at least one frame
                encode_adpcm(res->vad_cache, res->vad_cache_size, adpcm_output, &adpcm_len);

                if (adpcm_len > 0)
                {
                    //printf("Encoded VAD cache: %zu bytes       Raw: %zu bytes\n", adpcm_len, res->vad_cache_size);
                    // send_data(adpcm_output, adpcm_len);
                    send_data_esp_now(adpcm_output, adpcm_len);
                }
                size_t num_chunks = res->vad_cache_size / (mu_chunksize * sizeof(int16_t));
                // printf("vad_cache_size: %zu , mu_chunksize: %d, num_chunks: %zu\n", res->vad_cache_size, mu_chunksize, num_chunks);
                for (size_t i = 0; i < num_chunks; i++)
                {
                    int16_t *chunk = res->vad_cache + (i * mu_chunksize);
                    mn_state = multinet->detect(model_data, chunk);
                }
            }

            if (res->vad_state == VAD_SPEECH)
            {
                // printf("vad_data_size: %zu\n", res->data_size);
                //   Make sure we have enough data for at least one frame
                adpcm_len = 0; // Reset for new encoding
                encode_adpcm(res->data, res->data_size, adpcm_output, &adpcm_len);

                if (adpcm_len > 0)
                {
                    // printf("Encoded speech data: %zu bytes      Raw: %zu bytes\n", adpcm_len, res->data_size);
                    send_data_esp_now(adpcm_output, adpcm_len);
                }
                mn_state = multinet->detect(model_data, res->data);
            }
            // MultiNet words detect
            if (mn_state == ESP_MN_STATE_DETECTING)
            {
                continue;
            }

            if (mn_state == ESP_MN_STATE_DETECTED)
            {
                esp_mn_results_t *mn_result = multinet->get_results(model_data);
                for (int i = 0; i < mn_result->num; i++)
                {
                    printf("TOP %d, command_id: %d, phrase_id: %d, string:%s prob: %f\n",
                           i + 1, mn_result->command_id[i], mn_result->phrase_id[i], mn_result->string, mn_result->prob[i]);
                }
                printf("Playing animation for command_id: %d\n", mn_result->command_id[0]);
                if (anim_currentCommand != NULL)
                    anim_currentCommand->is_playing = false;
                anim_currentCommand = get_animation_by_key(mn_result->command_id[0]);
                lastState = -1;
                is_command = true;

                // Send CMD via ESP-NOW
                char cmd_buffer[32];
                int cmd_len = snprintf(cmd_buffer, sizeof(cmd_buffer), "CMD:%d", mn_result->command_id[0]);
                if (cmd_len > 0 && cmd_len < sizeof(cmd_buffer))
                {
                    send_data_esp_now((const uint8_t *)cmd_buffer, cmd_len);
                    ESP_LOGI(TAG, "Sent CMD via ESP-NOW: %d", mn_result->command_id[0]);
                }
            }
            if (mn_state == ESP_MN_STATE_TIMEOUT)
            {
                esp_mn_results_t *mn_result = multinet->get_results(model_data);
                printf("timeout, string:%s\n", mn_result->string);
                xTaskCreate(bubble_text_task, "bubbleText", 4096, mn_result->string, 5, NULL);

                // Send MSG via ESP-NOW
                char msg_buffer[ESP_NOW_MAX_DATA_LEN_V2];
                int msg_len = snprintf(msg_buffer, sizeof(msg_buffer), "MSG:%s", mn_result->string);
                if (msg_len > 0 && msg_len < sizeof(msg_buffer))
                {
                    send_data_esp_now((const uint8_t *)msg_buffer, msg_len);
                    ESP_LOGI(TAG, "Sent timeout MSG via ESP-NOW: %s", mn_result->string);
                }
                continue;
            }
        }
        else
        {
            if (is_speaking == true)
            { // Call once per speaking
                printf("clean\n");
                multinet->clean(model_data);
            }
            is_speaking = false;
        }
    }
    if (buff)
    {
        free(buff);
        buff = NULL;
    }
    vTaskDelete(NULL);
}

void ping_task(void *arg)
{
    send_data_esp_now((const uint8_t *)PING_MAGIC, PING_MAGIC_LEN);
    // ping every 10 seconds
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(10000)); // 10 seconds
        send_data_esp_now((const uint8_t *)PING_MAGIC, PING_MAGIC_LEN);
    }
}

void byebye_anim(void *pvParameters)
{
    printf("byebye_anim\n");
    spi_oled_animation_t *anim = &anim_byebye;
    int current_frame = 0;
    uint8_t bytes_per_row = (anim->width + 1) / 2; // 4bpp packing
    int loop_count = 0;                            // Track which loop we're on

    for (int i = 0; i < 3 * anim->frame_count; i++) // Changed to 4 loops
    {
        const uint8_t *frame_data = anim->animation_data +
                                    (current_frame * anim->height * bytes_per_row);

        // Determine gray scale based on loop count
        uint8_t gray_scale;
        if (loop_count < 2)
        {
            gray_scale = SSD1327_GS_15; // Full brightness for first 3 loops
        }
        else
        {
            // Gradual fade during 4th loop: from 15 down to 0
            int fade_progress = anim->frame_count - current_frame - 1;
            gray_scale = (fade_progress * 15) / (anim->frame_count - 1);
            printf("Fade progress: %d, Gray scale: %d\n", fade_progress, gray_scale);
        }

        // Lock SPI access
        xSemaphoreTake(spi_mutex, portMAX_DELAY);
        // Draw current frame
        spi_oled_drawImage(&spi_ssd1327,
                           anim->x, anim->y,
                           anim->width, anim->height,
                           frame_data, gray_scale);
        // Release SPI access
        xSemaphoreGive(spi_mutex);

        // Move to next frame
        current_frame++;
        if (current_frame >= anim->frame_count)
        {
            current_frame = 0;
            loop_count++; // Increment loop counter when we complete a full animation cycle
        }
        vTaskDelay(pdMS_TO_TICKS(anim->frame_delay_ms));
    }
    // spi_oled_deinit(&spi_ssd1327);
    gpio_set_level(GPIO_NUM_3, 0);
    gpio_set_level(GPIO_NUM_9, 0);
    esp_deep_sleep_start();
    vTaskDelete(NULL);
}

void boot_sound(void *pvParameters)
{
    vTaskDelay(pdMS_TO_TICKS(650));
    esp_err_t ret = esp_audio_play((const int16_t *)boot, sizeof(boot) / 2, portMAX_DELAY);
    if (ret != ESP_OK)
    {
        printf("Failed to play boot sound: %s", esp_err_to_name(ret));
    }
    vTaskDelete(NULL);
}

void byebye_sound(void *pvParameters)
{
    esp_err_t ret = esp_audio_play((const int16_t *)byebye, sizeof(byebye) / 2, portMAX_DELAY);
    if (ret != ESP_OK)
    {
        printf("Failed to play byebye sound: %s", esp_err_to_name(ret));
    }
    vTaskDelete(NULL);
}

void i2s_writer_task(void *arg)
{
    uint8_t i2s_buf[PLAY_CHUNK_SIZE];
    while (1)
    {
        // Block until enough PCM bytes are available
        size_t received = xStreamBufferReceive(play_stream_buf, i2s_buf, PLAY_CHUNK_SIZE, portMAX_DELAY);
        if (received > 0 && !isMute)
        {
            // Apply AGC to the audio buffer
            apply_agc((int16_t*)i2s_buf, received / 2, &agc_custom);
            
            //printf("Write %zu bytes to I2S (gain: %.2f)\n", received, agc_custom.current_gain);
            esp_err_t ret = esp_audio_play((const int16_t *)i2s_buf, received / 2, portMAX_DELAY);
            if (ret != ESP_OK)
            {
                printf("Failed to play audio: %s", esp_err_to_name(ret));
            }
        }
    }
}

void init_audio_stream_buffer()
{
    play_stream_buf = xStreamBufferCreate(PLAY_RING_BUFFER_SIZE, 1); // Trigger level = 1
    assert(play_stream_buf);
}

void draw_status()
{
    if (!isMicOff)
    {
        spi_oled_drawImage(&spi_ssd1327, 0, 0, 5, 10, (const uint8_t *)mic_high, SSD1327_GS_15);
    }
    else
    {
        spi_oled_drawImage(&spi_ssd1327, 0, 0, 5, 10, (const uint8_t *)mic_off, SSD1327_GS_15);
    }
    if (!isMute)
    {
        spi_oled_drawImage(&spi_ssd1327, 6, 0, 9, 10, (const uint8_t *)volume_on, SSD1327_GS_15);
    }
    else
    {
        spi_oled_drawImage(&spi_ssd1327, 6, 0, 9, 10, (const uint8_t *)volume_off, SSD1327_GS_15);
    }
}

void setup_oled(){
        // This task is responsible for handling the OLED display
    spi_mutex = xSemaphoreCreateMutex();
    spi_bus_config_t spi_bus_cfg = {
        .miso_io_num = -1,
        .mosi_io_num = SPI_MOSI_PIN_NUM,
        .sclk_io_num = SPI_SCK_PIN_NUM,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };

    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &spi_bus_cfg, SPI_DMA_CH_AUTO));

    /* 2. Configure the spi device */
    spi_device_interface_config_t dev_cfg = {
        .clock_speed_hz = 10 * 1000 * 1000, // Clock out at 10 MHz
        .mode = 0,                          // SPI mode 0
        .spics_io_num = SPI_CS_PIN_NUM,     // CS pin
        .queue_size = 7,                    // We want to be able to queue 7 transactions at a time
    };

    ESP_ERROR_CHECK(spi_bus_add_device(SPI_HOST_TAG, &dev_cfg, &oled_dev_handle));

    /* 3. Initialize the remaining GPIO pins */
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << DC_PIN_NUM),
        .mode = GPIO_MODE_OUTPUT,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
    };
    gpio_config(&io_conf);

    gpio_config_t io_conf2 = {
        .pin_bit_mask = (1ULL << RST_PIN_NUM),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&io_conf2);

    spi_oled_init(&spi_ssd1327);
}

void batteryLevel_Task(void *pvParameters)
{
    // Better ADC config for battery monitoring
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_GPIO7_CHANNEL, ADC_ATTEN_DB_6); // 0-2.2V range, better for battery （4.2V max / 2 by resistors）

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << GPIO_NUM_4) | (1ULL << GPIO_NUM_5),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE};
    gpio_config(&io_conf);

    int prev_gpio4 = -1, prev_gpio5 = -1, prev_battery_level = -1;
    uint32_t last_battery_check = 0;
    uint32_t last_blink = 0;
    bool blink_state = false;

    while (!isShutdown)
    {
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;

        int gpio4 = gpio_get_level(GPIO_NUM_4);
        int gpio5 = gpio_get_level(GPIO_NUM_5);
        bool gpio_changed = (gpio4 != prev_gpio4 || gpio5 != prev_gpio5);

        int battery_level = prev_battery_level;
        if (now - last_battery_check >= 60000 || prev_battery_level == -1)
        {
            int adc_raw = adc1_get_raw(ADC1_GPIO7_CHANNEL);
            float voltage = (adc_raw * 2.2f / 4095.0f) * 2.0f; // Convert to actual battery voltage

            if (voltage >= 4.0f)
                battery_level = 4;
            else if (voltage >= 3.8f)
                battery_level = 3;
            else if (voltage >= 3.6f)
                battery_level = 2;
            else
                battery_level = 1;

            last_battery_check = now;
        }

        bool need_update = gpio_changed || (battery_level != prev_battery_level);
        const uint8_t *icons[] = {(const uint8_t *)battery_1, (const uint8_t *)battery_2, (const uint8_t *)battery_3, (const uint8_t *)battery_4};
        if (gpio5 == 0)
        {
            // Charge full
            if (need_update)
            {
                spi_oled_drawImage(&spi_ssd1327, 112, 0, 16, 10, (const uint8_t *)battery_full, SSD1327_GS_15);
            }
        }
        else if (gpio4 == 0)
        {
            // Charging - blink
            if (now - last_blink >= 500 || need_update)
            {
                blink_state = !blink_state;
                int show_level = (blink_state) ? battery_level -1 : battery_level - 2;
                spi_oled_drawImage(&spi_ssd1327, 112, 0, 16, 10, (const uint8_t *)icons[show_level], SSD1327_GS_15);
                last_blink = now;
            }
        }
        else
        {
            // Not charging
            if (need_update)
            {
                spi_oled_drawImage(&spi_ssd1327, 112, 0, 16, 10, (const uint8_t *)icons[battery_level - 1], SSD1327_GS_15);
            }
        }

        prev_gpio4 = gpio4;
        prev_gpio5 = gpio5;
        prev_battery_level = battery_level;

        vTaskDelay(pdMS_TO_TICKS(200));
    }
    vTaskDelete(NULL);
}

void oled_task(void *arg)
{
    setup_oled();
    printf("screen is on\n");
    spi_oled_framebuffer_clear(&spi_ssd1327, SSD1327_GS_0);
    for (size_t i = 32; i > 0; i--)
    {
        spi_oled_drawImage(&spi_ssd1327, 0, i, 128, 128, (const uint8_t *)logo, (32 - i) / 2);
        vTaskDelay(pdMS_TO_TICKS(1000 / 60));
    }
    printf("logo is painted\n");
    vTaskDelay(800 / portTICK_PERIOD_MS);
    spi_oled_framebuffer_clear(&spi_ssd1327, SSD1327_GS_0);
    spi_oled_drawText(&spi_ssd1327, 43, 0, &font_10, SSD1327_GS_5, "bbTalkie", 0);
    spi_oled_drawText(&spi_ssd1327, 44, 0, &font_10, SSD1327_GS_15, "bbTalkie", 0);
    draw_status();
    xTaskCreate(batteryLevel_Task, "battery", 4 * 1024, NULL, 5, NULL);

    bool isFirstBoot = true;

    while (!isShutdown)
    {
        if (is_command)
        {
            state = 3;
        }
        else if (is_receiving)
        {
            state = 2; // Receiving
        }
        else if (is_speaking)
        {
            state = 1; // Speaking
        }
        else
        {
            state = 0; // Idle
        }
        if (state == 0 && macCount != lastMacCount)
        {
            lastMacCount = macCount;
            // convert macCount from int to string
            char macCountStr[2]; // Enough space for int range + null terminator
            sprintf(macCountStr, "%d", macCount);
            spi_oled_draw_square(&spi_ssd1327, 74, 38, 36, 36, SSD1327_GS_0);
            xTaskCreate(fade_in_drawCount, "drawCount", 2048, macCountStr, 5, NULL);
        }
        if (state != lastState)
        {
            stopAllAnimation();
            spi_oled_draw_square(&spi_ssd1327, 0, 14, 128, 80, SSD1327_GS_0);
            lastState = state;
            switch (state)
            {
            case 0: // Idle
                anim.is_playing = true;
                lastMacCount = 0;
                xTaskCreate(animation_task, "idleSingleAnim", 2048, &anim, 5, &anim.task_handle);
                if (isFirstBoot)
                {
                    anim_idleBar.is_playing = true;
                    xTaskCreate(animation_task, "idleBarAnim", 2048, &anim_idleBar, 5, &anim_idleBar.task_handle);
                    isFirstBoot = false;
                }
                else
                {
                    anim_idleWaveBar.is_playing = true;
                    xTaskCreate(animation_task, "idleWaveBarAnim", 2048, &anim_idleWaveBar, 5, &anim_idleWaveBar.task_handle);
                }
                printf("Idle job create\n");
                break;
            case 1: // Speaking
                anim_waveBar.reverse = false;
                anim_waveBar.is_playing = true;
                anim_podcast.is_playing = true;
                anim_speaking.is_playing = true;
                xTaskCreate(animation_task, "podcastAnim", 2048, &anim_podcast, 5, &anim_podcast.task_handle);
                xTaskCreate(animation_task, "waveBarAnim", 2048, &anim_waveBar, 5, &anim_waveBar.task_handle);
                xTaskCreate(animation_task, "speakingAnim", 2048, &anim_speaking, 5, &anim_speaking.task_handle);
                break;
            case 2: // Receiving
                anim_waveBar.reverse = true;
                anim_waveBar.is_playing = true;
                anim_speaker.is_playing = true;
                anim_receiving.is_playing = true;
                xTaskCreate(animation_task, "speakerAnim", 2048, &anim_speaker, 5, &anim_speaker.task_handle);
                xTaskCreate(animation_task, "waveBarAnim", 2048, &anim_waveBar, 5, &anim_waveBar.task_handle);
                xTaskCreate(animation_task, "receivingAnim", 2048, &anim_receiving, 5, &anim_receiving.task_handle);
                break;
            case 3: // Command
                // stop idleBarAnim
                printf("Create command anim task\n");
                spi_oled_draw_square(&spi_ssd1327, 0, 14, 128, 114, SSD1327_GS_0);
                if(anim_currentCommand == NULL){
                    printf("No animation for this command\n");
                    is_command = false;
                    break;
                }
                anim_currentCommand->is_playing = true;
                xTaskCreate(animation_task, "customAnim", 2048, anim_currentCommand, 5, &anim_currentCommand->task_handle);
                break;
            }
            isFirstBoot = false;
        }
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

// ISR handler for button press
static void IRAM_ATTR button_isr_handler(void *arg)
{
    // Trigger restart from ISR
    esp_restart();
}

void charging_Task(void *pvParameters)
{
    // Configure GPIO8 (Button) for interrupt detection
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << GPIO_WAKEUP_2),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE // Trigger on falling edge (button press)
    };
    gpio_config(&io_conf);

    // Install GPIO ISR service if not already installed
    gpio_install_isr_service(0);

    // Add ISR handler for the button
    gpio_isr_handler_add(GPIO_WAKEUP_2, button_isr_handler, NULL);
    // Better ADC config for battery monitoring
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_GPIO7_CHANNEL, ADC_ATTEN_DB_6); // 0-2.2V range, better for battery （4.2V max / 2 by resistors）

    gpio_config_t io_conf2 = {
        .pin_bit_mask = (1ULL << GPIO_NUM_4) | (1ULL << GPIO_NUM_5),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE};
    gpio_config(&io_conf2);

    int prev_gpio4 = -1, prev_gpio5 = -1, prev_battery_level = -1;
    uint32_t last_battery_check = 0;
    uint32_t last_blink = 0;
    bool blink_state = false;
    setup_oled();
    spi_oled_framebuffer_clear(&spi_ssd1327, SSD1327_GS_0);
    while (1)
    {
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;

        int gpio4 = gpio_get_level(GPIO_NUM_4);
        int gpio5 = gpio_get_level(GPIO_NUM_5);
        bool gpio_changed = (gpio4 != prev_gpio4 || gpio5 != prev_gpio5);

        int battery_level = prev_battery_level;
        if (now - last_battery_check >= 60000 || prev_battery_level == -1)
        {
            int adc_raw = adc1_get_raw(ADC1_GPIO7_CHANNEL);
            float voltage = (adc_raw * 2.2f / 4095.0f) * 2.0f; // Convert to actual battery voltage

            if (voltage >= 4.0f)
                battery_level = 4;
            else if (voltage >= 3.8f)
                battery_level = 3;
            else if (voltage >= 3.6f)
                battery_level = 2;
            else
                battery_level = 1;

            last_battery_check = now;
        }

        bool need_update = gpio_changed || (battery_level != prev_battery_level);

        if (gpio5 == 0)
        {
            // Charge full
            if (need_update)
            {
                spi_oled_drawImage(&spi_ssd1327, 32, 44, 64, 40, (const uint8_t *)battery_large_full, SSD1327_GS_15);
                led_color_t charge_full_color = {0, 255, 0};
                set_led_color(charge_full_color);
            }
        }
        else if (gpio4 == 0)
        {
            // Charging - blink
            if (need_update)
            {
                led_color_t charging_color = {240, 150, 0};
                set_led_color(charging_color);
            }
            if (now - last_blink >= 500 || need_update)
            {
                blink_state = !blink_state;
                const uint8_t *icons[] = {(const uint8_t *)battery_large_1, (const uint8_t *)battery_large_2, (const uint8_t *)battery_large_3, (const uint8_t *)battery_large_4};
                int show_level = (blink_state) ? battery_level - 1: battery_level - 2;
                spi_oled_drawImage(&spi_ssd1327, 32, 44, 64, 40, (const uint8_t *)icons[show_level], SSD1327_GS_15);
                last_blink = now;
            }
        }
        else
        {
            // Not charging
            printf("not charging\n");
            gpio_set_level(GPIO_NUM_3, 0);
            gpio_set_level(GPIO_NUM_9, 0);
            esp_deep_sleep_start();
            vTaskDelete(NULL); // Exit the task if not charging
        }

        prev_gpio4 = gpio4;
        prev_gpio5 = gpio5;
        prev_battery_level = battery_level;

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

void ws2812_init(void)
{
    ESP_LOGI(TAG, "Initializing WS2812 LED strip");

    // LED strip general initialization
    led_strip_config_t strip_config = {
        .strip_gpio_num = WS2812_GPIO_PIN,
        .max_leds = WS2812_LED_COUNT,
        .led_model = LED_MODEL_WS2812,
        .flags.invert_out = false,
    };

    // LED strip RMT configuration
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
        .flags.with_dma = false,
    };

    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
}


// Smooth transition between two colors
static void transition_to_color(led_color_t from, led_color_t to)
{
    for (int step = 0; step <= TRANSITION_STEPS; step++)
    {
        float t = (float)step / TRANSITION_STEPS;

        led_color_t current = {
            .r = lerp_color(from.r, to.r, t),
            .g = lerp_color(from.g, to.g, t),
            .b = lerp_color(from.b, to.b, t)};

        set_led_color(current);
        vTaskDelay(pdMS_TO_TICKS(TRANSITION_DELAY_MS));

        // Check for state changes during transition
        if (is_speaking)
            break; // Priority to speaking state
    }
}

void led_control_task(void *pvParameters)
{
    ESP_LOGI(TAG, "LED control task started");

    // Define color states
    led_color_t darker_teal = {DARKER_TEAL_R, DARKER_TEAL_G, DARKER_TEAL_B};
    led_color_t receiving_green = {RECEIVING_GREEN_R, RECEIVING_GREEN_G, RECEIVING_GREEN_B};
    led_color_t speaking_blue = {SPEAKING_BLUE_R, SPEAKING_BLUE_G, SPEAKING_BLUE_B};

    // Current state tracking
    typedef enum
    {
        STATE_IDLE,
        STATE_RECEIVING,
        STATE_SPEAKING,
        STATE_TRANSITION_TO_RECEIVING,
        STATE_TRANSITION_TO_IDLE,
        STATE_TRANSITION_TO_SPEAKING
    } led_state_t;

    led_state_t current_state = STATE_IDLE;
    led_state_t previous_state = STATE_IDLE;
    led_color_t current_color = darker_teal;

    // Set initial color
    set_led_color(darker_teal);

    uint32_t breathing_start_time = 0;

    while (!isShutdown)
    {
        uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;

        // Determine desired state based on flags
        led_state_t desired_state;
        if (is_speaking)
        {
            desired_state = STATE_SPEAKING;
        }
        else if (is_receiving)
        {
            desired_state = STATE_RECEIVING;
        }
        else
        {
            desired_state = STATE_IDLE;
        }

        // Handle state transitions
        if (desired_state != current_state)
        {
            ESP_LOGI(TAG, "State change: %d -> %d", current_state, desired_state);
            previous_state = current_state;

            switch (desired_state)
            {
            case STATE_SPEAKING:
                current_state = STATE_TRANSITION_TO_SPEAKING;
                breathing_start_time = current_time; // Reset breathing timer for blue
                break;
            case STATE_RECEIVING:
                current_state = STATE_TRANSITION_TO_RECEIVING;
                breathing_start_time = current_time; // Reset breathing timer for green
                break;
            case STATE_IDLE:
                current_state = STATE_TRANSITION_TO_IDLE;
                break;
            default:
                break;
            }
        }

        // Handle current state
        switch (current_state)
        {
        case STATE_TRANSITION_TO_SPEAKING:
            transition_to_color(current_color, speaking_blue);
            current_color = speaking_blue;
            current_state = STATE_SPEAKING;
            break;

        case STATE_TRANSITION_TO_RECEIVING:
            transition_to_color(current_color, receiving_green);
            current_color = receiving_green;
            current_state = STATE_RECEIVING;
            break;

        case STATE_TRANSITION_TO_IDLE:
            transition_to_color(current_color, darker_teal);
            current_color = darker_teal;
            current_state = STATE_IDLE;
            break;

        case STATE_SPEAKING:
            // Breathing effect with blue color
            {
                float multiplier = breathing_multiplier(current_time - breathing_start_time);
                led_color_t breathing_color = {
                    .r = (uint8_t)(speaking_blue.r * multiplier),
                    .g = (uint8_t)(speaking_blue.g * multiplier),
                    .b = (uint8_t)(speaking_blue.b * multiplier)};
                set_led_color(breathing_color);
                vTaskDelay(pdMS_TO_TICKS(BREATHING_PERIOD_MS / BREATHING_STEPS));
            }
            break;

        case STATE_RECEIVING:
            // Breathing effect with green color
            {
                float multiplier = breathing_multiplier(current_time - breathing_start_time);
                led_color_t breathing_color = {
                    .r = (uint8_t)(receiving_green.r * multiplier),
                    .g = (uint8_t)(receiving_green.g * multiplier),
                    .b = (uint8_t)(receiving_green.b * multiplier)};
                set_led_color(breathing_color);
                vTaskDelay(pdMS_TO_TICKS(BREATHING_PERIOD_MS / BREATHING_STEPS));
            }
            break;

        case STATE_IDLE:
        default:
            // Solid darker teal
            set_led_color(darker_teal);
            vTaskDelay(pdMS_TO_TICKS(50));
            break;
        }
    }
    vTaskDelete(NULL);
}

static void cleanup_button(void)
{
    if (btn != NULL && button_initialized) {
        iot_button_delete(btn);
        btn = NULL;
        button_initialized = false;
        printf("Button cleaned up\n");
    }
}

static void button_long_press_cb(void *arg, void *usr_data)
{
    printf("Long press detected! Entering deep sleep mode...\n");
    
    
    isShutdown = true;
    stopAllAnimation();
    vTaskDelay(pdMS_TO_TICKS(100));
    
    xTaskCreate(byebye_sound, "byebyeSound", 4 * 1024, NULL, 5, NULL);
    xTaskCreate(byebye_anim, "byebyeAnim", 4 * 1024, NULL, 5, NULL);
    
}

static void button_single_click_cb(void *arg, void *usr_data)
{
    printf("Single click\n");
    if(is_command){
        is_command = false;
    }
    else{
        isMicOff = !isMicOff;
    }
    draw_status();
}

static void button_double_click_cb(void *arg, void *usr_data)
{
    printf("Double click\n");
    isMute = !isMute;
    draw_status();
}

static esp_err_t init_button(void)
{
    // Clean up any existing button first
    cleanup_button();
    
    // Reset the button GPIO explicitly
    gpio_reset_pin(BUTTON_GPIO);
    vTaskDelay(pdMS_TO_TICKS(10)); // Small delay for GPIO to settle
    
    // Initialize the button
    button_config_t btn_cfg = {0};
    button_gpio_config_t gpio_cfg = {
        .gpio_num = BUTTON_GPIO,
        .active_level = BUTTON_ACTIVE_LEVEL,
        .enable_power_save = true,
    };

    esp_err_t ret = iot_button_new_gpio_device(&btn_cfg, &gpio_cfg, &btn);
    if (ret != ESP_OK) {
        printf("Failed to create button device: %s\n", esp_err_to_name(ret));
        return ret;
    }

    // Register event callbacks
    ret = iot_button_register_cb(btn, BUTTON_LONG_PRESS_START, NULL, button_long_press_cb, NULL);
    if (ret != ESP_OK) {
        printf("Failed to register long press callback\n");
        cleanup_button();
        return ret;
    }
    
    ret = iot_button_register_cb(btn, BUTTON_SINGLE_CLICK, NULL, button_single_click_cb, NULL);
    if (ret != ESP_OK) {
        printf("Failed to register single click callback\n");
        cleanup_button();
        return ret;
    }
    
    ret = iot_button_register_cb(btn, BUTTON_DOUBLE_CLICK, NULL, button_double_click_cb, NULL);
    if (ret != ESP_OK) {
        printf("Failed to register double click callback\n");
        cleanup_button();
        return ret;
    }

    button_initialized = true;
    printf("Button initialized successfully\n");
    return ESP_OK;
}

void app_main()
{
    // Check which GPIO caused the wakeup (if any)
    ws2812_init();
    uint64_t wakeup_pin_mask = esp_sleep_get_ext1_wakeup_status();
    
    // Reset critical flags first
    isShutdown = false;
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    init_esp_now();

    ESP_ERROR_CHECK(esp_board_init(SAMPLE_RATE, 1, BIT_DEPTH));

    models = esp_srmodel_init("model");
    afe_config_t *afe_config = afe_config_init(esp_get_input_format(), models, AFE_TYPE_SR, AFE_MODE_LOW_COST);
    
    afe_config->vad_min_noise_ms = 800;
    afe_config->vad_min_speech_ms = 128;
    afe_config->vad_mode = VAD_MODE_1;  // The larger the mode, the higher the speech trigger probability.
    afe_config->afe_linear_gain = 2.0;

    afe_handle = esp_afe_handle_from_config(afe_config);
    esp_afe_sr_data_t *afe_data = afe_handle->create_from_config(afe_config);
    afe_config_free(afe_config);

    printf("afe_linear_gain:%f\n", afe_config->afe_linear_gain);
    printf("agc_init:%d, agc_mode:%d, agc_compression_gain_db:%d, agc_target_level_dbfs:%d\n", 
           afe_config->agc_init, afe_config->agc_mode, afe_config->agc_compression_gain_db, afe_config->agc_target_level_dbfs);

    // Configure output GPIOs first
    gpio_config_t io_conf_3 = {
        .pin_bit_mask = (1ULL << GPIO_NUM_3),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf_3);

    gpio_config_t io_conf_9 = {
        .pin_bit_mask = (1ULL << GPIO_NUM_9),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf_9);

    gpio_set_level(GPIO_NUM_3, 1);
    gpio_set_level(GPIO_NUM_9, 1);

    // Configure wake up GPIOs
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << GPIO_WAKEUP_1) | (1ULL << GPIO_WAKEUP_2) | (1ULL << GPIO_WAKEUP_3),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE};
    gpio_config(&io_conf);

    rtc_gpio_pullup_en(GPIO_WAKEUP_1);
    rtc_gpio_pullup_en(GPIO_WAKEUP_2);
    rtc_gpio_pullup_en(GPIO_WAKEUP_3);

    rtc_gpio_pulldown_dis(GPIO_WAKEUP_1);
    rtc_gpio_pulldown_dis(GPIO_WAKEUP_2);
    rtc_gpio_pulldown_dis(GPIO_WAKEUP_3);

    esp_sleep_enable_ext1_wakeup((1ULL << GPIO_WAKEUP_1) | (1ULL << GPIO_WAKEUP_2) | (1ULL << GPIO_WAKEUP_3), ESP_EXT1_WAKEUP_ANY_LOW);

    // Handle charging wake-up
    if ((wakeup_pin_mask & (1ULL << GPIO_WAKEUP_1)) || (wakeup_pin_mask & (1ULL << GPIO_WAKEUP_3)))
    {
        printf("Wakeup caused by GPIO4 (Charger CHRG) / GPIO5 (Charger STDBY)\n");
        xTaskCreatePinnedToCore(charging_Task, "charging", 4 * 1024, NULL, 5, NULL, 0);
        return;
    }
    
    // Handle button wake-up
    if (wakeup_pin_mask & (1ULL << GPIO_WAKEUP_2))
    {
        printf("Wakeup caused by GPIO8 (Button)\n");
        // Add a small delay to let the GPIO settle after wake-up
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    // Initialize button AFTER all GPIO configurations and wake-up handling
    if (init_button() != ESP_OK) {
        printf("Failed to initialize button, retrying...\n");
        vTaskDelay(pdMS_TO_TICKS(500));
        if (init_button() != ESP_OK) {
            printf("Button initialization failed permanently\n");
            // You might want to handle this error case
        }
    }

    // Continue with the rest of your initialization
    init_audio_stream_buffer();
    xTaskCreatePinnedToCore(oled_task, "oled", 4 * 1024, NULL, 5, NULL, 0);
    xTaskCreatePinnedToCore(boot_sound, "bootSound", 3 * 1024, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(&feed_Task, "feed", 8 * 1024, (void *)afe_data, 5, NULL, 0);
    xTaskCreatePinnedToCore(&detect_Task, "detect", 4 * 1024, (void *)afe_data, 5, NULL, 1);
    xTaskCreatePinnedToCore(decode_Task, "decode", 4 * 1024, NULL, 5, NULL, 0);
    xTaskCreatePinnedToCore(i2s_writer_task, "i2sWriter", 4 * 1024, NULL, 5, NULL, 0);
    xTaskCreate(ping_task, "ping", 3 * 1024, NULL, 5, NULL);
    xTaskCreate(led_control_task, "led_control", 3 * 1024, NULL, 5, NULL);
}