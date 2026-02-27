/**
 * HAL Button Driver for MVP-W
 *
 * NOTE: The button is connected via I2C IO Expander (PCA9535), not direct GPIO!
 * GPIO 41/42 are for encoder rotation (A/B phases).
 *
 * For MVP, we use polling mode via I2C IO expander.
 */
#include "hal_button.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "HAL_BUTTON"

/* IO Expander I2C config (from sensecap-watcher.h) */
#define IO_EXPANDER_I2C_NUM     I2C_NUM_0   /* Use I2C_NUM_0 (General I2C) */
#define IO_EXPANDER_I2C_ADDR    0x24  /* PCA9535 address */
#define IO_EXPANDER_SDA         47
#define IO_EXPANDER_SCL         48
#define IO_EXPANDER_INT         2

/* Button is on IO Expander pin 3 */
#define BUTTON_PIN_MASK         (1 << 3)

/* Debounce time in ms */
#define DEBOUNCE_MS             50

static button_callback_t g_callback = NULL;
static bool g_is_pressed = false;
static int64_t g_last_change_time = 0;
static bool g_i2c_initialized = false;

/* Read button state from IO expander */
static int read_button_state(bool *pressed)
{
    uint8_t data = {0};

    /* Read input port 0 (pins 0-7) */
    uint8_t reg = 0x00;  /* Input Port 0 register */

    esp_err_t ret = i2c_master_write_read_device(
        IO_EXPANDER_I2C_NUM,
        IO_EXPANDER_I2C_ADDR,
        &reg, 1,
        data, 1,
        pdMS_TO_TICKS(100)
    );

    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "I2C read failed: %s", esp_err_to_name(ret));
        return -1;
    }

    /* Button is active low (0 = pressed) */
    *pressed = !(data[0] & BUTTON_PIN_MASK);
    return 0;
}

/* Poll button state (called from task context) */
void hal_button_poll(void)
{
    bool current_pressed;
    if (read_button_state(&current_pressed) != 0) {
        return;
    }

    int64_t now = esp_timer_get_time() / 1000;  /* ms */

    /* Check for state change with debounce */
    if (current_pressed != g_is_pressed) {
        if (now - g_last_change_time >= DEBOUNCE_MS) {
            g_is_pressed = current_pressed;
            g_last_change_time = now;

            ESP_LOGI(TAG, "Button %s", g_is_pressed ? "PRESSED" : "RELEASED");

            /* Call callback (safe for task context) */
            if (g_callback) {
                g_callback(g_is_pressed);
            }
        }
    }
}

int hal_button_init(button_callback_t callback)
{
    ESP_LOGI(TAG, "Initializing button via I2C IO Expander (I2C_NUM_0)...");

    g_callback = callback;
    g_is_pressed = false;
    g_last_change_time = 0;

    /* Initialize I2C if not already done */
    if (!g_i2c_initialized) {
        i2c_config_t i2c_conf = {
            .mode = I2C_MODE_MASTER,
            .sda_io_num = IO_EXPANDER_SDA,
            .scl_io_num = IO_EXPANDER_SCL,
            .sda_pullup_en = GPIO_PULLUP_ENABLE,
            .scl_pullup_en = GPIO_PULLUP_ENABLE,
            .master.clk_speed = 400000,
        };

        esp_err_t ret = i2c_param_config(IO_EXPANDER_I2C_NUM, &i2c_conf);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "I2C config failed: %s", esp_err_to_name(ret));
            return -1;
        }

        ret = i2c_driver_install(IO_EXPANDER_I2C_NUM, I2C_MODE_MASTER, 0, 0, 0);
        if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
            ESP_LOGE(TAG, "I2C driver install failed: %s", esp_err_to_name(ret));
            return -1;
        }

        g_i2c_initialized = true;
        ESP_LOGI(TAG, "I2C initialized (SDA=%d, SCL=%d)", IO_EXPANDER_SDA, IO_EXPANDER_SCL);
    }

    /* Read initial state */
    if (read_button_state(&g_is_pressed) == 0) {
        ESP_LOGI(TAG, "Button initialized, initial state: %s",
                 g_is_pressed ? "pressed" : "released");
    } else {
        ESP_LOGW(TAG, "Button initialized but read failed");
    }

    return 0;
}

bool hal_button_is_pressed(void)
{
    return g_is_pressed;
}

void hal_button_deinit(void)
{
    g_callback = NULL;
    /* Don't uninstall I2C - may be used by other components */
}
