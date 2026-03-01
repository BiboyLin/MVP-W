#include "hal_audio.h"
#include "sensecap-watcher.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "HAL_AUDIO"

/* Default sample rates */
#define SAMPLE_RATE_RECORD  16000   /* ASR expects 16kHz */
#define SAMPLE_RATE_PLAY    24000   /* 火山引擎 TTS uses 24kHz */

static bool codec_initialized = false;  /* codec init is global, only once */
static bool is_running = false;         /* current running state */
static uint32_t current_sample_rate = SAMPLE_RATE_RECORD;  /* current sample rate */
static esp_codec_dev_handle_t mic_handle = NULL;
static esp_codec_dev_handle_t speaker_handle = NULL;

/* Initialize codec once at system startup */
int hal_audio_init(void)
{
    if (codec_initialized) {
        return 0;
    }

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

    codec_initialized = true;
    is_running = true;  /* Keep codec running always */

    /* Set default sample rate for recording (16kHz) */
    bsp_codec_set_fs(SAMPLE_RATE_RECORD, 16, 1);
    current_sample_rate = SAMPLE_RATE_RECORD;

    /* Set volume and unmute (required for speaker output) */
    bsp_codec_mute_set(false);
    bsp_codec_volume_set(100, NULL);

    ESP_LOGI(TAG, "Audio codec initialized (16kHz for recording, volume=100)");
    return 0;
}

/* Set sample rate for playback (call before TTS playback) */
void hal_audio_set_sample_rate(uint32_t sample_rate)
{
    if (!codec_initialized) {
        return;
    }

    if (current_sample_rate == sample_rate) {
        return;  /* No change needed */
    }

    ESP_LOGI(TAG, "Switching sample rate: %lu -> %lu", current_sample_rate, sample_rate);
    bsp_codec_set_fs(sample_rate, 16, 1);
    current_sample_rate = sample_rate;
}

int hal_audio_start(void)
{
    if (is_running) {
        return 0;
    }

    /* Ensure codec is initialized */
    if (!codec_initialized) {
        if (hal_audio_init() != 0) {
            return -1;
        }
    }

    is_running = true;
    ESP_LOGI(TAG, "Audio started");
    return 0;
}

int hal_audio_read(uint8_t *out_buf, int max_len)
{
    if (!is_running || !mic_handle) {
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
    if (!is_running || !speaker_handle) {
        ESP_LOGW(TAG, "Write blocked: is_running=%d, speaker_handle=%p", is_running, speaker_handle);
        return -1;
    }

    size_t bytes_written = 0;
    ESP_LOGI(TAG, "Writing %d bytes to speaker...", len);
    esp_err_t ret = bsp_i2s_write((void *)data, len, &bytes_written, 100);
    ESP_LOGI(TAG, "Write result: ret=%d, written=%d", ret, (int)bytes_written);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Write error: %s", esp_err_to_name(ret));
        return -1;
    }

    return (int)bytes_written;
}

int hal_audio_stop(void)
{
    if (!is_running) {
        return 0;
    }

    /* Just mark as stopped, don't actually stop the codec */
    /* This avoids I2S reconfiguration issues */
    is_running = false;
    ESP_LOGI(TAG, "Audio stopped (codec stays running)");
    return 0;
}
