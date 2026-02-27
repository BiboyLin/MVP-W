/**
 * HAL Button Driver for MVP-W
 * Encoder button on GPIO 41 (active low)
 */
#include "hal_button.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "HAL_BUTTON"

/* Encoder button GPIO (from sensecap-watcher.h) */
#define BUTTON_GPIO     41

/* Debounce time in ms */
#define DEBOUNCE_MS     50

static button_callback_t g_callback = NULL;
static bool g_is_pressed = false;
static int64_t g_last_irq_time = 0;

static void IRAM_ATTR button_isr_handler(void *arg)
{
    (void)arg;
    int64_t now = esp_timer_get_time() / 1000;  /* Convert to ms */

    /* Debounce */
    if (now - g_last_irq_time < DEBOUNCE_MS) {
        return;
    }
    g_last_irq_time = now;

    /* Read current level (active low: 0 = pressed) */
    int level = gpio_get_level(BUTTON_GPIO);
    g_is_pressed = (level == 0);

    /* Call callback outside ISR context would need a task,
     * but for simplicity we call it directly (callback should be short) */
    if (g_callback) {
        g_callback(g_is_pressed);
    }
}

int hal_button_init(button_callback_t callback)
{
    ESP_LOGI(TAG, "Initializing button on GPIO %d", BUTTON_GPIO);

    g_callback = callback;
    g_is_pressed = false;
    g_last_irq_time = 0;

    /* Configure GPIO */
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE,  /* Interrupt on both edges */
    };

    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "GPIO config failed: %s", esp_err_to_name(ret));
        return -1;
    }

    /* Install ISR service if not already installed */
    ret = gpio_install_isr_service(0);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        /* ESP_ERR_INVALID_STATE means already installed, which is OK */
        ESP_LOGE(TAG, "ISR service install failed: %s", esp_err_to_name(ret));
        return -1;
    }

    /* Add ISR handler */
    ret = gpio_isr_handler_add(BUTTON_GPIO, button_isr_handler, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ISR handler add failed: %s", esp_err_to_name(ret));
        return -1;
    }

    /* Read initial state */
    g_is_pressed = (gpio_get_level(BUTTON_GPIO) == 0);

    ESP_LOGI(TAG, "Button initialized, initial state: %s",
             g_is_pressed ? "pressed" : "released");
    return 0;
}

bool hal_button_is_pressed(void)
{
    return g_is_pressed;
}

void hal_button_deinit(void)
{
    gpio_isr_handler_remove(BUTTON_GPIO);
    gpio_reset_pin(BUTTON_GPIO);
    g_callback = NULL;
}
