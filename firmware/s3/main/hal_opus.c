#include "hal_opus.h"
#include "esp_log.h"

#define TAG "HAL_OPUS"

/* TODO: Integrate with Opus codec library */
/* This is a stub implementation for MVP */

int hal_opus_init(void)
{
    ESP_LOGI(TAG, "Opus initialized (stub)");
    return 0;
}

int hal_opus_encode(const uint8_t *pcm_in, int pcm_len,
                    uint8_t *opus_out, int opus_max_len)
{
    if (!pcm_in || !opus_out || pcm_len <= 0 || opus_max_len <= 0) {
        return -1;
    }

    /* TODO: Implement actual Opus encoding */
    /* For now, just copy raw PCM data (not production-ready) */
    int out_len = (pcm_len < opus_max_len) ? pcm_len : opus_max_len;
    for (int i = 0; i < out_len; i++) {
        opus_out[i] = pcm_in[i];
    }

    ESP_LOGD(TAG, "Encode: %d -> %d bytes (stub)", pcm_len, out_len);
    return out_len;
}

int hal_opus_decode(const uint8_t *opus_in, int opus_len,
                    uint8_t *pcm_out, int pcm_max_len)
{
    if (!opus_in || !pcm_out || opus_len <= 0 || pcm_max_len <= 0) {
        return -1;
    }

    /* TODO: Implement actual Opus decoding */
    /* For now, just copy raw data (not production-ready) */
    int out_len = (opus_len < pcm_max_len) ? opus_len : pcm_max_len;
    for (int i = 0; i < out_len; i++) {
        pcm_out[i] = opus_in[i];
    }

    ESP_LOGD(TAG, "Decode: %d -> %d bytes (stub)", opus_len, out_len);
    return out_len;
}
