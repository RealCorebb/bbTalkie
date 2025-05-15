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
#include "m_1.h"
#include "esp_audio_enc.h"
#include "esp_audio_enc_reg.h"
#include "esp_audio_enc_default.h"
#include "esp_audio_dec_default.h"
#include "esp_audio_dec.h"
#include "esp_g711_enc.h"
#include "esp_g711_dec.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "esp_system.h"
#include "esp_now.h"
#include "esp_wifi.h"
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
#include "include/images/test.h"

#define SAMPLE_RATE 16000
#define BIT_DEPTH 16
#define ENCODED_BUF_SIZE 10240
#define PLAY_RING_BUFFER_SIZE 8192
#define PLAY_CHUNK_SIZE 2048
#define ESP_NOW_PACKET_SIZE 512
#define SERVER_IP "192.168.68.87" // Your PC's IP address
#define SERVER_PORT 8765          // Port to communicate on

#define SPI_MOSI_PIN_NUM 14
#define SPI_SCK_PIN_NUM  13
#define SPI_CS_PIN_NUM   10
#define DC_PIN_NUM   12
#define RST_PIN_NUM  11
#define SPI_HOST_TAG SPI2_HOST


static esp_afe_sr_iface_t *afe_handle = NULL;
StreamBufferHandle_t play_stream_buf;
static QueueHandle_t s_recv_queue = NULL;
static uint8_t broadcast_mac[ESP_NOW_ETH_ALEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}; // Broadcast MAC address (all ones)
volatile bool is_receiving = false;
static const char *TAG = "bbTalkie";

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
        ESP_LOGI(TAG, "ESP-NOW data sent successfully");
    }
    else
    {
        ESP_LOGW(TAG, "ESP-NOW data send failed");
    }
}

