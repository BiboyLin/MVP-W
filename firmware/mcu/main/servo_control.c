#include "servo_control.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* GPIO assignment */
#define SERVO_X_GPIO    12
#define SERVO_Y_GPIO    15

/* LEDC assignment */
#define LEDC_TIMER      LEDC_TIMER_0
#define LEDC_MODE       LEDC_LOW_SPEED_MODE
#define LEDC_CH_X       LEDC_CHANNEL_0
#define LEDC_CH_Y       LEDC_CHANNEL_1

/* Smooth-move step interval */
#define STEP_MS         10

static int s_angle[2] = {90, 90};  /* current commanded angles */

/* ------------------------------------------------------------------ */

void servo_control_init(void)
{
    ledc_timer_config_t timer = {
        .duty_resolution = SERVO_RES,
        .freq_hz         = SERVO_FREQ,
        .speed_mode      = LEDC_MODE,
        .timer_num       = LEDC_TIMER,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&timer);

    ledc_channel_config_t ch_x = {
        .channel    = LEDC_CH_X,
        .duty       = angle_to_duty(90),
        .gpio_num   = SERVO_X_GPIO,
        .speed_mode = LEDC_MODE,
        .hpoint     = 0,
        .timer_sel  = LEDC_TIMER,
    };
    ledc_channel_config(&ch_x);

    ledc_channel_config_t ch_y = {
        .channel    = LEDC_CH_Y,
        .duty       = angle_to_duty(90),
        .gpio_num   = SERVO_Y_GPIO,
        .speed_mode = LEDC_MODE,
        .hpoint     = 0,
        .timer_sel  = LEDC_TIMER,
    };
    ledc_channel_config(&ch_y);
}

/* ------------------------------------------------------------------ */

void servo_set_angle(servo_axis_t axis, int angle)
{
    if (angle < 0)   angle = 0;
    if (angle > 180) angle = 180;
    s_angle[axis] = angle;

    ledc_channel_t ch = (axis == SERVO_X) ? LEDC_CH_X : LEDC_CH_Y;
    ledc_set_duty(LEDC_MODE, ch, angle_to_duty(angle));
    ledc_update_duty(LEDC_MODE, ch);
}

/* ------------------------------------------------------------------ */

void servo_smooth_move(int x_target, int y_target, int duration_ms)
{
    int x0 = s_angle[SERVO_X];
    int y0 = s_angle[SERVO_Y];

    int steps = duration_ms / STEP_MS;
    if (steps < 1) steps = 1;

    for (int i = 1; i <= steps; i++) {
        servo_set_angle(SERVO_X, x0 + (x_target - x0) * i / steps);
        servo_set_angle(SERVO_Y, y0 + (y_target - y0) * i / steps);
        vTaskDelay(pdMS_TO_TICKS(STEP_MS));
    }
}

/* ------------------------------------------------------------------ */

int servo_get_angle(servo_axis_t axis)
{
    return s_angle[axis];
}
