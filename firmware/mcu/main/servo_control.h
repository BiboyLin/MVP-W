#pragma once
#include "servo_math.h"

typedef enum {
    SERVO_X = 0,  /* GPIO 12 — left/right */
    SERVO_Y = 1,  /* GPIO 15 — up/down    */
} servo_axis_t;

/**
 * Initialize LEDC timer, channels, and start background smooth task.
 * Must be called once at startup.
 */
void servo_control_init(void);

/**
 * Set target angle with automatic smoothing.
 * The servo will smoothly move to target in background.
 * @param axis   SERVO_X or SERVO_Y
 * @param angle  Target angle (0-180), float is rounded to integer
 */
void servo_set_angle(servo_axis_t axis, float angle);

/**
 * Set angle immediately without smoothing.
 * Use for initialization or emergency positioning.
 * @param axis   SERVO_X or SERVO_Y
 * @param angle  Target angle (0-180)
 */
void servo_set_angle_immediate(servo_axis_t axis, int angle);

/**
 * Get current actual angle (may differ from target during movement).
 * @param axis   SERVO_X or SERVO_Y
 * @return       Current angle (0-180)
 */
int servo_get_angle(servo_axis_t axis);

/**
 * Get target angle (last commanded value).
 * @param axis   SERVO_X or SERVO_Y
 * @return       Target angle (0-180)
 */
int servo_get_target(servo_axis_t axis);
