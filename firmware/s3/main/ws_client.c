#include "ws_client.h"
#include "ws_router.h"
#include "display_ui.h"
#include "hal_audio.h"
#include "hal_opus.h"
#include "esp_websocket_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#define TAG "WS_CLIENT"

/* WebSocket configuration (MVP: hardcoded) */
#define WS_SERVER_URL  "ws://192.168.31.10:8766"
#define WS_TIMEOUT_MS  10000

/* Audio binary frame magic */
#define AUDIO_MAGIC "AUD1"
#define AUDIO_HEADER_LEN 8

static esp_websocket_client_handle_t ws_client = NULL;
static bool is_connected = false;

static void ws_event_handler(void *handler_args, esp_event_base_t base,
                             int32_t event_id, void *event_data)
{
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;

    switch (event_id) {
        case WEBSOCKET_EVENT_CONNECTED:
            ESP_LOGI(TAG, "WebSocket connected");
            is_connected = true;
            /* Show happy greeting when connected */
            display_update(NULL, "happy", 0, NULL);
            break;

        case WEBSOCKET_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "WebSocket disconnected");
            is_connected = false;
            /* Show standby when disconnected */
            display_update("Disconnected", "standby", 0, NULL);
            break;

        case WEBSOCKET_EVENT_DATA:
            if (data->op_code == WS_TRANSPORT_OPCODES_TEXT) {
                /* Route text message */
                char *msg = strndup((char *)data->data_ptr, data->data_len);
                if (msg) {
                    ws_route_message(msg);
                    free(msg);
                }
            }
            else if (data->op_code == WS_TRANSPORT_OPCODES_BINARY) {
                /* Handle binary message (TTS audio) */
                ws_handle_tts_binary((const uint8_t *)data->data_ptr, data->data_len);
            }
            break;

        case WEBSOCKET_EVENT_ERROR:
            ESP_LOGE(TAG, "WebSocket error");
            break;

        default:
            break;
    }
}

int ws_client_init(void)
{
    esp_websocket_client_config_t cfg = {
        .uri = WS_SERVER_URL,
        .network_timeout_ms = WS_TIMEOUT_MS,
        .buffer_size = 4096,
    };

    ws_client = esp_websocket_client_init(&cfg);
    if (!ws_client) {
        ESP_LOGE(TAG, "Failed to init WebSocket client");
        return -1;
    }

    esp_websocket_register_events(ws_client, WEBSOCKET_EVENT_ANY, ws_event_handler, NULL);

    ESP_LOGI(TAG, "WebSocket client initialized (URL: %s)", WS_SERVER_URL);
    return 0;
}

int ws_client_start(void)
{
    if (!ws_client) {
        ESP_LOGE(TAG, "WebSocket not initialized");
        return -1;
    }

    esp_err_t ret = esp_websocket_client_start(ws_client);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start WebSocket: %s", esp_err_to_name(ret));
        return -1;
    }

    return 0;
}

void ws_client_stop(void)
{
    if (ws_client) {
        esp_websocket_client_stop(ws_client);
        esp_websocket_client_destroy(ws_client);
        ws_client = NULL;
        is_connected = false;
    }
}

int ws_client_send_binary(const uint8_t *data, int len)
{
    if (!ws_client || !is_connected) {
        return -1;
    }

    int sent = esp_websocket_client_send_bin(ws_client, (const char *)data, len, pdMS_TO_TICKS(1000));
    return sent;
}

int ws_client_send_text(const char *text)
{
    if (!ws_client || !is_connected) {
        return -1;
    }

    int sent = esp_websocket_client_send_text(ws_client, text, strlen(text), pdMS_TO_TICKS(1000));
    return sent;
}

int ws_client_is_connected(void)
{
    return is_connected ? 1 : 0;
}

/* ------------------------------------------------------------------ */
/* Implementation of WebSocket interface for button_voice             */
/* ------------------------------------------------------------------ */

/**
 * Audio protocol (MVP):
 *
 * Option 1: Binary frame with 4-byte header
 *   [0-3]   "AUD1" magic bytes
 *   [4-7]   uint32_t data length (little-endian)
 *   [8-n]   Opus encoded audio data
 *
 * Option 2: JSON with base64 (more overhead, use for production)
 *   {"type":"audio","format":"opus","sample_rate":16000,"data":"<base64>"}
 *
 * MVP uses Option 1 for simplicity and efficiency.
 */

