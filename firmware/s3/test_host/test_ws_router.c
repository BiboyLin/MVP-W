#include "unity.h"
#include "ws_router.h"
#include <string.h>

/* ------------------------------------------------------------------ */
/* Mock handlers - track if they were called and with what arguments  */
/* ------------------------------------------------------------------ */

static bool servo_called = false;
static bool tts_called = false;
static bool display_called = false;
static bool status_called = false;
static bool capture_called = false;
static bool reboot_called = false;

static ws_servo_cmd_t last_servo;
static ws_display_cmd_t last_display;
static ws_status_cmd_t last_status;
static ws_capture_cmd_t last_capture;

void mock_servo_handler(const ws_servo_cmd_t *cmd) {
    servo_called = true;
    last_servo = *cmd;
}

void mock_tts_handler(const ws_tts_cmd_t *cmd) {
    tts_called = true;
    (void)cmd;
}

void mock_display_handler(const ws_display_cmd_t *cmd) {
    display_called = true;
    last_display = *cmd;
}

void mock_status_handler(const ws_status_cmd_t *cmd) {
    status_called = true;
    last_status = *cmd;
}

void mock_capture_handler(const ws_capture_cmd_t *cmd) {
    capture_called = true;
    last_capture = *cmd;
}

void mock_reboot_handler(void) {
    reboot_called = true;
}

void reset_mocks(void) {
    servo_called = false;
    tts_called = false;
    display_called = false;
    status_called = false;
    capture_called = false;
    reboot_called = false;
    memset(&last_servo, 0, sizeof(last_servo));
    memset(&last_display, 0, sizeof(last_display));
    memset(&last_status, 0, sizeof(last_status));
    memset(&last_capture, 0, sizeof(last_capture));
}

/* ------------------------------------------------------------------ */
/* Setup / Teardown                                                   */
/* ------------------------------------------------------------------ */

void setUp(void) {
    reset_mocks();

    ws_router_t router = {
        .on_servo   = mock_servo_handler,
        .on_tts     = mock_tts_handler,
        .on_display = mock_display_handler,
        .on_status  = mock_status_handler,
        .on_capture = mock_capture_handler,
        .on_reboot  = mock_reboot_handler,
    };
    ws_router_init(&router);
}

void tearDown(void) {
}

/* ------------------------------------------------------------------ */
/* Test: Message Type Detection                                       */
/* ------------------------------------------------------------------ */

void test_route_servo_message(void) {
    const char *json = "{\"type\":\"servo\",\"x\":90,\"y\":45}";

    ws_msg_type_t type = ws_route_message(json);

    TEST_ASSERT_EQUAL(WS_MSG_SERVO, type);
    TEST_ASSERT_TRUE(servo_called);
    TEST_ASSERT_EQUAL_INT(90, last_servo.x);
    TEST_ASSERT_EQUAL_INT(45, last_servo.y);
}

void test_route_tts_message(void) {
    const char *json = "{\"type\":\"tts\",\"format\":\"opus\",\"data\":\"SGVsbG8=\"}";

    ws_msg_type_t type = ws_route_message(json);

    TEST_ASSERT_EQUAL(WS_MSG_TTS, type);
    TEST_ASSERT_TRUE(tts_called);
}

void test_route_display_message(void) {
    const char *json = "{\"type\":\"display\",\"text\":\"Hello\",\"emoji\":\"happy\",\"size\":32}";

    ws_msg_type_t type = ws_route_message(json);

    TEST_ASSERT_EQUAL(WS_MSG_DISPLAY, type);
    TEST_ASSERT_TRUE(display_called);
    TEST_ASSERT_EQUAL_STRING("Hello", last_display.text);
    TEST_ASSERT_EQUAL_STRING("happy", last_display.emoji);
    TEST_ASSERT_EQUAL_INT(32, last_display.size);
}

void test_route_display_without_optional_fields(void) {
    const char *json = "{\"type\":\"display\",\"text\":\"Test\"}";

    ws_msg_type_t type = ws_route_message(json);

    TEST_ASSERT_EQUAL(WS_MSG_DISPLAY, type);
    TEST_ASSERT_TRUE(display_called);
    TEST_ASSERT_EQUAL_STRING("Test", last_display.text);
    /* emoji and size are optional, should be empty/0 */
    TEST_ASSERT_EQUAL_STRING("", last_display.emoji);
    TEST_ASSERT_EQUAL_INT(0, last_display.size);
}

void test_route_status_message(void) {
    const char *json = "{\"type\":\"status\",\"state\":\"thinking\",\"message\":\"Processing...\"}";

    ws_msg_type_t type = ws_route_message(json);

    TEST_ASSERT_EQUAL(WS_MSG_STATUS, type);
    TEST_ASSERT_TRUE(status_called);
    TEST_ASSERT_EQUAL_STRING("thinking", last_status.state);
    TEST_ASSERT_EQUAL_STRING("Processing...", last_status.message);
}

void test_route_capture_message(void) {
    const char *json = "{\"type\":\"capture\",\"quality\":80}";

    ws_msg_type_t type = ws_route_message(json);

    TEST_ASSERT_EQUAL(WS_MSG_CAPTURE, type);
    TEST_ASSERT_TRUE(capture_called);
    TEST_ASSERT_EQUAL_INT(80, last_capture.quality);
}

void test_route_reboot_message(void) {
    const char *json = "{\"type\":\"reboot\"}";

    ws_msg_type_t type = ws_route_message(json);

    TEST_ASSERT_EQUAL(WS_MSG_REBOOT, type);
    TEST_ASSERT_TRUE(reboot_called);
}

