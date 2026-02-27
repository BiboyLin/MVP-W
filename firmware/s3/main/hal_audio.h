#ifndef HAL_AUDIO_H
#define HAL_AUDIO_H

#include <stdint.h>

/**
 * Start audio capture/playback
 */
int hal_audio_start(void);

/**
 * Read audio samples from microphone
 * @param out_buf Output buffer
 * @param max_len Maximum length
 * @return Number of bytes read, or -1 on error
 */
int hal_audio_read(uint8_t *out_buf, int max_len);

/**
 * Write audio samples to speaker
 * @param data Audio data
 * @param len Data length
 * @return Number of bytes written, or -1 on error
 */
int hal_audio_write(const uint8_t *data, int len);

/**
 * Stop audio capture/playback
 */
int hal_audio_stop(void);

#endif /* HAL_AUDIO_H */
