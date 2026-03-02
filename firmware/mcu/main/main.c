#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "servo_control.h"
#include "uart_handler.h"
#include "led_indicator.h"

#define TAG "MAIN"

void app_main(void)
{
    ESP_LOGI(TAG, "MVP-W MCU v1.0 starting");

    /* 1. Initialize peripherals */
    led_indicator_init();
    servo_control_init();
    uart_handler_init();

    /* 2. Startup: 3 blinks = boot OK */
    led_blink(3, 200);

    /* 3. Set default position (X:90, Y:120) - will smooth automatically */
    servo_set_angle(SERVO_X, 90);
    servo_set_angle(SERVO_Y, 120);
    vTaskDelay(pdMS_TO_TICKS(800));  /* Wait for smooth move to complete */

    /* 4. Solid LED = ready */
    led_set(true);

    ESP_LOGI(TAG, "Ready - UART2 RX:GPIO16 TX:GPIO17 115200 8N1");
    ESP_LOGI(TAG, "Protocol: X:<0-180> Y:<0-180>");

    /* 5. Start UART listener (FreeRTOS task) */
    uart_handler_start_task();

    /* Main task has nothing more to do */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
