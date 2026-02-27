#include "uart_bridge.h"
#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Private: Statistics                                                */
/* ------------------------------------------------------------------ */

static uart_bridge_t g_stats = {0};

/* ------------------------------------------------------------------ */
/* Public: Initialize                                                 */
/* ------------------------------------------------------------------ */

void uart_bridge_init(void)
{
    memset(&g_stats, 0, sizeof(g_stats));
}

/* ------------------------------------------------------------------ */
/* Public: Reset statistics                                           */
/* ------------------------------------------------------------------ */

void uart_bridge_reset_stats(void)
{
    g_stats.tx_count = 0;
    g_stats.error_count = 0;
}

/* ------------------------------------------------------------------ */
/* Public: Get statistics                                             */
/* ------------------------------------------------------------------ */

void uart_bridge_get_stats(uart_bridge_t *out_stats)
{
    if (out_stats) {
        *out_stats = g_stats;
    }
}

/* ------------------------------------------------------------------ */
/* Public: Send servo command                                         */
/* ------------------------------------------------------------------ */

int uart_bridge_send_servo(int x, int y)
{
    /* Clamp values to valid range */
    if (x < 0) x = 0;
    if (x > 180) x = 180;
    if (y < 0) y = 0;
    if (y > 180) y = 180;

    /* Format: "X:<x>\r\nY:<y>\r\n" */
    char buf[32];
    int len = snprintf(buf, sizeof(buf), "X:%d\r\nY:%d\r\n", x, y);

    if (len < 0 || len >= (int)sizeof(buf)) {
        g_stats.error_count++;
        return -1;
    }

    /* Send via HAL */
    int sent = hal_uart_send((uint8_t *)buf, len);
    if (sent != len) {
        g_stats.error_count++;
        return -1;
    }

    g_stats.tx_count++;
    return 0;
}
