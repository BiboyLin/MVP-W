#pragma once
#include <stdbool.h>

/** Configure GPIO 2 as output. */
void led_indicator_init(void);

/** Set LED on or off. */
void led_set(bool on);

/**
 * Blink LED a fixed number of times (blocking).
 * @param times      Number of blink cycles.
 * @param period_ms  Full on+off cycle duration in ms.
 */
void led_blink(int times, int period_ms);
