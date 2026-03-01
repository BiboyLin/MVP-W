#include "hal_audio.h"
#include "sensecap-watcher.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "HAL_AUDIO"

static bool is_initialized = false;
static esp_codec_dev_handle_t mic_handle = NULL;
static esp_codec_dev_handle_t speaker_handle = NULL;

int hal_audio_start(void)
{
    if (is_initialized) {
        return 0;
    }

    /* Initialize codec using SDK */
    ESP_LOGI(TAG, "Initializing audio codec via SDK...");

    esp_err_t ret = bsp_codec_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init codec: %s", esp_err_to_name(ret));
        return -1;
    }

    /* Get microphone and speaker handles */
    mic_handle = bsp_codec_microphone_get();
    speaker_handle = bsp_codec_speaker_get();

    if (!mic_handle) {
        ESP_LOGE(TAG, "Failed to get microphone handle");
        return -1;
    }

    if (!speaker_handle) {
        ESP_LOGE(TAG, "Failed to get speaker handle");
        return -1;
    }

    /* Resume codec device */
    ret = bsp_codec_dev_resume();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to resume codec: %s", esp_err_to_name(ret));
        return -1;
    }

    is_initialized = true;
    ESP_LOGI(TAG, "Audio initialized via SDK");
    return 0;
}

int hal_audio_read(uint8_t *out_buf, int max_len)
{
    if (!is_initialized || !mic_handle) {
        return -1;
    }

    size_t bytes_read = 0;
    esp_err_t ret = bsp_i2s_read(out_buf, max_len, &bytes_read, 100);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Read error: %s", esp_err_to_name(ret));
        return -1;
    }

    return (int)bytes_read;
}

int hal_audio_write(const uint8_t *data, int len)
{
    if (!is_initialized || !speaker_handle) {
        return -1;
    }

    size_t bytes_written = 0;
    esp_err_t ret = bsp_i2s_write((void *)data, len, &bytes_written, 100);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Write error: %s", esp_err_to_name(ret));
        return -1;
    }

    return (int)bytes_written;
}

int hal_audio_stop(void)
{
    if (!is_initialized) {
        return 0;
    }

    esp_err_t ret = bsp_codec_dev_stop();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Stop error: %s", esp_err_to_name(ret));
    }

    is_initialized = false;
    ESP_LOGI(TAG, "Audio stopped");
    return 0;
}
