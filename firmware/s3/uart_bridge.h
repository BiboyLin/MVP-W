#ifndef UART_BRIDGE_H
#define UART_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>

/* UART bridge context */
typedef struct {
    int tx_count;       /* Number of commands sent */
    int error_count;    /* Number of errors */
} uart_bridge_t;

/**
 * Initialize UART bridge
 */
void uart_bridge_init(void);

/**
 * Convert servo command to MCU protocol format and send
 *
 * Input: JSON servo command (x, y angles 0-180)
 * Output: "X:<x>\r\nY:<y>\r\n" sent via UART
 *
 * @param x X-axis angle (0-180)
 * @param y Y-axis angle (0-180)
 * @return 0 on success, -1 on error
 */
int uart_bridge_send_servo(int x, int y);

/**
 * Get bridge statistics
 */
void uart_bridge_get_stats(uart_bridge_t *out_stats);

/**
 * Reset statistics
 */
void uart_bridge_reset_stats(void);

/* ------------------------------------------------------------------ */
/* HAL Interface (to be implemented by hardware layer)                */
/* ------------------------------------------------------------------ */

/**
 * Send data via UART (HAL function)
 * @param data Pointer to data buffer
 * @param len Length of data
 * @return Number of bytes sent, or -1 on error
 */
int hal_uart_send(const uint8_t *data, int len);

#endif /* UART_BRIDGE_H */
