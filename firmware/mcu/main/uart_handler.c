#include "uart_handler.h"
#include "uart_protocol.h"
#include "servo_control.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG         "UART"
#define UART_NUM    UART_NUM_2
#define UART_TX     17
#define UART_RX     16
#define UART_BAUD   115200
#define RX_BUF      512

/* ------------------------------------------------------------------ */

static void uart_rx_task(void *arg)
{
    (void)arg;
    uint8_t raw[RX_BUF];
    char    line[64];
    int     line_len = 0;

    while (1) {
        int n = uart_read_bytes(UART_NUM, raw, sizeof(raw) - 1,
                                pdMS_TO_TICKS(100));
        for (int i = 0; i < n; i++) {
            char c = (char)raw[i];

            if (c == '\n' || c == '\r') {
                /* Complete line received — parse and dispatch */
                line[line_len] = '\0';
                char axis;
                int  angle;

                if (parse_axis_cmd(line, &axis, &angle) == 0) {
                    ESP_LOGI(TAG, "%c → %d°", axis, angle);
                    servo_set_angle(axis == 'X' ? SERVO_X : SERVO_Y, angle);
                } else if (line_len > 0) {
                    ESP_LOGW(TAG, "unknown cmd: '%s'", line);
                }
                line_len = 0;

            } else if (c != '\r' && line_len < (int)sizeof(line) - 1) {
                line[line_len++] = c;
            }
        }
    }
}

/* ------------------------------------------------------------------ */

void uart_handler_init(void)
{
    uart_config_t cfg = {
        .baud_rate  = UART_BAUD,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
    };
    uart_param_config(UART_NUM, &cfg);
    uart_set_pin(UART_NUM, UART_TX, UART_RX,
                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(UART_NUM, RX_BUF * 2, 0, 0, NULL, 0);
}

void uart_handler_start_task(void)
{
    xTaskCreate(uart_rx_task, "uart_rx", 4096, NULL, 10, NULL);
}
