/**
 * @file hal_display.c
 * @brief Display HAL implementation with SPIFFS-based emoji animation
 */

#include "hal_display.h"
#include "display_ui.h"
#include "emoji_png.h"
#include "emoji_anim.h"
#include "sensecap-watcher.h"
#include "esp_log.h"
#include "lvgl.h"

#if LV_USE_PNG
#include "lvgl/src/extra/libs/png/lv_png.h"
#endif

/* External CJK font for Chinese character support */
#if LV_FONT_SIMSUN_16_CJK
extern lv_font_t lv_font_simsun_16_cjk;
#endif

#define TAG "HAL_DISPLAY"

static lv_obj_t *label_text = NULL;
static lv_obj_t *img_emoji = NULL;
static bool is_initialized = false;

/* Map display_ui emoji_type to emoji_png emoji_anim_type */
static emoji_anim_type_t map_emoji_type(int ui_emoji_id)
{
    /* display_ui.h: 0=standby, 1=happy, 2=sad, 3=surprised, 4=angry,
     *              5=listening, 6=analyzing, 7=speaking */
    /* emoji_png.h: 0=greeting, 1=detecting, 2=detected, 3=speaking,
     *              4=listening, 5=analyzing, 6=standby */
    /* NOTE: speaking PNGs are 240x240 (wrong size), temporarily use listening */
    switch (ui_emoji_id) {
        case 0:  /* EMOJI_STANDBY */   return EMOJI_ANIM_STANDBY;    /* standby */
        case 1:  /* EMOJI_HAPPY */     return EMOJI_ANIM_GREETING;   /* greeting */
        case 2:  /* EMOJI_SAD */       return EMOJI_ANIM_DETECTED;   /* detected */
        case 3:  /* EMOJI_SURPRISED */ return EMOJI_ANIM_DETECTING;  /* detecting */
        case 4:  /* EMOJI_ANGRY */     return EMOJI_ANIM_ANALYZING;  /* analyzing */
        case 5:  /* EMOJI_LISTENING */ return EMOJI_ANIM_LISTENING;  /* listening */
        case 6:  /* EMOJI_ANALYZING */ return EMOJI_ANIM_ANALYZING;  /* analyzing */
        case 7:  /* EMOJI_SPEAKING */  return EMOJI_ANIM_LISTENING;  /* listening (temp) */
        default: return EMOJI_ANIM_STANDBY;
    }
}

int hal_display_init(void)
{
    if (is_initialized) {
        return 0;
    }

    ESP_LOGI(TAG, "Initializing display with LVGL...");

    /* 1. Initialize IO expander first (powers on LCD via BSP_PWR_LCD) */
    if (bsp_io_expander_init() == NULL) {
        ESP_LOGE(TAG, "Failed to initialize IO expander");
        return -1;
    }
    ESP_LOGI(TAG, "IO expander initialized, LCD power ON");

    /* 2. Initialize LVGL via SDK */
    lv_disp_t *disp = bsp_lvgl_init();
    if (disp == NULL) {
        ESP_LOGE(TAG, "Failed to initialize LVGL");
        return -1;
    }
    ESP_LOGI(TAG, "LVGL initialized");

    /* 3. Set backlight brightness (default is 0% after init) */
    esp_err_t ret = bsp_lcd_brightness_set(50);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to set brightness: %d", ret);
    } else {
        ESP_LOGI(TAG, "Backlight set to 50%%");
    }

    /* 4. Initialize LVGL PNG decoder */
#if LV_USE_PNG
    lv_png_init();
    ESP_LOGI(TAG, "PNG decoder initialized");
