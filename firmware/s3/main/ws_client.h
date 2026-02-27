#ifndef WS_CLIENT_H
#define WS_CLIENT_H

#include <stdint.h>

/**
 * Initialize WebSocket client
 */
int ws_client_init(void);

/**
 * Start WebSocket connection
 */
int ws_client_start(void);

/**
 * Stop WebSocket connection
 */
void ws_client_stop(void);

/**
 * Send binary data via WebSocket
 * @param data Data buffer
 * @param len Data length
 * @return 0 on success, -1 on error
 */
int ws_client_send_binary(const uint8_t *data, int len);

/**
 * Send text message via WebSocket
 * @param text Text message
 * @return 0 on success, -1 on error
 */
int ws_client_send_text(const char *text);

/**
 * Check if WebSocket is connected
 */
int ws_client_is_connected(void);

/**
 * Send audio data via WebSocket
 * @param data Audio data (Opus encoded)
 * @param len Data length
 * @return 0 on success, -1 on error
 */
int ws_send_audio(const uint8_t *data, int len);

/**
 * Send audio end marker via WebSocket
 * @return 0 on success, -1 on error
 */
int ws_send_audio_end(void);

#endif /* WS_CLIENT_H */