// ESP-NOW receive callback
static void esp_now_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int data_len)
{
    if (recv_info == NULL || data == NULL || data_len <= 0)
    {
        return;
    }

    // Log reception
    is_receiving = true;
    ESP_LOGI(TAG, "Received %d bytes", data_len);

    // Store in queue if available
    if (s_recv_queue != NULL)
    {
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

    // Create receive queue (holds up to 10 messages)
    s_recv_queue = xQueueCreate(10, sizeof(esp_now_recv_data_t));

    ESP_LOGI(TAG, "ESP-NOW initialized successfully");
    return true;
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
            ESP_LOGI(TAG, "Sent %zu bytes of data via ESP-NOW (offset %zu)", chunk_size, offset);
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

void encode_g711(const int16_t *pcm_data, size_t pcm_len_bytes, uint8_t *g711_output, size_t *g711_len)
{
    // Register g711 encoder
    esp_audio_enc_register_default();
    // Config
    esp_g711_enc_config_t g711_cfg = {
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = BIT_DEPTH,
        .channel = 1,
        .frame_duration = 32};

    esp_audio_enc_config_t enc_cfg = {
        .type = ESP_AUDIO_TYPE_G711A,
        .cfg = &g711_cfg,
        .cfg_sz = sizeof(g711_cfg)};

    esp_audio_enc_handle_t encoder;
    esp_audio_enc_open(&enc_cfg, &encoder);

    // Prepare I/O frames with aligned length
    esp_audio_enc_in_frame_t in_frame = {
        .buffer = (uint8_t *)pcm_data,
        .len = pcm_len_bytes};

    esp_audio_enc_out_frame_t out_frame = {
        .buffer = g711_output,
        .len = pcm_len_bytes / 2};

    // Encode
    esp_audio_enc_process(encoder, &in_frame, &out_frame);
    *g711_len = out_frame.encoded_bytes;
}

void decode_g711(const uint8_t *g711_data, size_t g711_len, uint8_t *pcm_output, size_t *pcm_len)
{
    // Register g711 decoder
    esp_audio_dec_register_default();
    // Config
    esp_g711_dec_cfg_t g711_cfg = {
        .channel = 1};

    esp_audio_dec_cfg_t dec_cfg = {
        .type = ESP_AUDIO_TYPE_G711A,
        .cfg = &g711_cfg,
        .cfg_sz = sizeof(g711_cfg)};

    esp_audio_dec_handle_t decoder;
    esp_audio_dec_open(&dec_cfg, &decoder);

    // Prepare I/O frames with aligned length
    esp_audio_dec_in_raw_t raw = {
        .buffer = g711_data,
        .len = g711_len};

    esp_audio_dec_out_frame_t out_frame = {
        .buffer = (uint8_t *)pcm_output,
        .len = g711_len * 2}; // Assuming 16-bit PCM

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

            printf("Received %d bytes\n", recv_data.data_len);
            decode_g711(recv_data.data, recv_data.data_len, pcm_buffer, &pcm_len);
            size_t sent = xStreamBufferSend(play_stream_buf, pcm_buffer, pcm_len, 0);
        }
        else if (xTaskGetTickCount() - last_recv_time > pdMS_TO_TICKS(128))
        {
            is_receiving = false;




            const int silence_samples = 512; // Adjust this number as needed
            int16_t* silence_buffer = calloc(silence_samples * 2, sizeof(int16_t)); // Already all zeros
            
            if (silence_buffer != NULL) {
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
    printf("detect chunksize:%d\n", afe_chunksize);
    int16_t *buff = malloc(afe_chunksize * sizeof(int16_t));
    assert(buff);
    printf("------------vad start------------\n");

    uint8_t *g711_output = malloc(ENCODED_BUF_SIZE);

    while (1)
    {
        afe_fetch_result_t *res = afe_handle->fetch(afe_data);
        if (!res || res->ret_value == ESP_FAIL)
        {
            printf("fetch error!\n");
            break;
        }

        // Print the audio data values
        /*       int samples = res->data_size / sizeof(int16_t);
              for (int i = 0; i < samples; i++) {
                  printf("OUT=%d\n", res->data[i]);
              } */

        // printf("vad state: %s\n",res->vad_state == VAD_SILENCE ? "noise" : "speech");

        // save speech data
        if (res->vad_state != VAD_SILENCE && !is_receiving)
        {

            // Define a buffer for g711 output
            size_t g711_len = 0;

            if (g711_output == NULL)
            {
                printf("Failed to allocate g711 buffer\n");
                return;
            }

            if (res->vad_cache_size > 0)
            {
                // send_data((const uint8_t *)res->vad_cache, res->vad_cache_size);
                //  Make sure we have enough data for at least one frame
                encode_g711(res->vad_cache, res->vad_cache_size, g711_output, &g711_len);

                if (g711_len > 0)
                {
                    printf("Encoded VAD cache: %zu bytes       Raw: %zu bytes\n", g711_len, res->vad_cache_size);
                    // send_data(g711_output, g711_len);
                    send_data_esp_now(g711_output, g711_len);
                }
            }

            if (res->vad_state == VAD_SPEECH)
            {
                // send_data((const uint8_t *)res->data, res->data_size);
                //  Make sure we have enough data for at least one frame

                g711_len = 0; // Reset for new encoding
                encode_g711(res->data, res->data_size, g711_output, &g711_len);

                if (g711_len > 0)
                {
                    printf("Encoded speech data: %zu bytes      Raw: %zu bytes\n", g711_len, res->data_size);
                    // send_data(g711_output, g711_len);
                    send_data_esp_now(g711_output, g711_len);
                }
            }
        }
    }
    if (buff)
    {
        free(buff);
        buff = NULL;
    }
    vTaskDelete(NULL);
}

void play_audio_task(void *pvParameters)
{
    // This task is responsible for playing the audio
    while (1)
    {
        esp_err_t ret = esp_audio_play((const int16_t *)m_1, sizeof(m_1) / 2, portMAX_DELAY);
        if (ret != ESP_OK)
        {
            printf("Failed to play audio: %s", esp_err_to_name(ret));
        }
        vTaskDelay(pdMS_TO_TICKS(5000)); // Delay to avoid busy-waiting
    }
}

void i2s_writer_task(void *arg)
{
    uint8_t i2s_buf[PLAY_CHUNK_SIZE];

    while (1)
    {
        // Block until enough PCM bytes are available
        size_t received = xStreamBufferReceive(play_stream_buf, i2s_buf, PLAY_CHUNK_SIZE, portMAX_DELAY);

        if (received > 0)
        {
            printf("Write %zu bytes to I2S\n", received);
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

void oled_task(void *arg)
{
    // This task is responsible for handling the OLED display
    
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
        .clock_speed_hz = 10 * 1000 * 1000,      // Clock out at 10 MHz
        .mode = 0,                               // SPI mode 0
        .spics_io_num = SPI_CS_PIN_NUM,          // CS pin
        .queue_size = 7,                         // We want to be able to queue 7 transactions at a time
    };

    spi_device_handle_t oled_dev_handle;
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

    /* 4. Create SSD1327 struct for use of the spi_oled functions */
    struct spi_ssd1327 spi_ssd1327 = {
        .dc_pin_num = DC_PIN_NUM,
        .rst_pin_num = RST_PIN_NUM,
        .spi_handle = &oled_dev_handle,
    };
    /* }}} */

    spi_oled_init(&spi_ssd1327);

    printf("screen is on\n");

    vTaskDelay(1000 / portTICK_PERIOD_MS);
    spi_oled_draw_square(&spi_ssd1327, 0, 0, 128, 128, SSD1327_GS_0);
    vTaskDelay(100 / portTICK_PERIOD_MS);

    while (1)
    {
        spi_oled_drawImage(&spi_ssd1327, 0, 0, 128, 128, (const uint8_t *)logo);
        printf("logo is painted\n");
        vTaskDelay(1500 / portTICK_PERIOD_MS);

        spi_oled_draw_circle(&spi_ssd1327, 0, 0);
        printf("(1/4) White 16 pixel radius circle is painted\n");
        vTaskDelay(1500 / portTICK_PERIOD_MS);

        spi_oled_draw_square(&spi_ssd1327, 0, 0, 128, 128, SSD1327_GS_0);
        printf("(2/4) Black 128x128 square painted\n");
        vTaskDelay(500 / portTICK_PERIOD_MS);

        spi_oled_draw_square(&spi_ssd1327, 0, 0, 64, 64, SSD1327_GS_15);
        printf("(3/4) White 128x128 square painted\n");
        vTaskDelay(1500 / portTICK_PERIOD_MS);
    }
}

void app_main()
{

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

    srmodel_list_t *models = esp_srmodel_init("model");
    afe_config_t *afe_config = afe_config_init(esp_get_input_format(), models, AFE_TYPE_SR, AFE_MODE_LOW_COST);
/*     afe_config->agc_init = true; // Enable AGC
    afe_config->agc_mode = AFE_AGC_MODE_WAKENET; // Use WEBRTC AGC
    afe_config->agc_compression_gain_db = 32; // The maximum gain of AGC
    afe_config->agc_target_level_dbfs = 1; // The target level of AGC */
    afe_config->vad_min_noise_ms = 1000; // The minimum duration of noise or silence in ms.
    afe_config->vad_min_speech_ms = 128; // The minimum duration of speech in ms.
    afe_config->vad_mode = VAD_MODE_1;   // The larger the mode, the higher the speech trigger probability.
    afe_config->afe_linear_gain = 3.0;

    afe_handle = esp_afe_handle_from_config(afe_config);
    esp_afe_sr_data_t *afe_data = afe_handle->create_from_config(afe_config);
    afe_config_free(afe_config);

    //printf afe_config->afe_linear_gain
    printf("afe_linear_gain:%f\n", afe_config->afe_linear_gain);

    //printf afe_config->agc_init andafe_config_.agc_mode and afe_config->agc_compression_gain_db and afe_config->agc_target_level_dbfs
    printf("agc_init:%d, agc_mode:%d, agc_compression_gain_db:%d, agc_target_level_dbfs:%d\n", afe_config->agc_init, afe_config->agc_mode, afe_config->agc_compression_gain_db, afe_config->agc_target_level_dbfs);

    // digitalWrite pin 5 to high
    // set GPIO_NUM_5 to output
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << GPIO_NUM_6),  // Select GPIO6
        .mode = GPIO_MODE_OUTPUT,              // Set as output mode
        .pull_up_en = GPIO_PULLUP_DISABLE,     // Disable pull-up
        .pull_down_en = GPIO_PULLDOWN_DISABLE, // Disable pull-down
        .intr_type = GPIO_INTR_DISABLE,        // Disable interrupt
    };
    gpio_config(&io_conf);

    // Set GPIO6 to high
    gpio_set_level(GPIO_NUM_6, 1);

    init_audio_stream_buffer();
    // esp_audio_play((int16_t*)m_1, sizeof(m_1), portMAX_DELAY);
    xTaskCreatePinnedToCore(&feed_Task, "feed", 8 * 1024, (void *)afe_data, 5, NULL, 0);
    xTaskCreatePinnedToCore(&detect_Task, "detect", 4 * 1024, (void *)afe_data, 5, NULL, 1);
    //xTaskCreatePinnedToCore(play_audio_task, "music", 4 * 1024, NULL, 5, NULL, 0);
    xTaskCreatePinnedToCore(decode_Task, "decode", 4 * 1024, NULL, 5, NULL, 0);
    xTaskCreatePinnedToCore(i2s_writer_task, "i2sWriter", 4 * 1024, NULL, 5, NULL, 0);
    xTaskCreatePinnedToCore(oled_task, "oled", 4 * 1024, NULL, 5, NULL, 0);

}
