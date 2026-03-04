#pragma once
#include "servo_math.h"
#include <stdbool.h>

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
 * Set target angles for BOTH servos atomically (synchronized start).
 * Both servos will start moving at exactly the same time.
 * @param x  X-axis target angle (0-180)
 * @param y  Y-axis target angle (90-150)
 */
void servo_set_angle_sync(int x, int y);

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

/**
 * Check if either servo is still moving toward target.
 * @return  true if any servo is moving
 */
bool servo_is_moving(void);
