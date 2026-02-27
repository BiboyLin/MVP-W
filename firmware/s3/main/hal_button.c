/**
 * HAL Button Driver for MVP-W
 *
 * Uses esp_io_expander component for proper PCA9535 support
 */
#include "hal_button.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_io_expander.h"
#include "esp_io_expander_pca95xx_16bit.h"

#define TAG "HAL_BUTTON"

/* IO Expander I2C config (from sensecap-watcher.h) */
#define IO_EXPANDER_I2C_NUM     I2C_NUM_0
#define IO_EXPANDER_I2C_ADDR    ESP_IO_EXPANDER_I2C_PCA9535_ADDRESS_001  /* 0x24 */
#define IO_EXPANDER_SDA         47
#define IO_EXPANDER_SCL         48
#define IO_EXPANDER_INT         2

/* Button is on IO Expander pin 3 */
#define BUTTON_PIN_NUM          3

/* Input mask (from sensecap-watcher.h DRV_IO_EXP_INPUT_MASK) */
#define IO_EXP_INPUT_MASK       0x20ff  /* P0.0 ~ P0.7 | P1.3 */

/* Debounce time in ms */
#define DEBOUNCE_MS             50

static button_callback_t g_callback = NULL;
static bool g_is_pressed = false;
static int64_t g_last_change_time = 0;
static esp_io_expander_handle_t g_io_exp_handle = NULL;

/* Poll button state (called from task context) */
void hal_button_poll(void)
{
    if (g_io_exp_handle == NULL) {
        return;
    }

    /* Read button state */
    uint32_t pin_val = 0;
    esp_err_t ret = esp_io_expander_get_level(g_io_exp_handle, (1 << BUTTON_PIN_NUM), &pin_val);
    if (ret != ESP_OK) {
        return;
    }

    /* Button is active low (0 = pressed) */
    bool current_pressed = (pin_val == 0);

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
    ESP_LOGI(TAG, "Initializing button via IO Expander...");

    g_callback = callback;
    g_is_pressed = false;
    g_last_change_time = 0;

    /* Initialize I2C master */
    i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = IO_EXPANDER_SDA,
        .scl_io_num = IO_EXPANDER_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000,
    };

    esp_err_t ret = i2c_param_config(IO_EXPANDER_I2C_NUM, &i2c_conf);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "I2C config failed: %s", esp_err_to_name(ret));
        return -1;
    }

    ret = i2c_driver_install(IO_EXPANDER_I2C_NUM, I2C_MODE_MASTER, 0, 0, 0);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "I2C driver install failed: %s", esp_err_to_name(ret));
        return -1;
    }

    ESP_LOGI(TAG, "I2C initialized (SDA=%d, SCL=%d)", IO_EXPANDER_SDA, IO_EXPANDER_SCL);

    /* Create IO expander handle */
    ret = esp_io_expander_new_i2c_pca95xx_16bit(IO_EXPANDER_I2C_NUM, IO_EXPANDER_I2C_ADDR, &g_io_exp_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "IO expander create failed: %s", esp_err_to_name(ret));
        return -1;
    }

    /* Set button pin as input */
    ret = esp_io_expander_set_dir(g_io_exp_handle, (1 << BUTTON_PIN_NUM), IO_EXPANDER_INPUT);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Set dir failed: %s", esp_err_to_name(ret));
    }

    /* Read initial state */
    uint32_t pin_val = 0;
    ret = esp_io_expander_get_level(g_io_exp_handle, (1 << BUTTON_PIN_NUM), &pin_val);
    if (ret == ESP_OK) {
        g_is_pressed = (pin_val == 0);
        ESP_LOGI(TAG, "Button initialized, initial state: %s",
                 g_is_pressed ? "pressed" : "released");
    } else {
        ESP_LOGW(TAG, "Button initialized but initial read failed");
    }

    return 0;
}

bool hal_button_is_pressed(void)
{
    return g_is_pressed;
}

void hal_button_deinit(void)
{
    if (g_io_exp_handle) {
        esp_io_expander_del(g_io_exp_handle);
        g_io_exp_handle = NULL;
    }
    g_callback = NULL;
}
