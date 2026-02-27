#include "hal_audio.h"
#include "driver/i2s_std.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "HAL_AUDIO"

/* I2S configuration (from PRD.md) */
#define I2S_NUM         I2S_NUM_0
#define I2S_MCLK        10
#define I2S_BCLK        11
#define I2S_LRCK        12
#define I2S_DOUT        15  /* Speaker */
#define I2S_DIN         16  /* Microphone */
#define SAMPLE_RATE     16000
#define DMA_BUF_COUNT   8
#define DMA_BUF_LEN     1024

static i2s_chan_handle_t tx_handle = NULL;  /* Speaker */
static i2s_chan_handle_t rx_handle = NULL;  /* Microphone */
static bool is_initialized = false;

int hal_audio_start(void)
{
    if (is_initialized) {
        return 0;
    }

    /* I2S channel configuration */
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num = DMA_BUF_COUNT;
    chan_cfg.dma_frame_num = DMA_BUF_LEN;

    /* Create TX channel (speaker) */
    if (i2s_new_channel(&chan_cfg, &tx_handle, NULL) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create TX channel");
        return -1;
    }

    /* Create RX channel (microphone) */
    if (i2s_new_channel(&chan_cfg, NULL, &rx_handle) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create RX channel");
        return -1;
    }

    /* Standard mode configuration */
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIP_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_MCLK,
            .bclk = I2S_BCLK,
            .ws = I2S_LRCK,
            .dout = I2S_DOUT,
            .din = I2S_DIN,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    /* Initialize TX channel */
    if (i2s_channel_init_std_mode(tx_handle, &std_cfg) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init TX channel");
        return -1;
    }

    /* Initialize RX channel */
    if (i2s_channel_init_std_mode(rx_handle, &std_cfg) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init RX channel");
        return -1;
    }

    /* Enable channels */
    i2s_channel_enable(tx_handle);
    i2s_channel_enable(rx_handle);

    is_initialized = true;
    ESP_LOGI(TAG, "Audio initialized");
    return 0;
}

int hal_audio_read(uint8_t *out_buf, int max_len)
{
    if (!is_initialized || !rx_handle) {
        return -1;
    }

    size_t bytes_read = 0;
    esp_err_t ret = i2s_channel_read(rx_handle, out_buf, max_len, &bytes_read, pdMS_TO_TICKS(100));

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Read error: %s", esp_err_to_name(ret));
        return -1;
    }

    return (int)bytes_read;
}

int hal_audio_write(const uint8_t *data, int len)
{
    if (!is_initialized || !tx_handle) {
        return -1;
    }

    size_t bytes_written = 0;
    esp_err_t ret = i2s_channel_write(tx_handle, data, len, &bytes_written, pdMS_TO_TICKS(100));

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

    if (tx_handle) {
        i2s_channel_disable(tx_handle);
        i2s_del_channel(tx_handle);
        tx_handle = NULL;
    }

    if (rx_handle) {
        i2s_channel_disable(rx_handle);
        i2s_del_channel(rx_handle);
        rx_handle = NULL;
    }

    is_initialized = false;
    ESP_LOGI(TAG, "Audio stopped");
    return 0;
}
