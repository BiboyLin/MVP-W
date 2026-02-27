#include "ws_client.h"
#include "ws_router.h"
#include "esp_websocket_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "WS_CLIENT"

/* WebSocket configuration (MVP: hardcoded) */
#define WS_SERVER_URL  "ws://192.168.1.100:8766"
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
            break;

        case WEBSOCKET_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "WebSocket disconnected");
            is_connected = false;
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
        .timeout_ms = WS_TIMEOUT_MS,
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

int ws_send_audio(const uint8_t *data, int len)
{
    /* Format as JSON: {"type":"audio","format":"opus","sample_rate":16000,"data":"<base64>"} */
    /* For MVP, send raw binary with header */
    /* TODO: Implement base64 encoding */

    char header[64];
    snprintf(header, sizeof(header), "{\"type\":\"audio\",\"len\":%d,\"data\":\"", len);

    /* Send header */
    ws_client_send_text(header);

    /* Send binary data (simplified - should be base64) */
    ws_client_send_binary(data, len);

    /* Send footer */
    ws_client_send_text("\"}");

    return 0;
}

int ws_send_audio_end(void)
{
    return ws_client_send_text("{\"type\":\"audio_end\"}");
}
