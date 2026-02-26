#include "servo_math.h"

uint32_t angle_to_duty(int angle)
{
    if (angle < 0)   angle = 0;
    if (angle > 180) angle = 180;

    /* Step 1: angle → pulse width (µs) */
    uint32_t pulse_us = SERVO_MIN_US
        + ((uint32_t)angle * (SERVO_MAX_US - SERVO_MIN_US) / 180u);

    /* Step 2: µs → LEDC duty count
     *   period_us = 1 000 000 / FREQ = 20 000 µs
     *   duty = pulse_us * 2^RES / period_us
     */
    return (pulse_us * (1u << SERVO_RES)) / (1000000u / SERVO_FREQ);
}