void test_route_unknown_type(void) {
    const char *json = "{\"type\":\"unknown\"}";

    ws_msg_type_t type = ws_route_message(json);

    TEST_ASSERT_EQUAL(WS_MSG_UNKNOWN, type);
    TEST_ASSERT_FALSE(servo_called);
}

void test_route_invalid_json(void) {
    const char *json = "not a json";

    ws_msg_type_t type = ws_route_message(json);

    TEST_ASSERT_EQUAL(WS_MSG_UNKNOWN, type);
}

void test_route_missing_type(void) {
    const char *json = "{\"x\":90,\"y\":45}";

    ws_msg_type_t type = ws_route_message(json);

    TEST_ASSERT_EQUAL(WS_MSG_UNKNOWN, type);
}

void test_route_null_input(void) {
    ws_msg_type_t type = ws_route_message(NULL);

    TEST_ASSERT_EQUAL(WS_MSG_UNKNOWN, type);
}

/* ------------------------------------------------------------------ */
/* Test: Servo Command Parsing                                        */
/* ------------------------------------------------------------------ */

void test_parse_servo_valid(void) {
    const char *json = "{\"type\":\"servo\",\"x\":0,\"y\":180}";
    ws_servo_cmd_t cmd;

    int ret = ws_parse_servo(json, &cmd);

    TEST_ASSERT_EQUAL_INT(0, ret);
    TEST_ASSERT_EQUAL_INT(0, cmd.x);
    TEST_ASSERT_EQUAL_INT(180, cmd.y);
}

void test_parse_servo_center(void) {
    const char *json = "{\"type\":\"servo\",\"x\":90,\"y\":90}";
    ws_servo_cmd_t cmd;

    int ret = ws_parse_servo(json, &cmd);

    TEST_ASSERT_EQUAL_INT(0, ret);
    TEST_ASSERT_EQUAL_INT(90, cmd.x);
    TEST_ASSERT_EQUAL_INT(90, cmd.y);
}

void test_parse_servo_null_output(void) {
    const char *json = "{\"type\":\"servo\",\"x\":90,\"y\":90}";

    int ret = ws_parse_servo(json, NULL);

    TEST_ASSERT_EQUAL_INT(-1, ret);
}

/* ------------------------------------------------------------------ */
/* Test: Display Command Parsing                                      */
/* ------------------------------------------------------------------ */

void test_parse_display_with_emoji(void) {
    const char *json = "{\"type\":\"display\",\"text\":\"Hi\",\"emoji\":\"sad\"}";
    ws_display_cmd_t cmd;

    int ret = ws_parse_display(json, &cmd);

    TEST_ASSERT_EQUAL_INT(0, ret);
    TEST_ASSERT_EQUAL_STRING("Hi", cmd.text);
    TEST_ASSERT_EQUAL_STRING("sad", cmd.emoji);
}

void test_parse_display_without_optional_fields(void) {
    const char *json = "{\"type\":\"display\",\"text\":\"Test\"}";
    ws_display_cmd_t cmd;

    int ret = ws_parse_display(json, &cmd);

    TEST_ASSERT_EQUAL_INT(0, ret);
    TEST_ASSERT_EQUAL_STRING("Test", cmd.text);
    /* emoji and size are optional, should be empty/0 */
    TEST_ASSERT_EQUAL_STRING("", cmd.emoji);
    TEST_ASSERT_EQUAL_INT(0, cmd.size);
}

/* ------------------------------------------------------------------ */
/* Test: Status Command Parsing                                       */
/* ------------------------------------------------------------------ */

void test_parse_status_thinking(void) {
    const char *json = "{\"type\":\"status\",\"state\":\"thinking\",\"message\":\"Processing\"}";
    ws_status_cmd_t cmd;

    int ret = ws_parse_status(json, &cmd);

    TEST_ASSERT_EQUAL_INT(0, ret);
    TEST_ASSERT_EQUAL_STRING("thinking", cmd.state);
    TEST_ASSERT_EQUAL_STRING("Processing", cmd.message);
}

void test_parse_status_speaking(void) {
    const char *json = "{\"type\":\"status\",\"state\":\"speaking\",\"message\":\"Replying\"}";
    ws_status_cmd_t cmd;

    int ret = ws_parse_status(json, &cmd);

    TEST_ASSERT_EQUAL_INT(0, ret);
    TEST_ASSERT_EQUAL_STRING("speaking", cmd.state);
    TEST_ASSERT_EQUAL_STRING("Replying", cmd.message);
}

/* ------------------------------------------------------------------ */
/* Main                                                               */
/* ------------------------------------------------------------------ */

int main(void) {
    UNITY_BEGIN();

    /* Message type detection */
    RUN_TEST(test_route_servo_message);
    RUN_TEST(test_route_tts_message);
    RUN_TEST(test_route_display_message);
    RUN_TEST(test_route_display_without_optional_fields);
    RUN_TEST(test_route_status_message);
    RUN_TEST(test_route_capture_message);
    RUN_TEST(test_route_reboot_message);
    RUN_TEST(test_route_unknown_type);
    RUN_TEST(test_route_invalid_json);
    RUN_TEST(test_route_missing_type);
    RUN_TEST(test_route_null_input);

    /* Servo parsing */
    RUN_TEST(test_parse_servo_valid);
    RUN_TEST(test_parse_servo_center);
    RUN_TEST(test_parse_servo_null_output);

    /* Display parsing */
    RUN_TEST(test_parse_display_with_emoji);
    RUN_TEST(test_parse_display_without_optional_fields);

    /* Status parsing */
    RUN_TEST(test_parse_status_thinking);
    RUN_TEST(test_parse_status_speaking);

    return UNITY_END();
}
