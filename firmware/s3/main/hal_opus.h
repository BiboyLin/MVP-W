#ifndef HAL_OPUS_H
#define HAL_OPUS_H

#include <stdint.h>

/**
 * Initialize Opus encoder/decoder
 * @return 0 on success, -1 on error
 */
int hal_opus_init(void);

/**
 * Encode PCM data to Opus
 * @param pcm_in Input PCM data (16-bit, 16kHz, mono)
 * @param pcm_len Length of PCM data in bytes
 * @param opus_out Output buffer for Opus data
 * @param opus_max_len Max output buffer size
 * @return Encoded bytes on success, -1 on error
 */
int hal_opus_encode(const uint8_t *pcm_in, int pcm_len,
                    uint8_t *opus_out, int opus_max_len);

/**
 * Decode Opus data to PCM
 * @param opus_in Input Opus data
 * @param opus_len Length of Opus data in bytes
 * @param pcm_out Output buffer for PCM data
 * @param pcm_max_len Max output buffer size
 * @return Decoded bytes on success, -1 on error
 */
int hal_opus_decode(const uint8_t *opus_in, int opus_len,
                    uint8_t *pcm_out, int pcm_max_len);

#endif /* HAL_OPUS_H */