#define AUDIO_MAGIC "AUD1"

int ws_send_audio(const uint8_t *data, int len)
{
    if (!ws_client || !is_connected || len <= 0) {
        return -1;
    }

    /* Build frame: 4-byte magic + 4-byte length + data */
    int frame_len = 4 + 4 + len;
    uint8_t *frame = malloc(frame_len);
    if (!frame) {
        ESP_LOGE(TAG, "Failed to allocate audio frame");
        return -1;
    }

    /* Copy header */
    memcpy(frame, AUDIO_MAGIC, 4);
    uint32_t data_len = len;
    memcpy(frame + 4, &data_len, 4);

    /* Copy audio data */
    memcpy(frame + 8, data, len);

    /* Send as binary frame */
    int sent = esp_websocket_client_send_bin(ws_client, (const char *)frame, frame_len, pdMS_TO_TICKS(1000));

    free(frame);

    if (sent != frame_len) {
        ESP_LOGW(TAG, "Audio send incomplete: %d/%d", sent, frame_len);
        return -1;
    }

    ESP_LOGD(TAG, "Sent audio frame: %d bytes (payload %d)", sent, len);
    return 0;
}

int ws_send_audio_end(void)
{
    /* Send audio end marker as JSON text frame */
    const char *msg = "{\"type\":\"audio_end\"}";
    return ws_client_send_text(msg);
}

/* ------------------------------------------------------------------ */
/* TTS Binary Frame Handling                                          */
/* ------------------------------------------------------------------ */

/**
 * Parse and play TTS audio from binary frame
 *
 * Frame format:
 *   [0-3]   "AUD1" magic bytes
 *   [4-7]   uint32_t data length (little-endian)
 *   [8-n]   Opus encoded audio data
 *
 * @param data Binary frame data
 * @param len Frame length
 */
void ws_handle_tts_binary(const uint8_t *data, int len)
{
    if (!data || len < AUDIO_HEADER_LEN) {
        ESP_LOGW(TAG, "TTS frame too short: %d bytes", len);
        return;
    }

    /* Check magic bytes */
    if (memcmp(data, AUDIO_MAGIC, 4) != 0) {
        ESP_LOGW(TAG, "TTS frame invalid magic: %.4s", data);
        return;
    }

    /* Parse data length */
    uint32_t opus_len;
    memcpy(&opus_len, data + 4, 4);

    if (opus_len == 0 || opus_len > (uint32_t)(len - AUDIO_HEADER_LEN)) {
        ESP_LOGW(TAG, "TTS frame invalid length: %" PRIu32 " (frame: %d)", opus_len, len);
        return;
    }

    const uint8_t *opus_data = data + AUDIO_HEADER_LEN;

    ESP_LOGI(TAG, "TTS audio received: %" PRIu32 " bytes Opus", opus_len);

    /* Update display to speaking animation */
    display_update(NULL, "speaking", 0, NULL);

    /* Decode Opus to PCM */
    uint8_t *pcm_buf = malloc(4096);  /* Enough for one Opus frame */
    if (!pcm_buf) {
        ESP_LOGE(TAG, "Failed to allocate PCM buffer");
        return;
    }

    int pcm_len = hal_opus_decode(opus_data, opus_len, pcm_buf, 4096);
    if (pcm_len > 0) {
        ESP_LOGI(TAG, "TTS decoded: %d bytes PCM", pcm_len);

        /* Start audio playback if not already started */
        hal_audio_start();

        /* Play PCM data */
        int written = hal_audio_write(pcm_buf, pcm_len);
        if (written != pcm_len) {
            ESP_LOGW(TAG, "TTS playback incomplete: %d/%d", written, pcm_len);
        }
    } else {
        ESP_LOGW(TAG, "TTS Opus decode failed");
    }

    free(pcm_buf);

    /* Note: Don't stop audio here - may receive more frames */
    /* Display will be updated by caller when TTS is complete */
}

/**
 * Signal TTS playback complete (called by application)
 */
void ws_tts_complete(void)
{
    hal_audio_stop();
    display_update(NULL, "happy", 0, NULL);
    ESP_LOGI(TAG, "TTS playback complete");
}
