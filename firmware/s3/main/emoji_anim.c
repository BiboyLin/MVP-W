/**
 * @file emoji_anim.c
 * @brief Emoji animation timer system implementation
 */

#include "emoji_anim.h"
#include "esp_log.h"

#define TAG "EMOJI_ANIM"

/* Animation state */
static lv_obj_t *g_img_obj = NULL;
static lv_timer_t *g_timer = NULL;
static emoji_anim_type_t g_current_type = EMOJI_ANIM_NONE;
static int g_current_frame = 0;
static uint32_t g_interval_ms = EMOJI_ANIM_INTERVAL_MS;

static void emoji_timer_callback(lv_timer_t *timer)
{
    (void)timer;

    if (g_current_type == EMOJI_ANIM_NONE || g_img_obj == NULL) {
        return;
    }

    int frame_count = emoji_get_frame_count(g_current_type);
    if (frame_count <= 0) {
        ESP_LOGW(TAG, "No frames for type %d", g_current_type);
        emoji_anim_stop();
        return;
    }

    /* Advance to next frame */
    g_current_frame = (g_current_frame + 1) % frame_count;

    /* Get image descriptor */
    lv_img_dsc_t *img = emoji_get_image(g_current_type, g_current_frame);
    if (img != NULL) {
        lv_img_set_src(g_img_obj, img);
    }
}

int emoji_anim_init(lv_obj_t *img_obj)
{
    if (img_obj == NULL) {
        ESP_LOGE(TAG, "Invalid image object");
        return -1;
    }

    g_img_obj = img_obj;
    g_current_type = EMOJI_ANIM_NONE;
    g_current_frame = 0;

    ESP_LOGI(TAG, "Animation system initialized");
    return 0;
}

int emoji_anim_start(emoji_anim_type_t type)
{
    if (g_img_obj == NULL) {
        ESP_LOGE(TAG, "Animation not initialized");
        return -1;
    }

    if (type < 0 || type >= EMOJI_ANIM_COUNT) {
        ESP_LOGE(TAG, "Invalid emoji type: %d", type);
        return -1;
    }

    int frame_count = emoji_get_frame_count(type);
    if (frame_count <= 0) {
        ESP_LOGW(TAG, "No frames available for type: %s", emoji_type_name(type));
        return -1;
    }

    /* Stop existing animation */
    emoji_anim_stop();

    /* Set new type */
    g_current_type = type;
    g_current_frame = 0;

    /* Show first frame immediately */
    lv_img_dsc_t *img = emoji_get_image(type, 0);
    if (img != NULL) {
        lv_img_set_src(g_img_obj, img);
    }

    /* Start timer if more than one frame */
    if (frame_count > 1) {
        g_timer = lv_timer_create(emoji_timer_callback, g_interval_ms, NULL);
        if (g_timer == NULL) {
            ESP_LOGE(TAG, "Failed to create timer");
            return -1;
        }
    }

    ESP_LOGI(TAG, "Started animation: %s (%d frames)", emoji_type_name(type), frame_count);
    return 0;
}

void emoji_anim_stop(void)
{
    if (g_timer != NULL) {
        lv_timer_del(g_timer);
        g_timer = NULL;
    }
    g_current_type = EMOJI_ANIM_NONE;
    g_current_frame = 0;
}

bool emoji_anim_is_running(void)
{
    return g_timer != NULL && g_current_type != EMOJI_ANIM_NONE;
}

emoji_anim_type_t emoji_anim_get_type(void)
{
    return g_current_type;
}

void emoji_anim_set_interval(uint32_t interval_ms)
{
    g_interval_ms = interval_ms;
    if (g_timer != NULL) {
        lv_timer_set_period(g_timer, interval_ms);
    }
}

int emoji_anim_show_static(emoji_anim_type_t type, int frame)
{
    if (g_img_obj == NULL) {
        ESP_LOGE(TAG, "Animation not initialized");
        return -1;
    }

    /* Stop any running animation */
    emoji_anim_stop();

    if (type < 0 || type >= EMOJI_ANIM_COUNT) {
        ESP_LOGE(TAG, "Invalid emoji type: %d", type);
        return -1;
    }

    lv_img_dsc_t *img = emoji_get_image(type, frame);
    if (img == NULL) {
        /* Try first frame as fallback */
        img = emoji_get_image(type, 0);
        if (img == NULL) {
            ESP_LOGE(TAG, "No image available for type: %s", emoji_type_name(type));
            return -1;
        }
    }

    lv_img_set_src(g_img_obj, img);
    g_current_type = type;
    g_current_frame = frame;

    ESP_LOGI(TAG, "Showing static: %s frame %d", emoji_type_name(type), frame);
    return 0;
}
