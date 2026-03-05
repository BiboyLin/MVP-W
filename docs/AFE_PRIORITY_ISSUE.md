# AFE Ringbuffer 优先级同步问题

## 问题描述

ESP-SR AFE (Audio Front-End) 的 ringbuffer 存在写入/读取时序同步问题，无论 detection_task 优先级如何设置都会出现问题：

| detection_task 优先级 | 现象 | 原因 |
|---------------------|------|------|
| 4 (低于 voice_task) | Ringbuffer Full | voice_task 写入太快，detection_task 来不及 fetch() |
| 5 (等于 voice_task) | 不确定 | 取决于任务调度 |
| 6 (高于 voice_task) | Ringbuffer Empty | detection_task 读取太快，voice_task 来不及 feed() |

## 根本原因

### 任务架构

```
voice_recorder_task (优先级 5, 60ms 轮询)
    │
    ▼
hal_audio_read() → hal_wake_word_feed() → AFE ringbuffer
    │
    │ (低优先级，等待 CPU)
    ▼
detection_task (原优先级 6)
    │
    ▼
AFE fetch() → 检测唤醒词
```

### 问题本质

两个任务的执行速率不匹配：

1. **voice_recorder_task**: 每 60ms 执行一次，读取一帧音频并喂入 AFE
2. **detection_task**: 每次 fetch() 消耗一帧，但执行时机不确定

当 detection_task 优先级高于 voice_task 时：
- detection_task 立即执行 fetch()
- 但 voice_task 还没来得及执行 feed()
- 导致 ringbuffer 为空

当 detection_task 优先级低于 voice_task 时：
- voice_task 持续喂入数据
- detection_task 没有足够 CPU 时间及时读取
- 导致 ringbuffer 满

## 已尝试的修复方案

### 方案 1: 调整优先级 (失败)

| 优先级配置 | 结果 |
|-----------|------|
| detection=4, voice=5 | Ringbuffer Full |
| detection=5, voice=5 | 不确定 |
| detection=6, voice=5 | Ringbuffer Empty |

### 方案 2: 添加启动延迟 (部分有效但未完全解决)

在 `hal_wake_word_start()` 中添加 100ms 延迟：
- 首次启动时可以缓冲几帧数据
- 但运行稳定后仍然出现时序问题

## 可能的解决方案

### 方案 A: 使用信号量同步

```c
// 在 detection_task 中，fetch() 之前等待信号量
SemaphoreHandle_t feed_done_sem = xSemaphoreCreateBinary();

// voice_recorder_task 中，feed() 之后释放信号量
void hal_wake_word_feed(...) {
    // ... feed data
    xSemaphoreGive(feed_done_sem);
}

// detection_task 中
void detection_task(void *arg) {
    while (1) {
        xSemaphoreTake(feed_done_sem, portMAX_DELAY);
        afe_fetch_result_t *res = ctx->afe_iface->fetch(ctx->afe_data);
        // ... process result
    }
}
```

**问题**: 每帧都需要信号量，可能引入额外开销

### 方案 B: 在 feed() 中同步执行 fetch()

修改 `hal_wake_word_feed()`，在喂入数据后立即调用 fetch()：

```c
void hal_wake_word_feed(wake_word_ctx_t *ctx, const int16_t *samples, size_t num_samples) {
    // ... 喂入数据

    // 立即 fetch 确保数据被消费
    while (ctx->afe_iface->get_feed_chunksize(ctx->afe_data) <= ctx->input_buffer_size) {
        afe_fetch_result_t *res = ctx->afe_iface->fetch(ctx->afe_data);
        if (res && res->wakeup_state == WAKENET_DETECTED) {
            // 处理唤醒词检测
        }
    }
}
```

**问题**: 可能阻塞 voice_recorder_task，需要测试

### 方案 C: 增大 AFE ringbuffer

在 AFE 配置中增大 buffer 大小，但这需要修改 ESP-SR 库，可能不可行

### 方案 D: 降低 voice_recorder_task 优先级

将 voice_recorder_task 从 5 降到 4，让 detection_task (6) 有机会先执行：

```c
// button_voice.c
BaseType_t ret = xTaskCreate(
    voice_recorder_task,
    "voice_task",
    4096,
    NULL,
    4,  // 从 5 降到 4
    &g_voice_task_handle
);
```

**测试结果**: 未测试

### 方案 E: 使用队列代替直接调用

创建 FreeRTOS 队列，voice_recorder_task 将音频数据放入队列，detection_task 从队列取出：

```c
QueueHandle_t audio_queue = xQueueCreate(10, sizeof(audio_frame_t));

// voice_recorder_task
xQueueSend(audio_queue, &frame, 0);

// detection_task
if (xQueueReceive(audio_queue, &frame, 0)) {
    ctx->afe_iface->feed(ctx->afe_data, frame.samples);
    // 同时 fetch
}
```

**问题**: 增加复杂度和延迟

## 当前代码状态

文件: `firmware/s3/main/hal_wake_word.c`

```c
// 当前配置
#define DETECTION_TASK_PRIO    6  // 高优先级
#define DETECTION_TASK_STACK   4096

// hal_wake_word_start() 中有 100ms 延迟
void hal_wake_word_start(wake_word_ctx_t *ctx) {
    xEventGroupSetBits(ctx->event_group, DETECTION_RUNNING_BIT);
    vTaskDelay(pdMS_TO_TICKS(100));  // 启动延迟
    ESP_LOGI(TAG, "Wake word detection started");
}
```

## 下一步建议

1. **测试方案 D**: 降低 voice_recorder_task 优先级到 4
2. **测试方案 B**: 在 feed() 中同步执行 fetch()
3. **联系 ESP-SR 社区**: 确认是否已知问题

## 相关文件

- `firmware/s3/main/hal_wake_word.c` - AFE 初始化和检测任务
- `firmware/s3/main/button_voice.c` - 语音录制任务
- `firmware/s3/main/hal_audio.c` - I2S 音频驱动

## 参考

- ESP-SR 文档: https://github.com/espressif/esp-sr
- AFE API: `esp_afe_sr_iface_t`
