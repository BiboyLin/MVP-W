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
        return -1;
    }

    /* Send raw PCM directly, no AUD1 header */
    int sent = esp_websocket_client_send_bin(ws_client, (const char *)data, len, pdMS_TO_TICKS(1000));

    if (sent != len) {
        ESP_LOGW(TAG, "Audio send incomplete: %d/%d", sent, len);
        return -1;
    }

    ESP_LOGD(TAG, "Sent raw PCM: %d bytes", sent);
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

    ESP_LOGI(TAG, "TTS audio received: %d bytes raw PCM", len);

    /* Update display to speaking animation */
    display_update(NULL, "speaking", 0, NULL);

    /* Start audio playback if not already started */
    hal_audio_start();

    /* Play raw PCM directly */
    int written = hal_audio_write(data, len);
    if (written != len) {
        ESP_LOGW(TAG, "TTS playback incomplete: %d/%d", written, len);
    }

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
