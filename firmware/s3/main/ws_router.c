#include "ws_router.h"
#include "cJSON.h"
#include <string.h>

/* ------------------------------------------------------------------ */
/* Private: Router context                                            */
/* ------------------------------------------------------------------ */

static ws_router_t g_router = {0};

/* ------------------------------------------------------------------ */
/* Public: Initialize router                                          */
/* ------------------------------------------------------------------ */

void ws_router_init(ws_router_t *router)
{
    if (router) {
        g_router = *router;
    }
}

/* ------------------------------------------------------------------ */
/* Private: Get string from cJSON object safely                       */
/* ------------------------------------------------------------------ */

static const char *get_string(cJSON *obj, const char *key)
{
    cJSON *item = cJSON_GetObjectItem(obj, key);
    if (item && cJSON_IsString(item)) {
        return item->valuestring;
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
/* Private: Get integer from cJSON object safely                      */
/* ------------------------------------------------------------------ */

static int get_int(cJSON *obj, const char *key, int default_val)
{
    cJSON *item = cJSON_GetObjectItem(obj, key);
    if (item && cJSON_IsNumber(item)) {
        return item->valueint;
    }
    return default_val;
}

/* ------------------------------------------------------------------ */
/* Private: Copy string to fixed buffer safely                         */
/* ------------------------------------------------------------------ */

static void copy_string(char *dst, size_t dst_size, const char *src)
{
    if (!dst || dst_size == 0) return;
    if (src) {
        strncpy(dst, src, dst_size - 1);
        dst[dst_size - 1] = '\0';
    } else {
        dst[0] = '\0';
    }
}

/* ------------------------------------------------------------------ */
/* Public: Route message to appropriate handler                       */
/* ------------------------------------------------------------------ */

ws_msg_type_t ws_route_message(const char *json_str)
{
    if (!json_str) {
        return WS_MSG_UNKNOWN;
    }

    cJSON *root = cJSON_Parse(json_str);
    if (!root) {
        return WS_MSG_UNKNOWN;
    }

    /* Get type field */
    const char *type = get_string(root, "type");
    if (!type) {
        cJSON_Delete(root);
        return WS_MSG_UNKNOWN;
    }

    ws_msg_type_t msg_type = WS_MSG_UNKNOWN;

    /* Route based on type */
    if (strcmp(type, "servo") == 0) {
        msg_type = WS_MSG_SERVO;
        if (g_router.on_servo) {
            ws_servo_cmd_t cmd = {
                .x = get_int(root, "x", 90),
                .y = get_int(root, "y", 90),
            };
            g_router.on_servo(&cmd);
        }
    }
    else if (strcmp(type, "tts") == 0) {
        msg_type = WS_MSG_TTS;
        if (g_router.on_tts) {
            ws_tts_cmd_t cmd = {0};
            copy_string(cmd.format, sizeof(cmd.format), get_string(root, "format"));
            cmd.data_b64 = get_string(root, "data");
            cmd.data_len = cmd.data_b64 ? (int)strlen(cmd.data_b64) : 0;
            g_router.on_tts(&cmd);
        }
    }
    else if (strcmp(type, "display") == 0) {
        msg_type = WS_MSG_DISPLAY;
        if (g_router.on_display) {
            ws_display_cmd_t cmd = {0};
            copy_string(cmd.text, sizeof(cmd.text), get_string(root, "text"));
            copy_string(cmd.emoji, sizeof(cmd.emoji), get_string(root, "emoji"));
            cmd.size = get_int(root, "size", 0);
            g_router.on_display(&cmd);
        }
    }
    else if (strcmp(type, "status") == 0) {
        msg_type = WS_MSG_STATUS;
        if (g_router.on_status) {
            ws_status_cmd_t cmd = {0};
            copy_string(cmd.state, sizeof(cmd.state), get_string(root, "state"));
            copy_string(cmd.message, sizeof(cmd.message), get_string(root, "message"));
            g_router.on_status(&cmd);
        }
    }
    else if (strcmp(type, "capture") == 0) {
        msg_type = WS_MSG_CAPTURE;
        if (g_router.on_capture) {
            ws_capture_cmd_t cmd = {
                .quality = get_int(root, "quality", 80),
            };
            g_router.on_capture(&cmd);
        }
    }
    else if (strcmp(type, "reboot") == 0) {
        msg_type = WS_MSG_REBOOT;
        if (g_router.on_reboot) {
            g_router.on_reboot();
        }
    }
    /* Media stream types (Watcher -> Cloud) - recognized but no handler */
    else if (strcmp(type, "audio") == 0) {
        msg_type = WS_MSG_AUDIO;
    }
    else if (strcmp(type, "audio_end") == 0) {
        msg_type = WS_MSG_AUDIO_END;
    }
    else if (strcmp(type, "video") == 0) {
        msg_type = WS_MSG_VIDEO;
    }
    else if (strcmp(type, "sensor") == 0) {
        msg_type = WS_MSG_SENSOR;
    }
    else if (strcmp(type, "ping") == 0) {
        msg_type = WS_MSG_PING;
    }
    else if (strcmp(type, "pong") == 0) {
        msg_type = WS_MSG_PONG;
    }
    else if (strcmp(type, "error") == 0) {
        msg_type = WS_MSG_ERROR;
    }
    else if (strcmp(type, "connected") == 0) {
        msg_type = WS_MSG_CONNECTED;
    }

    cJSON_Delete(root);
    return msg_type;
}

/* ------------------------------------------------------------------ */
/* Public: Parse servo command                                        */
/* ------------------------------------------------------------------ */

int ws_parse_servo(const char *json_str, ws_servo_cmd_t *out_cmd)
{
    if (!json_str || !out_cmd) {
        return -1;
    }

    cJSON *root = cJSON_Parse(json_str);
    if (!root) {
        return -1;
    }

    out_cmd->x = get_int(root, "x", 90);
    out_cmd->y = get_int(root, "y", 90);

    cJSON_Delete(root);
    return 0;
}

/* ------------------------------------------------------------------ */
/* Public: Parse display command                                      */
/* ------------------------------------------------------------------ */

int ws_parse_display(const char *json_str, ws_display_cmd_t *out_cmd)
{
    if (!json_str || !out_cmd) {
        return -1;
    }

    cJSON *root = cJSON_Parse(json_str);
    if (!root) {
        return -1;
    }

    memset(out_cmd, 0, sizeof(*out_cmd));
    copy_string(out_cmd->text, sizeof(out_cmd->text), get_string(root, "text"));
    copy_string(out_cmd->emoji, sizeof(out_cmd->emoji), get_string(root, "emoji"));
    out_cmd->size = get_int(root, "size", 0);

    cJSON_Delete(root);
    return 0;
}

/* ------------------------------------------------------------------ */
/* Public: Parse status command                                       */
/* ------------------------------------------------------------------ */

int ws_parse_status(const char *json_str, ws_status_cmd_t *out_cmd)
{
    if (!json_str || !out_cmd) {
        return -1;
    }

    cJSON *root = cJSON_Parse(json_str);
    if (!root) {
        return -1;
    }

    memset(out_cmd, 0, sizeof(*out_cmd));
    copy_string(out_cmd->state, sizeof(out_cmd->state), get_string(root, "state"));
    copy_string(out_cmd->message, sizeof(out_cmd->message), get_string(root, "message"));

    cJSON_Delete(root);
    return 0;
}
