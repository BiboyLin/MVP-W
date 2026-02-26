/**
 * Host TDD — servo_math.c
 *
 * Run:
 *   cd firmware/mcu/test_host
 *   cmake -B build && cmake --build build
 *   ./build/test_servo_math        (Linux/macOS)
 *   build\Debug\test_servo_math    (Windows MSVC)
 */
#include "unity.h"
#include "servo_math.h"

void setUp(void)    {}
void tearDown(void) {}

/* ── known values ─────────────────────────────────────────────────── */

void test_duty_0deg(void)
{
    /* 0° → 500 µs → 500*8192/20000 = 204 */
    TEST_ASSERT_EQUAL_UINT32(204, angle_to_duty(0));
}

void test_duty_90deg(void)
{
    /* 90° → 1500 µs → 1500*8192/20000 = 614 */
    TEST_ASSERT_EQUAL_UINT32(614, angle_to_duty(90));
}

void test_duty_180deg(void)
{
    /* 180° → 2500 µs → 2500*8192/20000 = 1024 */
    TEST_ASSERT_EQUAL_UINT32(1024, angle_to_duty(180));
}

/* ── boundary / clamp ─────────────────────────────────────────────── */

void test_clamp_negative(void)
{
    TEST_ASSERT_EQUAL_UINT32(angle_to_duty(0), angle_to_duty(-1));
    TEST_ASSERT_EQUAL_UINT32(angle_to_duty(0), angle_to_duty(-180));
}

void test_clamp_over_180(void)
{
    TEST_ASSERT_EQUAL_UINT32(angle_to_duty(180), angle_to_duty(181));
    TEST_ASSERT_EQUAL_UINT32(angle_to_duty(180), angle_to_duty(360));
}

/* ── monotonicity: more angle → more duty ────────────────────────── */

void test_monotonic(void)
{
    for (int i = 0; i < 180; i++) {
        TEST_ASSERT_GREATER_OR_EQUAL(angle_to_duty(i), angle_to_duty(i + 1));
    }
}

/* ── range: duty must stay within 13-bit resolution ─────────────── */

void test_duty_within_resolution(void)
{
    uint32_t max_duty = (1u << SERVO_RES) - 1u;  /* 8191 */
    for (int a = 0; a <= 180; a++) {
        TEST_ASSERT_LESS_OR_EQUAL(max_duty, angle_to_duty(a));
    }
}

/* ── entry point ─────────────────────────────────────────────────── */

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_duty_0deg);
    RUN_TEST(test_duty_90deg);
    RUN_TEST(test_duty_180deg);
    RUN_TEST(test_clamp_negative);
    RUN_TEST(test_clamp_over_180);
    RUN_TEST(test_monotonic);
    RUN_TEST(test_duty_within_resolution);
    return UNITY_END();
}
