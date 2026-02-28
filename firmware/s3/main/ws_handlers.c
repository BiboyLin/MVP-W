/**
 * @file ws_handlers.c
 * @brief WebSocket message handlers implementation
 */

#include "ws_handlers.h"
#include "ws_client.h"
#include "uart_bridge.h"
#include "display_ui.h"
#include "esp_system.h"
#include <string.h>

/* ------------------------------------------------------------------ */
/* Helper: State to Emoji Mapping                                     */
/* ------------------------------------------------------------------ */

const char* ws_state_to_emoji(const char *state)
{
    if (!state) {
        return "standby";
    }

    if (strcmp(state, "thinking") == 0) {
        return "analyzing";
    }
    if (strcmp(state, "speaking") == 0) {
        return "speaking";
    }
    if (strcmp(state, "idle") == 0) {
        return "standby";
    }
    if (strcmp(state, "error") == 0) {
        return "sad";
    }

    /* Unknown state defaults to standby */
    return "standby";
}

/* ------------------------------------------------------------------ */
/* Handler: Servo Command                                             */
/* ------------------------------------------------------------------ */

void on_servo_handler(const ws_servo_cmd_t *cmd)
{
    if (!cmd) {
        return;
    }

    /* Forward to MCU via UART */
    uart_bridge_send_servo(cmd->x, cmd->y);
}

/* ------------------------------------------------------------------ */
/* Handler: Display Command                                           */
/* ------------------------------------------------------------------ */

void on_display_handler(const ws_display_cmd_t *cmd)
{
    if (!cmd) {
        return;
    }

    /* Get emoji, use "normal" if empty */
    const char *emoji = cmd->emoji;
    if (!emoji || emoji[0] == '\0') {
        emoji = "normal";
    }

    /* Update display */
    display_update(cmd->text, emoji, cmd->size, NULL);
}

/* ------------------------------------------------------------------ */
/* Handler: Status Command                                            */
/* ------------------------------------------------------------------ */

void on_status_handler(const ws_status_cmd_t *cmd)
{
    if (!cmd) {
        return;
    }

    /* Map state to emoji */
    const char *emoji = ws_state_to_emoji(cmd->state);

    /* Update display with message and appropriate emoji */
    display_update(cmd->message, emoji, 0, NULL);

    /* Stop TTS audio when transitioning to idle state */
    if (strcmp(cmd->state, "idle") == 0) {
        ws_tts_complete();
    }
}

/* ------------------------------------------------------------------ */
/* Handler: Capture Command (MVP: Not Implemented)                    */
/* ------------------------------------------------------------------ */

void on_capture_handler(const ws_capture_cmd_t *cmd)
{
    /* MVP: Camera capture not implemented */
    (void)cmd;
}

/* ------------------------------------------------------------------ */
/* Handler: Reboot Command                                            */
/* ------------------------------------------------------------------ */

void on_reboot_handler(void)
{
    esp_restart();
}

/* ------------------------------------------------------------------ */
/* Convenience: Get Router with All Handlers                          */
/* ------------------------------------------------------------------ */

ws_router_t ws_handlers_get_router(void)
{
    ws_router_t router = {
        .on_servo   = on_servo_handler,
        .on_tts     = NULL,  /* TTS uses binary frames, not JSON */
        .on_display = on_display_handler,
        .on_status  = on_status_handler,
        .on_capture = on_capture_handler,
        .on_reboot  = on_reboot_handler,
    };
    return router;
}
