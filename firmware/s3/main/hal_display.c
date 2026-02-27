#include "hal_display.h"
#include "esp_log.h"

#define TAG "HAL_DISPLAY"

/* TODO: Integrate with LVGL and sensecap-watcher SDK */
/* This is a stub implementation for MVP */

int hal_display_init(void)
{
    ESP_LOGI(TAG, "Display initialized (stub)");
    return 0;
}

int hal_display_set_text(const char *text, int font_size)
{
    ESP_LOGI(TAG, "Set text: '%s' (size %d)", text, font_size);
    /* TODO: Call LVGL to update label */
    return 0;
}

int hal_display_set_emoji(int emoji_id)
{
    const char *emoji_names[] = {
        "normal", "happy", "sad", "surprised", "angry"
    };

    if (emoji_id >= 0 && emoji_id < 5) {
        ESP_LOGI(TAG, "Set emoji: %s", emoji_names[emoji_id]);
    } else {
        ESP_LOGW(TAG, "Unknown emoji ID: %d", emoji_id);
    }

    /* TODO: Call LVGL to update image */
    return 0;
}
