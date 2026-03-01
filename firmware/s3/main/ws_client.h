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

/**
 * Handle TTS binary frame from WebSocket
 * @param data Binary frame data (AUD1 format)
 * @param len Frame length
 */
void ws_handle_tts_binary(const uint8_t *data, int len);

/**
 * Signal TTS playback complete
 * Call this when TTS session is finished
 */
void ws_tts_complete(void);

/**
 * Check TTS timeout and auto-complete if needed
 * Call this periodically from main loop
 */
void ws_tts_timeout_check(void);

#endif /* WS_CLIENT_H */
