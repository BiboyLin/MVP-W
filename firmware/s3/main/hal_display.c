#include "hal_display.h"
#include "sensecap-watcher.h"
#include "esp_log.h"
#include "lvgl.h"

#define TAG "HAL_DISPLAY"

static lv_obj_t *label_text = NULL;
static lv_obj_t *label_emoji = NULL;
static bool is_initialized = false;

int hal_display_init(void)
{
    if (is_initialized) {
        return 0;
    }

    ESP_LOGI(TAG, "Initializing display with LVGL...");

    /* Initialize LVGL via SDK */
    lv_disp_t *disp = bsp_lvgl_init();
    if (disp == NULL) {
        ESP_LOGE(TAG, "Failed to initialize LVGL");
        return -1;
    }

    /* Get current active screen */
    lv_obj_t *scr = lv_disp_get_scr_act(NULL);

    /* Create text label */
    label_text = lv_label_create(scr);
    lv_obj_set_width(label_text, 380);
    lv_label_set_long_mode(label_text, LV_LABEL_LONG_WRAP);
    lv_obj_align(label_text, LV_ALIGN_CENTER, 0, -20);

    /* Create emoji label (placeholder for now) */
    label_emoji = lv_label_create(scr);
    lv_obj_align(label_emoji, LV_ALIGN_CENTER, 0, 40);
    lv_label_set_text(label_emoji, "");

    is_initialized = true;
    ESP_LOGI(TAG, "Display initialized with LVGL");
    return 0;
}

int hal_display_set_text(const char *text, int font_size)
{
    if (!is_initialized || !label_text) {
        ESP_LOGW(TAG, "Display not initialized");
        return -1;
    }

    if (!text) {
        return -1;
    }

    /* Update label text */
    lv_label_set_text(label_text, text);

    /* Note: font_size is ignored for now, uses default font */
    ESP_LOGI(TAG, "Set text: '%s' (size %d)", text, font_size);
    return 0;
}

int hal_display_set_emoji(int emoji_id)
{
    if (!is_initialized || !label_emoji) {
        ESP_LOGW(TAG, "Display not initialized");
        return -1;
    }

    /* Map emoji ID to display text (MVP: using text for now) */
    const char *emoji_text = "";
    const char *emoji_name = "unknown";

    switch (emoji_id) {
        case 0:  /* EMOJI_NORMAL */
            emoji_text = "😐";
            emoji_name = "normal";
            break;
        case 1:  /* EMOJI_HAPPY */
            emoji_text = "😊";
            emoji_name = "happy";
            break;
        case 2:  /* EMOJI_SAD */
            emoji_text = "😢";
            emoji_name = "sad";
            break;
        case 3:  /* EMOJI_SURPRISED */
            emoji_text = "😮";
            emoji_name = "surprised";
            break;
        case 4:  /* EMOJI_ANGRY */
            emoji_text = "😠";
            emoji_name = "angry";
            break;
        default:
            emoji_text = "❓";
            break;
    }

    lv_label_set_text(label_emoji, emoji_text);
    ESP_LOGI(TAG, "Set emoji: %s", emoji_name);
    return 0;
}
