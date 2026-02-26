#pragma once
#include "servo_math.h"

typedef enum {
    SERVO_X = 0,  /* GPIO 12 — left/right */
    SERVO_Y = 1,  /* GPIO 13 — up/down    */
} servo_axis_t;

/** Initialize LEDC timer and channels for both servo axes. */
void servo_control_init(void);

/**
 * Set one servo axis to a target angle immediately.
 * @param axis   SERVO_X or SERVO_Y
 * @param angle  0-180 (clamped)
 */
void servo_set_angle(servo_axis_t axis, int angle);

/**
 * Smoothly interpolate both axes to target positions.
 * Blocks the calling task for ~duration_ms.
 * @param x_target   Target X angle (0-180)
 * @param y_target   Target Y angle (0-180)
 * @param duration_ms  Movement duration in milliseconds
 */
void servo_smooth_move(int x_target, int y_target, int duration_ms);

/** Return current commanded angle for an axis. */
int servo_get_angle(servo_axis_t axis);
