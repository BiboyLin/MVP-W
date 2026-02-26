#pragma once
#include <stdint.h>

/* Servo PWM parameters (50 Hz, 13-bit LEDC) */
#define SERVO_FREQ      50u     /* Hz                    */
#define SERVO_RES       13u     /* LEDC timer resolution */
#define SERVO_MIN_US    500u    /* pulse width at   0°   */
#define SERVO_MAX_US    2500u   /* pulse width at 180°   */

/**
 * Convert servo angle to LEDC duty count.
 *
 * Formula: pulse_us = MIN + angle*(MAX-MIN)/180
 *          duty     = pulse_us * 2^RES / (1e6/FREQ)
 *
 * Typical results (50 Hz, 13-bit):
 *   0°  → 500 µs  → duty 204
 *   90° → 1500 µs → duty 614
 *   180°→ 2500 µs → duty 1024
 *
 * @param angle  Desired angle, 0-180 (clamped to range).
 * @return       LEDC duty value in [0, 2^SERVO_RES - 1].
 */
uint32_t angle_to_duty(int angle);
