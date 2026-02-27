#ifndef WS_ROUTER_H
#define WS_ROUTER_H

#include <stdint.h>
#include <stdbool.h>

/* Message types (from PRD.md) */
typedef enum {
    WS_MSG_UNKNOWN = 0,

    /* Control commands (Cloud -> Watcher) */
    WS_MSG_SERVO,           /* {"type": "servo", "x": 90, "y": 45} */
    WS_MSG_TTS,             /* {"type": "tts", "format": "opus", "data": "<base64>"} */
    WS_MSG_DISPLAY,         /* {"type": "display", "text": "...", "emoji": "happy", "size": 24} */
    WS_MSG_CAPTURE,         /* {"type": "capture", "quality": 80} */
    WS_MSG_STATUS,          /* {"type": "status", "state": "thinking", "message": "..."} */
    WS_MSG_REBOOT,          /* {"type": "reboot"} */

    /* Media streams (Watcher -> Cloud) */
    WS_MSG_AUDIO,           /* {"type": "audio", "format": "opus", "sample_rate": 16000, "data": "<base64>"} */
    WS_MSG_AUDIO_END,       /* {"type": "audio_end"} */
    WS_MSG_VIDEO,           /* {"type": "video", "format": "jpeg", "width": 412, "height": 412, "data": "<base64>"} */
    WS_MSG_SENSOR,          /* {"type": "sensor", "co2": 400, "temperature": 25.5, "humidity": 60} */

    /* System messages */
    WS_MSG_PING,
    WS_MSG_PONG,
    WS_MSG_ERROR,
    WS_MSG_CONNECTED,
} ws_msg_type_t;

/* Servo command structure */
typedef struct {
    int x;  /* 0-180 */
    int y;  /* 0-180 */
} ws_servo_cmd_t;

/* TTS command structure */
typedef struct {
    char format[16];        /* "opus" */
    const char *data_b64;   /* base64 encoded opus data (pointer to original JSON, valid during callback only) */
    int data_len;           /* length of data_b64 string */
} ws_tts_cmd_t;

/* Display command structure */
#define WS_DISPLAY_TEXT_MAX  128
#define WS_DISPLAY_EMOJI_MAX 16
typedef struct {
    char text[WS_DISPLAY_TEXT_MAX];   /* text to display */
    char emoji[WS_DISPLAY_EMOJI_MAX]; /* happy/sad/surprised/angry/normal (optional) */
    int size;                         /* font size (default 24) */
} ws_display_cmd_t;

/* Status command structure */
#define WS_STATUS_STATE_MAX   16
#define WS_STATUS_MESSAGE_MAX 256
typedef struct {
    char state[WS_STATUS_STATE_MAX];    /* thinking/speaking/idle/error */
    char message[WS_STATUS_MESSAGE_MAX]; /* status message */
} ws_status_cmd_t;

/* Capture command structure */
typedef struct {
    int quality;            /* JPEG quality (1-100) */
} ws_capture_cmd_t;

/* Audio stream structure */
typedef struct {
    const char *format;     /* "opus" */
    int sample_rate;        /* 16000 */
    const char *data_b64;   /* base64 encoded opus data */
    int data_len;           /* length of data_b64 string */
} ws_audio_cmd_t;

/* Video stream structure */
typedef struct {
    const char *format;     /* "jpeg" */
    int width;
    int height;
    const char *data_b64;   /* base64 encoded jpeg data */
    int data_len;           /* length of data_b64 string */
} ws_video_cmd_t;

/* Sensor data structure */
typedef struct {
    int co2;
    float temperature;
    float humidity;
} ws_sensor_cmd_t;

/* Handler function types (to be implemented by application) */
typedef void (*ws_servo_handler_t)(const ws_servo_cmd_t *cmd);
typedef void (*ws_tts_handler_t)(const ws_tts_cmd_t *cmd);
typedef void (*ws_display_handler_t)(const ws_display_cmd_t *cmd);
typedef void (*ws_status_handler_t)(const ws_status_cmd_t *cmd);
typedef void (*ws_capture_handler_t)(const ws_capture_cmd_t *cmd);
typedef void (*ws_reboot_handler_t)(void);

/* Router context */
typedef struct {
    ws_servo_handler_t   on_servo;
    ws_tts_handler_t     on_tts;
    ws_display_handler_t on_display;
    ws_status_handler_t  on_status;
    ws_capture_handler_t on_capture;
    ws_reboot_handler_t  on_reboot;
} ws_router_t;

/**
 * Initialize router with handlers
 */
void ws_router_init(ws_router_t *router);

/**
 * Route a JSON message to appropriate handler
 * @param json_str Null-terminated JSON string
 * @return Message type, or WS_MSG_UNKNOWN on parse error
 */
ws_msg_type_t ws_route_message(const char *json_str);

/**
 * Parse servo command from JSON
 * @param json_str JSON string
 * @param out_cmd Output structure
 * @return 0 on success, -1 on error
 */
int ws_parse_servo(const char *json_str, ws_servo_cmd_t *out_cmd);

/**
 * Parse display command from JSON
 * @param json_str JSON string
 * @param out_cmd Output structure
 * @return 0 on success, -1 on error
 */
int ws_parse_display(const char *json_str, ws_display_cmd_t *out_cmd);

/**
 * Parse status command from JSON
 * @param json_str JSON string
 * @param out_cmd Output structure
 * @return 0 on success, -1 on error
 */
int ws_parse_status(const char *json_str, ws_status_cmd_t *out_cmd);

#endif /* WS_ROUTER_H */
