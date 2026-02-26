#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "servo_control.h"
#include "uart_handler.h"
#include "led_indicator.h"

#define TAG "MAIN"

void app_main(void)
{
    ESP_LOGI(TAG, "MVP-W MCU v1.0 — starting");

    /* 1. Initialize peripherals */
    led_indicator_init();
    servo_control_init();
    uart_handler_init();

    /* 2. Startup: 3 blinks = boot OK */
    led_blink(3, 200);

    /* 3. Move servos to center (90°/90°) */
    servo_smooth_move(90, 90, 500);

    /* 4. Solid LED = ready */
    led_set(true);

    ESP_LOGI(TAG, "Ready — UART2 RX:GPIO16  TX:GPIO17  115200 8N1");
    ESP_LOGI(TAG, "Protocol: 'X:<0-180>\\r\\n'  'Y:<0-180>\\r\\n'");

    /* 5. Start UART listener (FreeRTOS task) */
    uart_handler_start_task();

    /* Main task has nothing more to do */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
