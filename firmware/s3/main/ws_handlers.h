#ifndef WS_HANDLERS_H
#define WS_HANDLERS_H

#include "ws_router.h"

/**
 * @file ws_handlers.h
 * @brief WebSocket message handlers for Watcher device
 *
 * These handlers are called by ws_router when receiving messages from cloud.
 * Each handler performs the appropriate action (UART, display, etc.)
 */

/* ------------------------------------------------------------------ */
/* Handler Functions (to be registered with ws_router)                */
/* ------------------------------------------------------------------ */

/**
 * Handle servo command - forward to MCU via UART
 * @param cmd Servo command with x, y angles
 */
void on_servo_handler(const ws_servo_cmd_t *cmd);

/**
 * Handle display command - update screen text and emoji
 * @param cmd Display command with text, emoji, size
 */
void on_display_handler(const ws_display_cmd_t *cmd);

/**
 * Handle status command - update display based on AI state
 * @param cmd Status command with state and message
 */
void on_status_handler(const ws_status_cmd_t *cmd);

/**
 * Handle capture command - take photo (MVP: not implemented)
 * @param cmd Capture command with quality
 */
void on_capture_handler(const ws_capture_cmd_t *cmd);

/**
 * Handle reboot command - restart device
 */
void on_reboot_handler(void);

/* ------------------------------------------------------------------ */
/* Helper Functions (for testing)                                     */
/* ------------------------------------------------------------------ */

/**
 * Map status state string to emoji string
 * @param state State string (thinking, speaking, idle, error)
 * @return Emoji string (analyzing, speaking, standby, sad)
 */
const char* ws_state_to_emoji(const char *state);

/**
 * Get all handlers as a router struct (convenience function)
 * @return ws_router_t with all handlers populated
 */
ws_router_t ws_handlers_get_router(void);

#endif /* WS_HANDLERS_H */