#endif

    /* 5. Initialize SPIFFS and load emoji images */
    if (emoji_spiffs_init() != 0) {
        ESP_LOGW(TAG, "Failed to initialize SPIFFS, emoji animations disabled");
    } else {
        if (emoji_load_all_images() != 0) {
            ESP_LOGW(TAG, "Failed to load emoji images");
        } else {
            ESP_LOGI(TAG, "Emoji images loaded successfully");
        }
    }

    /* 5. Get current active screen */
    lv_obj_t *scr = lv_disp_get_scr_act(NULL);

    /* Set screen background to dark color */
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    /* Disable scrolling/dragging on screen */
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);

    /* 6. Create emoji image FIRST (centered) - so it's in background */
    img_emoji = lv_img_create(scr);
    lv_obj_align(img_emoji, LV_ALIGN_CENTER, 0, 40);

    /* 7. Create text label AFTER emoji - so it's in foreground */
    label_text = lv_label_create(scr);
    lv_obj_set_width(label_text, 380);
    lv_label_set_long_mode(label_text, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(label_text, LV_TEXT_ALIGN_CENTER, 0);  /* Center align text */
    lv_label_set_text(label_text, "Ready");
    lv_obj_set_style_text_color(label_text, lv_color_white(), 0);
    lv_obj_align(label_text, LV_ALIGN_CENTER, 0, -140);  /* Move higher to avoid emoji overlap */

    /* Set CJK font for Chinese character support */
#if LV_FONT_SIMSUN_16_CJK
    lv_obj_set_style_text_font(label_text, &lv_font_simsun_16_cjk, 0);
    ESP_LOGI(TAG, "Using SimSun 16 CJK font for Chinese support");
#else
    ESP_LOGW(TAG, "CJK font not enabled, Chinese characters may not display");
#endif

    /* 8. Initialize animation system */
    if (emoji_anim_init(img_emoji) == 0) {
        /* Start with greeting animation */
        emoji_anim_start(EMOJI_ANIM_GREETING);
    }

    is_initialized = true;
    ESP_LOGI(TAG, "Display initialized with LVGL and emoji animations");
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

    /* Truncate text to max 30 chars to fit display boundary */
    #define MAX_DISPLAY_CHARS 30
    char truncated[MAX_DISPLAY_CHARS + 4];  /* +4 for "..." and null */
    int len = strlen(text);

    if (len > MAX_DISPLAY_CHARS) {
        strncpy(truncated, text, MAX_DISPLAY_CHARS);
        strcpy(truncated + MAX_DISPLAY_CHARS, "...");
        lv_label_set_text(label_text, truncated);
        ESP_LOGI(TAG, "Set text (truncated): '%s' -> '%s'", text, truncated);
    } else {
        lv_label_set_text(label_text, text);
        ESP_LOGI(TAG, "Set text: '%s' (size %d)", text, font_size);
    }

    return 0;
}

int hal_display_set_emoji(int emoji_id)
{
    if (!is_initialized || !img_emoji) {
        ESP_LOGW(TAG, "Display not initialized");
        return -1;
    }

    /* Map UI emoji type to animation type */
    emoji_anim_type_t type = map_emoji_type(emoji_id);

    /* Start animation */
    int ret = emoji_anim_start(type);
    if (ret != 0) {
        ESP_LOGW(TAG, "Failed to start animation for emoji ID: %d", emoji_id);
        return -1;
    }

    const char *emoji_name = "unknown";
    switch (emoji_id) {
        case 0:  emoji_name = "standby"; break;
        case 1:  emoji_name = "happy"; break;
        case 2:  emoji_name = "sad"; break;
        case 3:  emoji_name = "surprised"; break;
        case 4:  emoji_name = "angry"; break;
        case 5:  emoji_name = "listening"; break;
        case 6:  emoji_name = "analyzing"; break;
        case 7:  emoji_name = "speaking"; break;
    }

    ESP_LOGI(TAG, "Set emoji: %s -> %s animation", emoji_name, emoji_type_name(type));
    return 0;
}

/**
 * @brief Start speaking animation (for voice interaction)
 */
int hal_display_start_speaking(void)
{
    if (!is_initialized) {
        return -1;
    }
    return emoji_anim_start(EMOJI_ANIM_SPEAKING);
}

/**
 * @brief Start listening animation (for voice input)
 */
int hal_display_start_listening(void)
{
    if (!is_initialized) {
        return -1;
    }
    return emoji_anim_start(EMOJI_ANIM_LISTENING);
}

/**
 * @brief Start analyzing animation (for AI processing)
 */
int hal_display_start_analyzing(void)
{
    if (!is_initialized) {
        return -1;
    }
    return emoji_anim_start(EMOJI_ANIM_ANALYZING);
}

/**
 * @brief Stop animation and show standby
 */
int hal_display_stop_animation(void)
{
    if (!is_initialized) {
        return -1;
    }
    return emoji_anim_start(EMOJI_ANIM_STANDBY);
}
