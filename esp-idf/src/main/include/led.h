#include "led_strip.h"
#define WS2812_GPIO_PIN 15
#define WS2812_LED_COUNT 1
static led_strip_handle_t led_strip;

// LED state structure
typedef struct
{
    uint8_t r, g, b;
} led_color_t;

// Color definitions
#define DARKER_TEAL_R 32
#define DARKER_TEAL_G 150
#define DARKER_TEAL_B 32

#define RECEIVING_GREEN_R 32
#define RECEIVING_GREEN_G 32
#define RECEIVING_GREEN_B 255

#define SPEAKING_BLUE_R 255
#define SPEAKING_BLUE_G 30
#define SPEAKING_BLUE_B 0

// Animation parameters
#define TRANSITION_STEPS 10      // Steps for smooth transition
#define TRANSITION_DELAY_MS 20   // Delay between transition steps
#define BREATHING_PERIOD_MS 1000 // Full breathing cycle duration
#define BREATHING_STEPS 50       // Steps in breathing cycle

// Linear interpolation between two values
static uint8_t lerp_color(uint8_t start, uint8_t end, float t)
{
    return (uint8_t)(start + t * (end - start));
}

// Calculate breathing effect multiplier (sine wave based)
static float breathing_multiplier(uint32_t time_ms)
{
    float phase = (float)(time_ms % BREATHING_PERIOD_MS) / BREATHING_PERIOD_MS;
    // Use sine wave for smooth breathing, ranging from 0.3 to 1.0
    return 0.3f + 0.7f * (1.0f + sinf(2.0f * M_PI * phase)) / 2.0f;
}

// Apply color to LED strip
static void set_led_color(led_color_t color)
{
    ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, 0, color.r, color.g, color.b));
    ESP_ERROR_CHECK(led_strip_refresh(led_strip));
}