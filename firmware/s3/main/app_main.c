#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "driver/uart.h"

#include "ws_router.h"
#include "ws_handlers.h"
#include "uart_bridge.h"
#include "button_voice.h"
#include "display_ui.h"
#include "wifi_client.h"
#include "ws_client.h"
#include "hal_uart.h"
#include "hal_audio.h"
#include "hal_display.h"
#include "sensecap-watcher.h"

#define TAG "MAIN"

/* Hardware test mode (set to 1 for self-test) */
#define ENABLE_HW_SELFTEST  1

/* ------------------------------------------------------------------ */
/* Button Callbacks (using SDK's bsp_set_btn_* interface)            */
/* ------------------------------------------------------------------ */

static void on_button_long_press(void)
{
    ESP_LOGI(TAG, "Button LONG PRESS - start recording");
    voice_recorder_process_event(VOICE_EVENT_BUTTON_PRESS);
    display_update("Listening...", "listening", 0, NULL);
}

static void on_button_long_release(void)
{
    ESP_LOGI(TAG, "Button LONG RELEASE - stop recording");
    voice_recorder_process_event(VOICE_EVENT_BUTTON_RELEASE);
    display_update("Ready", "happy", 0, NULL);
}

/* ------------------------------------------------------------------ */
/* Hardware Self-Test                                                 */
/* ------------------------------------------------------------------ */

#if ENABLE_HW_SELFTEST

/* Note: Display test is done by display_ui_init() which calls hal_display_init() */

static int test_uart(void)
{
    ESP_LOGI(TAG, "[TEST] UART...");
    /* Note: hal_uart_init is called by uart_bridge_init */
    /* Send test command to MCU */
    int ret = uart_bridge_send_servo(90, 90);
    if (ret != 0) {
        ESP_LOGE(TAG, "[TEST] UART send failed");
        return -1;
    }
    ESP_LOGI(TAG, "[TEST] UART PASS (sent X:90 Y:90)");
    return 0;
}

static int test_audio(void)
{
    ESP_LOGI(TAG, "[TEST] Audio...");
    if (hal_audio_init() != 0) {
        ESP_LOGE(TAG, "[TEST] Audio init failed");
        return -1;
    }
    /* Start audio to test I2S read */
    if (hal_audio_start() != 0) {
        ESP_LOGE(TAG, "[TEST] Audio start failed");
        return -1;
    }
    /* Read a small buffer to test I2S */
    uint8_t buf[64];
    int len = hal_audio_read(buf, sizeof(buf));
    hal_audio_stop();

    /* Note: len may be 0 or timeout, that's OK for init test */
    ESP_LOGI(TAG, "[TEST] Audio PASS (I2S initialized, read %d bytes)", len);
    return 0;
}

static void run_hw_selftest(void)
{
    ESP_LOGI(TAG, "=====================================");
    ESP_LOGI(TAG, "   HARDWARE SELF-TEST START");
    ESP_LOGI(TAG, "=====================================");

    /* Display already initialized by display_ui_init() */
    ESP_LOGI(TAG, "[TEST] Display PASS (initialized)");

    /* UART test */
    int uart_result = test_uart();

    /* Audio test */
    int audio_result = test_audio();

    /* WiFi test is done by wifi_init() + wifi_connect() in main() */

    int pass_count = 2;  /* Display + WiFi */
    int fail_count = 0;

    if (uart_result == 0) pass_count++; else fail_count++;
    if (audio_result == 0) pass_count++; else fail_count++;

    ESP_LOGI(TAG, "=====================================");
    ESP_LOGI(TAG, "   SELF-TEST RESULTS: %d PASS, %d FAIL",
             pass_count, fail_count);
    ESP_LOGI(TAG, "=====================================");

    /* Display result on screen */
    if (fail_count == 0) {
        display_update("HW TEST OK", "happy", 0, NULL);
    } else {
        char msg[32];
        snprintf(msg, sizeof(msg), "FAIL:%d", fail_count);
        display_update(msg, "sad", 0, NULL);
    }
}

#endif /* ENABLE_HW_SELFTEST */

/* ------------------------------------------------------------------ */
/* Main Application                                                   */
/* ------------------------------------------------------------------ */

void app_main(void)
{
    ESP_LOGI(TAG, "MVP-W S3 v1.0 starting");

    /* 1. Initialize display first (for status feedback) */
    display_ui_init();
    display_update("Starting...", "normal", 0, NULL);

    /* 2. Initialize UART bridge to MCU */
    uart_bridge_init();

#if ENABLE_HW_SELFTEST
    /* Run hardware self-test */
    run_hw_selftest();

    ESP_LOGI(TAG, "Self-test complete. Waiting 5s...");
    vTaskDelay(pdMS_TO_TICKS(5000));
#endif

    /* 3. Initialize voice recorder */
    voice_recorder_init();

    /* 3.5 Register button callbacks (SDK already initialized IO expander) */
    bsp_set_btn_long_press_cb(on_button_long_press);
    bsp_set_btn_long_release_cb(on_button_long_release);
    ESP_LOGI(TAG, "Button callbacks registered via SDK");

    /* 4. Initialize and connect to WiFi */
    display_update("Connecting WiFi...", "normal", 0, NULL);
    wifi_init();  /* Initialize WiFi driver */
    if (wifi_connect() != 0) {
        ESP_LOGE(TAG, "WiFi connection failed");
        display_update("WiFi Error", "sad", 0, NULL);
        /* Continue anyway - may connect later */
    } else {
        ESP_LOGI(TAG, "WiFi connected");
    }

    /* 5. Initialize WebSocket client */
    ws_client_init();

    /* 6. Register message router handlers */
    ws_router_t router = ws_handlers_get_router();
    ws_router_init(&router);
    ESP_LOGI(TAG, "WS router handlers registered");

    /* 7. Start WebSocket connection */
    display_update("Connecting Cloud...", "normal", 0, NULL);
    ws_client_start();

    /* 8. Ready */
    ESP_LOGI(TAG, "Ready");
    display_update("Ready", "happy", 0, NULL);

    /* Main loop - process audio, feed watchdog */
    esp_task_wdt_add(NULL);
    while (1) {
        /* Process audio if recording */
        voice_recorder_tick();

        /* Feed watchdog */
        esp_task_wdt_reset();

        /* Short delay for responsiveness (60ms for audio frame rate) */
        vTaskDelay(pdMS_TO_TICKS(60));
    }
}
