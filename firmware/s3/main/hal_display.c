/**
 * HAL Display Driver for MVP-W
 * Using esp_lcd_spd2010 component from remote registry
 */
#include "hal_display.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/ledc.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_spd2010.h"
#include "lvgl.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#define TAG "HAL_DISPLAY"

/* Display configuration */
#define LCD_H_RES          412
#define LCD_V_RES          412
#define LCD_BITS_PER_PIXEL 16

/* QSPI GPIO (from sensecap-watcher.h) */
#define LCD_SPI_NUM        SPI3_HOST
#define LCD_SPI_CS         45
#define LCD_GPIO_BL        8
#define LCD_SPI_PCLK       7
#define LCD_SPI_DATA0      9
#define LCD_SPI_DATA1      1
#define LCD_SPI_DATA2      14
#define LCD_SPI_DATA3      13

/* Backlight PWM */
#define LCD_LEDC_TIMER     LEDC_TIMER_1
#define LCD_LEDC_MODE      LEDC_LOW_SPEED_MODE
#define LCD_LEDC_CHANNEL   LEDC_CHANNEL_1
#define LCD_LEDC_DUTY_RES  LEDC_TIMER_10_BIT

/* LVGL buffer */
#define LVGL_BUF_HEIGHT    50

static esp_lcd_panel_io_handle_t panel_io = NULL;
static esp_lcd_panel_handle_t panel = NULL;
static lv_disp_draw_buf_t disp_buf;
static lv_color_t *lvgl_buf = NULL;
static lv_disp_drv_t disp_drv;
static lv_obj_t *label_text = NULL;
static lv_obj_t *label_emoji = NULL;
static bool is_initialized = false;
static SemaphoreHandle_t lvgl_mutex = NULL;

static void disp_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p);
static bool notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx);
static void lvgl_tick_task(void *arg);

/* Initialize backlight PWM */
static esp_err_t backlight_init(void)
{
    ESP_LOGI(TAG, "Initializing backlight PWM...");

    ledc_timer_config_t timer_cfg = {
        .speed_mode = LCD_LEDC_MODE,
        .duty_resolution = LCD_LEDC_DUTY_RES,
        .timer_num = LCD_LEDC_TIMER,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_RETURN_ON_ERROR(ledc_timer_config(&timer_cfg), TAG, "LEDC timer config failed");

    ledc_channel_config_t ch_cfg = {
        .speed_mode = LCD_LEDC_MODE,
        .channel = LCD_LEDC_CHANNEL,
        .timer_sel = LCD_LEDC_TIMER,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = LCD_GPIO_BL,
        .duty = 0,
        .hpoint = 0,
    };
    ESP_RETURN_ON_ERROR(ledc_channel_config(&ch_cfg), TAG, "LEDC channel config failed");

    ESP_LOGI(TAG, "Backlight PWM initialized");
    return ESP_OK;
}

static esp_err_t backlight_set(int percent)
{
    if (percent > 100) percent = 100;
    if (percent < 0) percent = 0;

    uint32_t duty = (BIT(LCD_LEDC_DUTY_RES) * percent) / 100;
    esp_err_t ret = ledc_set_duty(LCD_LEDC_MODE, LCD_LEDC_CHANNEL, duty);
    if (ret != ESP_OK) return ret;
    return ledc_update_duty(LCD_LEDC_MODE, LCD_LEDC_CHANNEL);
}

/* LVGL flush callback */
static void disp_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t) drv->user_data;
    const int offsetx1 = area->x1;
    const int offsetx2 = area->x2;
    const int offsety1 = area->y1;
    const int offsety2 = area->y2;

    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, color_p);
}

/* Notify LVGL flush ready callback */
static bool notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    lv_disp_drv_t *disp_driver = (lv_disp_drv_t *)user_ctx;
    lv_disp_flush_ready(disp_driver);
    return false;
}

