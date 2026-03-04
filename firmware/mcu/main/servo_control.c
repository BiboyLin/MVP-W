#include "servo_control.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>

/* GPIO assignment */
#define SERVO_X_GPIO    12
#define SERVO_Y_GPIO    15

/* LEDC assignment */
#define LEDC_TIMER      LEDC_TIMER_0
#define LEDC_MODE       LEDC_LOW_SPEED_MODE
#define LEDC_CH_X       LEDC_CHANNEL_0
#define LEDC_CH_Y       LEDC_CHANNEL_1

/* Smooth-move settings */
#define STEP_MS         10      /* interpolation interval (ms) */
#define SMOOTH_SPEED    2       /* degrees per step */

/* Y-axis mechanical protection limits */
#define SERVO_Y_MIN     90
#define SERVO_Y_MAX     150

static int s_current[2] = {90, 90};    /* current actual angles */
static int s_target[2]  = {90, 90};    /* target angles from commands */
static int s_start[2]   = {90, 90};    /* start angles for synchronized move */
static int s_steps[2]   = {0, 0};      /* total steps needed for sync move */
static int s_step[2]    = {0, 0};      /* current step in sync move */
static bool s_sync_mode = false;       /* true = use synchronized interpolation */
static TaskHandle_t s_smooth_task = NULL;

/* ------------------------------------------------------------------ */
/* Internal: apply angle to hardware immediately */
static void servo_apply_hardware(servo_axis_t axis, int angle)
{
    ledc_channel_t ch = (axis == SERVO_X) ? LEDC_CH_X : LEDC_CH_Y;
    ledc_set_duty(LEDC_MODE, ch, angle_to_duty(angle));
    ledc_update_duty(LEDC_MODE, ch);
}

