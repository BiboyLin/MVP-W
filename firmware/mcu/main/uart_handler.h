#pragma once

/** Configure UART2 (GPIO 16 RX / GPIO 17 TX, 115200 8N1). */
void uart_handler_init(void);

/** Start the UART receive task (FreeRTOS task, priority 10). */
void uart_handler_start_task(void);
