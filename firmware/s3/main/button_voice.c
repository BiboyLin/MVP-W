#include "button_voice.h"
#include "hal_audio.h"
#include "hal_opus.h"
#include "hal_button.h"
#include "ws_client.h"
#include "display_ui.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

#define TAG "VOICE"

/* ------------------------------------------------------------------ */
/* Private: State and statistics                                       */
/* ------------------------------------------------------------------ */

static voice_state_t g_state = VOICE_STATE_IDLE;
static voice_stats_t g_stats = {0};

/* Audio buffer for PCM data (16kHz, 16-bit, 60ms frame = 1920 bytes) */
#define PCM_FRAME_SIZE  1920
#define OPUS_FRAME_SIZE 256  /* Approximate max Opus frame size */

static uint8_t g_pcm_buf[PCM_FRAME_SIZE];
static uint8_t g_opus_buf[OPUS_FRAME_SIZE];

/* ------------------------------------------------------------------ */
/* Public: Initialize                                                 */
/* ------------------------------------------------------------------ */

void voice_recorder_init(void)
{
    g_state = VOICE_STATE_IDLE;
    memset(&g_stats, 0, sizeof(g_stats));
}

/* ------------------------------------------------------------------ */
/* Public: Get current state                                          */
/* ------------------------------------------------------------------ */

voice_state_t voice_recorder_get_state(void)
{
    return g_state;
}

/* ------------------------------------------------------------------ */
/* Public: Reset statistics                                           */
/* ------------------------------------------------------------------ */

void voice_recorder_reset_stats(void)
{
    g_stats.record_count = 0;
    g_stats.encode_count = 0;
    g_stats.error_count = 0;
}

/* ------------------------------------------------------------------ */
/* Public: Get statistics                                             */
/* ------------------------------------------------------------------ */

void voice_recorder_get_stats(voice_stats_t *out_stats)
{
    if (out_stats) {
        out_stats->record_count = g_stats.record_count;
        out_stats->encode_count = g_stats.encode_count;
        out_stats->error_count = g_stats.error_count;
        out_stats->current_state = (int)g_state;
    }
}

/* ------------------------------------------------------------------ */
/* Private: Start recording                                           */
/* ------------------------------------------------------------------ */

static int start_recording(void)
{
    if (hal_audio_start() != 0) {
        g_stats.error_count++;
        return -1;
    }
    g_state = VOICE_STATE_RECORDING;
    return 0;
}

/* ------------------------------------------------------------------ */
/* Private: Stop recording                                            */
/* ------------------------------------------------------------------ */

static int stop_recording(void)
{
    hal_audio_stop();

    /* Send audio end marker */
    if (ws_send_audio_end() != 0) {
        g_stats.error_count++;
        /* Still transition to idle */
    }

    g_state = VOICE_STATE_IDLE;
    g_stats.record_count++;
    return 0;
}

/* ------------------------------------------------------------------ */
/* Public: Process event                                              */
/* ------------------------------------------------------------------ */

void voice_recorder_process_event(voice_event_t event)
{
    switch (g_state) {
        case VOICE_STATE_IDLE:
            if (event == VOICE_EVENT_BUTTON_PRESS) {
                start_recording();
            }
            break;

        case VOICE_STATE_RECORDING:
            if (event == VOICE_EVENT_BUTTON_RELEASE ||
                event == VOICE_EVENT_TIMEOUT) {
                stop_recording();
            }
            break;
    }
}

/* ------------------------------------------------------------------ */
/* Public: Process tick (read, encode, send)                          */
/* ------------------------------------------------------------------ */

int voice_recorder_tick(void)
{
    if (g_state != VOICE_STATE_RECORDING) {
        return 0;
    }

    /* Read audio samples */
    int pcm_len = hal_audio_read(g_pcm_buf, PCM_FRAME_SIZE);
    if (pcm_len < 0) {
        g_stats.error_count++;
        return -1;
    }
    if (pcm_len == 0) {
        return 0;  /* No data available */
    }

    /* Encode to Opus */
    int opus_len = hal_opus_encode(g_pcm_buf, pcm_len, g_opus_buf, OPUS_FRAME_SIZE);
    if (opus_len < 0) {
        g_stats.error_count++;
        return -1;
    }

    /* Send via WebSocket */
    if (ws_send_audio(g_opus_buf, opus_len) != 0) {
        g_stats.error_count++;
        return -1;
    }

    g_stats.encode_count++;
    return 1;  /* One frame encoded and sent */
}

/* ------------------------------------------------------------------ */
/* Private: Button callback (called from task context via poll)        */
/* ------------------------------------------------------------------ */

static void button_callback(bool pressed)
{
    /* This is called from task context (via hal_button_poll) */
    if (pressed) {
        ESP_LOGI(TAG, "Button PRESSED - starting recording");
        voice_recorder_process_event(VOICE_EVENT_BUTTON_PRESS);
        display_update("Recording...", "normal", 0, NULL);
    } else {
        ESP_LOGI(TAG, "Button RELEASED - stopping recording");
        voice_recorder_process_event(VOICE_EVENT_BUTTON_RELEASE);
        display_update("Processing...", "thinking", 0, NULL);
    }
}

/* ------------------------------------------------------------------ */
/* Private: Voice recorder task                                        */
/* ------------------------------------------------------------------ */

static TaskHandle_t g_voice_task_handle = NULL;
static volatile bool g_task_running = false;

/* Tick interval: 60ms for Opus frame size */
#define TICK_INTERVAL_MS    60

static void voice_recorder_task(void *arg)
{
    ESP_LOGI(TAG, "Voice recorder task started");
    ESP_LOGI(TAG, "Button polling disabled for MVP - use voice command or timeout");

    while (g_task_running) {
        /* Button polling disabled for MVP
         * TODO: Integrate esp_io_expander component for proper button support
         * hal_button_poll();
         */

        /* Process audio encoding/sending if recording */
        voice_recorder_tick();

        vTaskDelay(pdMS_TO_TICKS(TICK_INTERVAL_MS));
    }

    g_voice_task_handle = NULL;
    vTaskDelete(NULL);
}

/* ------------------------------------------------------------------ */
/* Public: Start voice recorder system (with button and task)         */
/* ------------------------------------------------------------------ */

int voice_recorder_start(void)
{
    /* Button via IO expander disabled for MVP
     * TODO: Integrate esp_io_expander component for proper button support
     * For now, use a timer or other trigger method
     */
    ESP_LOGI(TAG, "Voice recorder started (button polling disabled)");

    /* Start voice recorder task */
    g_task_running = true;
    BaseType_t ret = xTaskCreate(
        voice_recorder_task,
        "voice_task",
        4096,
        NULL,
        5,
        &g_voice_task_handle
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Task create failed");
        g_task_running = false;
        return -1;
    }

    ESP_LOGI(TAG, "Voice recorder started");
    return 0;
}

/* ------------------------------------------------------------------ */
/* Public: Stop voice recorder system                                  */
/* ------------------------------------------------------------------ */

void voice_recorder_stop(void)
{
    g_task_running = false;

    if (g_voice_task_handle) {
        vTaskDelay(pdMS_TO_TICKS(100));  /* Wait for task to exit */
    }

    /* hal_button_deinit(); -- Button disabled for MVP */
    ESP_LOGI(TAG, "Voice recorder stopped");
}
