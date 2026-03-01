#include "ws_client.h"
#include "ws_router.h"
#include "display_ui.h"
#include "hal_audio.h"
#include "esp_websocket_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdlib.h>
#include <string.h>

#define TAG "WS_CLIENT"

/* WebSocket configuration (MVP: hardcoded) */
#define WS_SERVER_URL  "ws://192.168.31.10:8766"
#define WS_TIMEOUT_MS  10000

static esp_websocket_client_handle_t ws_client = NULL;
static bool is_connected = false;
static bool tts_playing = false;  /* TTS playback state */

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
                /* Handle server text messages */
                char *msg = strndup((char *)data->data_ptr, data->data_len);
                if (msg) {
                    /* End TTS playback when receiving any text message */
                    if (tts_playing) {
                        ws_tts_complete();
                    }

                    /* Check for server protocol formats first */
                    if (strncmp(msg, "result:", 7) == 0) {
                        /* ASR result: "result: {text}" */
                        ESP_LOGI(TAG, "ASR result: %s", msg + 7);
                        display_update(msg + 7, "analyzing", 0, NULL);
                    }
                    else if (strcmp(msg, "tts:start") == 0) {
                        /* TTS start marker - prepare to receive audio */
                        ESP_LOGI(TAG, "TTS start, preparing audio playback");
                        /* Don't start audio here - will start on first binary chunk */
                    }
                    else if (strncmp(msg, "error:", 6) == 0) {
                        /* Error message */
                        ESP_LOGE(TAG, "Server error: %s", msg + 6);
                        display_update("Error", "sad", 0, NULL);
                    }
                    else {
                        /* Try JSON routing for other messages */
                        ws_route_message(msg);
                    }
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
        .buffer_size = 16384,  /* Increased for audio streaming (16KB) */
        .task_stack = 16384,   /* Increased stack size (16KB) */
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
 * Audio protocol (v1.1):
 *
 * Raw PCM binary frame (no header):
 *   [0-n]   PCM audio data (16-bit, 16kHz, mono)
 *
 * Simpler and more efficient for MVP.
 */

int ws_send_audio(const uint8_t *data, int len)
{
    if (!ws_client || !is_connected || len <= 0) {
        ESP_LOGW(TAG, "ws_send_audio: not ready (conn=%d, len=%d)", is_connected, len);
        return -1;
    }

    /* Send raw PCM directly, no AUD1 header */
    /* Increased timeout to 5 seconds for better reliability */
    int sent = esp_websocket_client_send_bin(ws_client, (const char *)data, len, pdMS_TO_TICKS(5000));

    if (sent != len) {
        ESP_LOGW(TAG, "Audio send incomplete: %d/%d", sent, len);
        return -1;
    }

    return 0;
}

int ws_send_audio_end(void)
{
    /* Send audio end marker (适配 watcher-server 协议) */
    return ws_client_send_text("over");
}

/* ------------------------------------------------------------------ */
/* TTS Binary Frame Handling                                          */
/* ------------------------------------------------------------------ */

/**
 * Play TTS audio from binary frame (Raw PCM)
 *
 * Frame format (v1.1):
 *   [0-n]   PCM audio data (16-bit, 16kHz, mono)
 *
 * @param data Binary frame data
 * @param len Frame length
 */
void ws_handle_tts_binary(const uint8_t *data, int len)
{
    if (!data || len <= 0) {
        ESP_LOGW(TAG, "TTS frame empty: %d bytes", len);
        return;
    }

    /* Only update display and start audio on first chunk */
    if (!tts_playing) {
        ESP_LOGI(TAG, "TTS started, first chunk: %d bytes", len);
        display_update(NULL, "speaking", 0, NULL);
        /* Switch to 24kHz for TTS playback (火山引擎 TTS) */
        hal_audio_set_sample_rate(24000);
        hal_audio_start();
        tts_playing = true;
    }

    /* Play raw PCM directly */
    ESP_LOGI(TAG, "Calling hal_audio_write(%d bytes)...", len);
    int written = hal_audio_write(data, len);
    ESP_LOGI(TAG, "hal_audio_write returned: %d", written);
    if (written != len) {
        ESP_LOGW(TAG, "TTS playback incomplete: %d/%d", written, len);
    }
}

/**
 * Signal TTS playback complete (called by application)
 */
void ws_tts_complete(void)
{
    if (tts_playing) {
        hal_audio_stop();
        /* Restore 16kHz for recording */
        hal_audio_set_sample_rate(16000);
        display_update(NULL, "happy", 0, NULL);
        tts_playing = false;
        ESP_LOGI(TAG, "TTS playback complete");
    }
}
