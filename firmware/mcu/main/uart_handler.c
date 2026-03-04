#include "uart_handler.h"
#include "uart_protocol.h"
#include "servo_control.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"

#define TAG         "UART"
#define UART_NUM    UART_NUM_2
#define UART_TX     17
#define UART_RX     16
#define UART_BAUD   115200
#define RX_BUF      512

/* Sync command buffering */
#define SYNC_TIMEOUT_MS  50   /* Max time to wait for paired command */

static bool s_has_x = false;
static bool s_has_y = false;
static int  s_pending_x = 0;
static int  s_pending_y = 0;
static int64_t s_first_cmd_time = 0;

/* ------------------------------------------------------------------ */

/* Flush pending commands (either sync or individual) */
static void flush_pending_commands(void)
{
    if (s_has_x && s_has_y) {
        /* Both commands received - use synchronized mode */
        ESP_LOGI(TAG, "Sync move: X=%d, Y=%d", s_pending_x, s_pending_y);
        servo_set_angle_sync(s_pending_x, s_pending_y);
    } else if (s_has_x) {
        /* Only X received */
        ESP_LOGI(TAG, "X → %d° (solo)", s_pending_x);
        servo_set_angle(SERVO_X, s_pending_x);
    } else if (s_has_y) {
        /* Only Y received */
        ESP_LOGI(TAG, "Y → %d° (solo)", s_pending_y);
        servo_set_angle(SERVO_Y, s_pending_y);
    }

    /* Reset buffer */
    s_has_x = false;
    s_has_y = false;
}

/* ------------------------------------------------------------------ */

static void uart_rx_task(void *arg)
{
    (void)arg;
    uint8_t raw[RX_BUF];
    char    line[64];
    int     line_len = 0;

    while (1) {
        int n = uart_read_bytes(UART_NUM, raw, sizeof(raw) - 1,
                                pdMS_TO_TICKS(10));  /* Shorter timeout for sync */

        /* Check for sync timeout */
        if (s_has_x || s_has_y) {
            int64_t now = esp_timer_get_time() / 1000;  /* ms */
            if ((now - s_first_cmd_time) > SYNC_TIMEOUT_MS) {
                flush_pending_commands();
            }
        }

        for (int i = 0; i < n; i++) {
            char c = (char)raw[i];

            if (c == '\n' || c == '\r') {
                /* Complete line received — parse and dispatch */
                line[line_len] = '\0';
                char axis;
                int  angle;

                if (parse_axis_cmd(line, &axis, &angle) == 0) {
                    /* Buffer command for sync */
                    if (!s_has_x && !s_has_y) {
                        s_first_cmd_time = esp_timer_get_time() / 1000;
                    }

                    if (axis == 'X') {
                        s_pending_x = angle;
                        s_has_x = true;
                    } else {
                        s_pending_y = angle;
                        s_has_y = true;
                    }

                    /* If both received, flush immediately */
                    if (s_has_x && s_has_y) {
                        flush_pending_commands();
                    }
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