/* LVGL tick task */
static void lvgl_tick_task(void *arg)
{
    (void)arg;
    while (1) {
        if (xSemaphoreTake(lvgl_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            lv_tick_inc(5);
            lv_timer_handler();
            xSemaphoreGive(lvgl_mutex);
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

int hal_display_init(void)
{
    if (is_initialized) {
        return 0;
    }

    ESP_LOGI(TAG, "Initializing display...");

    /* Step 1: Initialize backlight (set to 0 first) */
    if (backlight_init() != ESP_OK) {
        ESP_LOGE(TAG, "Backlight init failed");
        return -1;
    }

    /* Step 2: Initialize SPI bus using SPD2010 macro */
    ESP_LOGI(TAG, "Initializing QSPI bus...");
    const spi_bus_config_t buscfg = SPD2010_PANEL_BUS_QSPI_CONFIG(
        LCD_SPI_PCLK,
        LCD_SPI_DATA0,
        LCD_SPI_DATA1,
        LCD_SPI_DATA2,
        LCD_SPI_DATA3,
        LCD_H_RES * LCD_V_RES * LCD_BITS_PER_PIXEL / 8
    );
    esp_err_t ret = spi_bus_initialize(LCD_SPI_NUM, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI bus init failed: %s", esp_err_to_name(ret));
        return -1;
    }

    /* Step 3: Install panel IO using SPD2010 macro */
    ESP_LOGI(TAG, "Installing panel IO...");
    const esp_lcd_panel_io_spi_config_t io_config = SPD2010_PANEL_IO_QSPI_CONFIG(
        LCD_SPI_CS,
        notify_lvgl_flush_ready,
        &disp_drv
    );
    ret = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_SPI_NUM, &io_config, &panel_io);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Panel IO failed: %s", esp_err_to_name(ret));
        return -1;
    }

    /* Step 4: Create SPD2010 panel */
    ESP_LOGI(TAG, "Creating SPD2010 panel...");
    spd2010_vendor_config_t vendor_config = {
        .flags = {
            .use_qspi_interface = 1,
        },
    };
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = -1,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = LCD_BITS_PER_PIXEL,
        .vendor_config = &vendor_config,
    };
    ret = esp_lcd_new_panel_spd2010(panel_io, &panel_config, &panel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Panel create failed: %s", esp_err_to_name(ret));
        return -1;
    }

    /* Step 5: Reset and init panel */
    ESP_LOGI(TAG, "Resetting panel...");
    ret = esp_lcd_panel_reset(panel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Panel reset failed: %s", esp_err_to_name(ret));
        return -1;
    }

    ESP_LOGI(TAG, "Initializing panel...");
    ret = esp_lcd_panel_init(panel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Panel init failed: %s", esp_err_to_name(ret));
        return -1;
    }

    /* Step 6: Turn on display */
    ESP_LOGI(TAG, "Turning on display...");
    ret = esp_lcd_panel_disp_on_off(panel, true);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Display on failed: %s", esp_err_to_name(ret));
    }

    /* Step 7: Create LVGL mutex */
    lvgl_mutex = xSemaphoreCreateMutex();
    if (!lvgl_mutex) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return -1;
    }

    xSemaphoreTake(lvgl_mutex, portMAX_DELAY);

    /* Step 8: Initialize LVGL */
    ESP_LOGI(TAG, "Initializing LVGL...");
    lv_init();

    /* Step 9: Allocate buffer in PSRAM */
    lvgl_buf = heap_caps_malloc(LCD_H_RES * LVGL_BUF_HEIGHT * sizeof(lv_color_t),
                                 MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!lvgl_buf) {
        ESP_LOGW(TAG, "PSRAM alloc failed, using internal RAM");
        lvgl_buf = malloc(LCD_H_RES * LVGL_BUF_HEIGHT * sizeof(lv_color_t));
    }
    if (!lvgl_buf) {
        ESP_LOGE(TAG, "Failed to allocate LVGL buffer");
        xSemaphoreGive(lvgl_mutex);
        return -1;
    }
    ESP_LOGI(TAG, "LVGL buffer allocated: %d bytes", LCD_H_RES * LVGL_BUF_HEIGHT * sizeof(lv_color_t));

    lv_disp_draw_buf_init(&disp_buf, lvgl_buf, NULL, LCD_H_RES * LVGL_BUF_HEIGHT);

    /* Step 10: Initialize display driver */
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = LCD_H_RES;
    disp_drv.ver_res = LCD_V_RES;
    disp_drv.flush_cb = disp_flush_cb;
    disp_drv.draw_buf = &disp_buf;
    disp_drv.user_data = panel;
    lv_disp_drv_register(&disp_drv);

    /* Step 11: Create UI elements */
    lv_obj_t *scr = lv_disp_get_scr_act(NULL);

    label_text = lv_label_create(scr);
    lv_obj_set_width(label_text, 380);
    lv_label_set_long_mode(label_text, LV_LABEL_LONG_WRAP);
    lv_obj_align(label_text, LV_ALIGN_CENTER, 0, -50);
    lv_label_set_text(label_text, "Hello MVP-W!");

    label_emoji = lv_label_create(scr);
    lv_obj_align(label_emoji, LV_ALIGN_CENTER, 0, 50);
    lv_label_set_text(label_emoji, "^_^");

    xSemaphoreGive(lvgl_mutex);

    /* Step 12: Start LVGL task */
    xTaskCreate(lvgl_tick_task, "lvgl", 4096, NULL, 5, NULL);

    /* Step 13: Turn on backlight */
    ESP_LOGI(TAG, "Setting backlight to 80%%...");
    backlight_set(80);

    is_initialized = true;
    ESP_LOGI(TAG, "Display initialized successfully!");
    return 0;
}

int hal_display_set_text(const char *text, int font_size)
{
    if (!is_initialized || !label_text || !text) {
        return -1;
    }

    if (xSemaphoreTake(lvgl_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        lv_label_set_text(label_text, text);
        xSemaphoreGive(lvgl_mutex);
    }

    ESP_LOGI(TAG, "Set text: '%s'", text);
    (void)font_size;
    return 0;
}

int hal_display_set_emoji(int emoji_id)
{
    if (!is_initialized || !label_emoji) {
        return -1;
    }

    const char *emoji_text = "";
    const char *emoji_name = "unknown";

    switch (emoji_id) {
        case 0: emoji_text = "-"; emoji_name = "normal"; break;
        case 1: emoji_text = "^_^"; emoji_name = "happy"; break;
        case 2: emoji_text = "T_T"; emoji_name = "sad"; break;
        case 3: emoji_text = "O_O"; emoji_name = "surprised"; break;
        case 4: emoji_text = ">_<"; emoji_name = "angry"; break;
        case 5: emoji_text = "?_?"; emoji_name = "thinking"; break;
        default: emoji_text = "?"; break;
    }

    if (xSemaphoreTake(lvgl_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        lv_label_set_text(label_emoji, emoji_text);
        xSemaphoreGive(lvgl_mutex);
    }

    ESP_LOGI(TAG, "Set emoji: %s", emoji_name);
    return 0;
}
