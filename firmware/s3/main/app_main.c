#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_task_wdt.h"

#include "ws_router.h"
#include "uart_bridge.h"
#include "button_voice.h"
#include "display_ui.h"
#include "wifi_client.h"
#include "ws_client.h"

#define TAG "MAIN"

void app_main(void)
{
    ESP_LOGI(TAG, "MVP-W S3 v1.0 starting");

    /* 1. Initialize display first (for status feedback) */
    display_ui_init();
    display_update("Starting...", "normal", 0, NULL);

    /* 2. Initialize UART bridge to MCU */
    uart_bridge_init();

    /* 3. Initialize voice recorder */
    voice_recorder_init();

    /* 4. Initialize WiFi */
    wifi_init();

    /* 5. Connect to WiFi */
    display_update("Connecting WiFi...", "normal", 0, NULL);
    if (wifi_connect() != 0) {
        ESP_LOGE(TAG, "WiFi connection failed");
        display_update("WiFi Error", "sad", 0, NULL);
        /* Continue anyway - may connect later */
    } else {
        ESP_LOGI(TAG, "WiFi connected");
    }

    /* 6. Initialize WebSocket client */
    ws_client_init();

    /* 7. Register message router handlers */
    ws_router_t router = {
        .on_servo   = NULL,  /* Will forward to UART */
        .on_tts     = NULL,  /* Will play audio */
        .on_display = NULL,  /* Will update display */
        .on_status  = NULL,  /* Will update status */
        .on_capture = NULL,  /* Will capture image */
        .on_reboot  = NULL,  /* Will reboot */
    };
    ws_router_init(&router);

    /* 8. Start WebSocket connection */
    display_update("Connecting Cloud...", "normal", 0, NULL);
    ws_client_start();

    /* 9. Ready */
    ESP_LOGI(TAG, "Ready");
    display_update("Ready", "happy", 0, NULL);

    /* Main loop - feed watchdog */
    esp_task_wdt_add(NULL);
    while (1) {
        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
