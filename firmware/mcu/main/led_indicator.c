#include "led_indicator.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define LED_GPIO 2

void led_indicator_init(void)
{
    gpio_config_t io = {
        .intr_type    = GPIO_INTR_DISABLE,
        .mode         = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << LED_GPIO),
        .pull_down_en = 0,
        .pull_up_en   = 0,
    };
    gpio_config(&io);
    gpio_set_level(LED_GPIO, 0);
}

void led_set(bool on)
{
    gpio_set_level(LED_GPIO, on ? 1 : 0);
}

void led_blink(int times, int period_ms)
{
    int half = period_ms / 2;
    if (half < 1) half = 1;

    for (int i = 0; i < times; i++) {
        gpio_set_level(LED_GPIO, 1);
        vTaskDelay(pdMS_TO_TICKS(half));
        gpio_set_level(LED_GPIO, 0);
        vTaskDelay(pdMS_TO_TICKS(half));
    }
}
