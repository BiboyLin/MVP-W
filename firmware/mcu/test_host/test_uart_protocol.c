/**
 * Host TDD — uart_protocol.c
 *
 * Run:
 *   cd firmware/mcu/test_host
 *   cmake -B build && cmake --build build
 *   ./build/test_uart_protocol        (Linux/macOS)
 *   build\Debug\test_uart_protocol    (Windows MSVC)
 */
#include "unity.h"
#include "uart_protocol.h"

void setUp(void)    {}
void tearDown(void) {}

/* ── happy path ──────────────────────────────────────────────────── */

void test_parse_x_axis(void)
{
    char axis; int angle;
    TEST_ASSERT_EQUAL_INT(0, parse_axis_cmd("X:90", &axis, &angle));
    TEST_ASSERT_EQUAL_CHAR('X', axis);
    TEST_ASSERT_EQUAL_INT(90, angle);
}

void test_parse_y_axis(void)
{
    char axis; int angle;
    TEST_ASSERT_EQUAL_INT(0, parse_axis_cmd("Y:90", &axis, &angle));
    TEST_ASSERT_EQUAL_CHAR('Y', axis);
    TEST_ASSERT_EQUAL_INT(90, angle);
}

void test_parse_boundary_0(void)
{
    char axis; int angle;
    TEST_ASSERT_EQUAL_INT(0, parse_axis_cmd("X:0", &axis, &angle));
    TEST_ASSERT_EQUAL_INT(0, angle);
}

void test_parse_boundary_180(void)
{
    char axis; int angle;
    TEST_ASSERT_EQUAL_INT(0, parse_axis_cmd("X:180", &axis, &angle));
    TEST_ASSERT_EQUAL_INT(180, angle);
}

/* ── invalid axis ─────────────────────────────────────────────────── */

void test_invalid_axis_z(void)
{
    char axis; int angle;
    TEST_ASSERT_NOT_EQUAL(0, parse_axis_cmd("Z:90", &axis, &angle));
}

void test_invalid_axis_lowercase(void)
{
    char axis; int angle;
    TEST_ASSERT_NOT_EQUAL(0, parse_axis_cmd("x:90", &axis, &angle));
    TEST_ASSERT_NOT_EQUAL(0, parse_axis_cmd("y:90", &axis, &angle));
}

/* ── angle out of range ──────────────────────────────────────────── */

void test_angle_over_180(void)
{
    char axis; int angle;
    TEST_ASSERT_NOT_EQUAL(0, parse_axis_cmd("X:181", &axis, &angle));
    TEST_ASSERT_NOT_EQUAL(0, parse_axis_cmd("Y:360", &axis, &angle));
}

void test_angle_negative(void)
{
    char axis; int angle;
    /* "-1" would make sscanf parse '-' as separator mismatch → error */
    TEST_ASSERT_NOT_EQUAL(0, parse_axis_cmd("X:-1", &axis, &angle));
}

/* ── malformed input ─────────────────────────────────────────────── */

void test_missing_separator(void)
{
    char axis; int angle;
    TEST_ASSERT_NOT_EQUAL(0, parse_axis_cmd("X90",     &axis, &angle));
    TEST_ASSERT_NOT_EQUAL(0, parse_axis_cmd("Y 90",    &axis, &angle));
}

void test_empty_string(void)
{
    char axis; int angle;
    TEST_ASSERT_NOT_EQUAL(0, parse_axis_cmd("", &axis, &angle));
}

void test_garbage_input(void)
{
    char axis; int angle;
    TEST_ASSERT_NOT_EQUAL(0, parse_axis_cmd("garbage",  &axis, &angle));
    TEST_ASSERT_NOT_EQUAL(0, parse_axis_cmd("servo:90", &axis, &angle));
}

/* ── null guard ──────────────────────────────────────────────────── */

void test_null_line(void)
{
    char axis; int angle;
    TEST_ASSERT_NOT_EQUAL(0, parse_axis_cmd(NULL, &axis, &angle));
}

void test_null_out_axis(void)
{
    int angle;
    TEST_ASSERT_NOT_EQUAL(0, parse_axis_cmd("X:90", NULL, &angle));
}

void test_null_out_angle(void)
{
    char axis;
    TEST_ASSERT_NOT_EQUAL(0, parse_axis_cmd("X:90", &axis, NULL));
}

/* ── entry point ─────────────────────────────────────────────────── */

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_parse_x_axis);
    RUN_TEST(test_parse_y_axis);
    RUN_TEST(test_parse_boundary_0);
    RUN_TEST(test_parse_boundary_180);
    RUN_TEST(test_invalid_axis_z);
    RUN_TEST(test_invalid_axis_lowercase);
    RUN_TEST(test_angle_over_180);
    RUN_TEST(test_angle_negative);
    RUN_TEST(test_missing_separator);
    RUN_TEST(test_empty_string);
    RUN_TEST(test_garbage_input);
    RUN_TEST(test_null_line);
    RUN_TEST(test_null_out_axis);
    RUN_TEST(test_null_out_angle);
    return UNITY_END();
}