/* ------------------------------------------------------------------ */
/* Background task: smoothly interpolate current -> target */
static void servo_smooth_task(void *arg)
{
    (void)arg;
    while (1) {
        if (s_sync_mode) {
            /* Synchronized mode: both axes move proportionally to finish together */
            bool still_moving = false;

            for (int axis = 0; axis < 2; axis++) {
                if (s_step[axis] < s_steps[axis]) {
                    s_step[axis]++;

                    /* Linear interpolation: current = start + (target - start) * progress */
                    int progress = s_step[axis];
                    int total = s_steps[axis];
                    int interpolated = s_start[axis] + (s_target[axis] - s_start[axis]) * progress / total;

                    s_current[axis] = interpolated;
                    servo_apply_hardware(axis, interpolated);
                    still_moving = true;
                }
            }

            /* Exit sync mode when both axes complete */
            if (!still_moving) {
                s_sync_mode = false;
                /* Ensure we end exactly at target */
                s_current[0] = s_target[0];
                s_current[1] = s_target[1];
                servo_apply_hardware(SERVO_X, s_target[0]);
                servo_apply_hardware(SERVO_Y, s_target[1]);
            }
        } else {
            /* Independent mode: each axis moves at its own pace */
            for (int axis = 0; axis < 2; axis++) {
                int cur = s_current[axis];
                int tgt = s_target[axis];

                if (cur != tgt) {
                    /* Move one step toward target */
                    if (abs(tgt - cur) <= SMOOTH_SPEED) {
                        s_current[axis] = tgt;
                    } else if (tgt > cur) {
                        s_current[axis] = cur + SMOOTH_SPEED;
                    } else {
                        s_current[axis] = cur - SMOOTH_SPEED;
                    }
                    servo_apply_hardware(axis, s_current[axis]);
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(STEP_MS));
    }
}

/* ------------------------------------------------------------------ */

void servo_control_init(void)
{
    /* Configure LEDC timer */
    ledc_timer_config_t timer = {
        .duty_resolution = SERVO_RES,
        .freq_hz         = SERVO_FREQ,
        .speed_mode      = LEDC_MODE,
        .timer_num       = LEDC_TIMER,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&timer);

    /* Configure X channel */
    ledc_channel_config_t ch_x = {
        .channel    = LEDC_CH_X,
        .duty       = angle_to_duty(90),
        .gpio_num   = SERVO_X_GPIO,
        .speed_mode = LEDC_MODE,
        .hpoint     = 0,
        .timer_sel  = LEDC_TIMER,
    };
    ledc_channel_config(&ch_x);

    /* Configure Y channel */
    ledc_channel_config_t ch_y = {
        .channel    = LEDC_CH_Y,
        .duty       = angle_to_duty(90),
        .gpio_num   = SERVO_Y_GPIO,
        .speed_mode = LEDC_MODE,
        .hpoint     = 0,
        .timer_sel  = LEDC_TIMER,
    };
    ledc_channel_config(&ch_y);

    /* Start background smooth task */
    xTaskCreate(servo_smooth_task, "servo_smooth", 2048, NULL, 5, &s_smooth_task);
}

/* ------------------------------------------------------------------ */

void servo_set_angle(servo_axis_t axis, float angle)
{
    /* Round to nearest integer */
    int rounded = (int)lroundf(angle);

    /* Global limits */
    if (rounded < 0)   rounded = 0;
    if (rounded > 180) rounded = 180;

    /* Y-axis mechanical protection */
    if (axis == SERVO_Y) {
        if (rounded < SERVO_Y_MIN) rounded = SERVO_Y_MIN;
        if (rounded > SERVO_Y_MAX) rounded = SERVO_Y_MAX;
    }

    /* Disable sync mode when setting individual axis */
    s_sync_mode = false;

    /* Set target - background task will smooth to it */
    s_target[axis] = rounded;
}

/* ------------------------------------------------------------------ */

void servo_set_angle_sync(int x, int y)
{
    /* Clamp X axis */
    if (x < 0) x = 0;
    if (x > 180) x = 180;

    /* Clamp Y axis (with mechanical protection) */
    if (y < SERVO_Y_MIN) y = SERVO_Y_MIN;
    if (y > SERVO_Y_MAX) y = SERVO_Y_MAX;

    /* Store start positions (current actual angles) */
    s_start[0] = s_current[0];
    s_start[1] = s_current[1];

    /* Store target positions */
    s_target[0] = x;
    s_target[1] = y;

    /* Calculate steps needed for each axis */
    int dist_x = abs(x - s_start[0]);
    int dist_y = abs(y - s_start[1]);
    int steps_x = (dist_x + SMOOTH_SPEED - 1) / SMOOTH_SPEED;  /* ceil division */
    int steps_y = (dist_y + SMOOTH_SPEED - 1) / SMOOTH_SPEED;

    /* Use the maximum steps for BOTH axes so they finish together */
    int max_steps = steps_x > steps_y ? steps_x : steps_y;

    /* If already at target, no movement needed */
    if (max_steps == 0) {
        s_sync_mode = false;
        return;
    }

    s_steps[0] = max_steps;
    s_steps[1] = max_steps;
    s_step[0] = 0;
    s_step[1] = 0;

    /* Enable synchronized mode */
    s_sync_mode = true;
}

/* ------------------------------------------------------------------ */

bool servo_is_moving(void)
{
    if (s_sync_mode) {
        return (s_step[0] < s_steps[0]) || (s_step[1] < s_steps[1]);
    } else {
        return (s_current[0] != s_target[0]) || (s_current[1] != s_target[1]);
    }
}

/* ------------------------------------------------------------------ */

void servo_set_angle_immediate(servo_axis_t axis, int angle)
{
    /* Global limits */
    if (angle < 0)   angle = 0;
    if (angle > 180) angle = 180;

    /* Y-axis mechanical protection */
    if (axis == SERVO_Y) {
        if (angle < SERVO_Y_MIN) angle = SERVO_Y_MIN;
        if (angle > SERVO_Y_MAX) angle = SERVO_Y_MAX;
    }

    /* Set both target and current - no smoothing */
    s_target[axis] = angle;
    s_current[axis] = angle;
    servo_apply_hardware(axis, angle);
}

/* ------------------------------------------------------------------ */

int servo_get_angle(servo_axis_t axis)
{
    return s_current[axis];
}

/* ------------------------------------------------------------------ */

int servo_get_target(servo_axis_t axis)
{
    return s_target[axis];
}
