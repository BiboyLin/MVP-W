# 语音流协议规范

> **版本**: 2.0
> **日期**: 2026-03-11
> **范围**: 语音流（适配 watcher-server v2.0）
> **注意**: 详细协议见 `COMMUNICATION_PROTOCOL.md`

---

## 1. 音频格式

| 参数 | 录音 (ASR) | 播放 (TTS) |
|------|------------|------------|
| 格式 | PCM (Raw Binary) | PCM (Raw Binary) |
| 采样率 | 16kHz | 24kHz |
| 位深 | 16-bit signed | 16-bit signed |
| 声道 | Mono | Mono |
| 字节序 | Little-endian | Little-endian |
| 帧大小 | 1920 bytes (60ms) | 可变 |

**带宽**:
- 上传: 256 kbps
- 下载: 384 kbps

---

## 2. 客户端 → 服务器

### 2.1 音频数据 (二进制)

```
WebSocket Binary Frame
┌─────────────────────────┐
│   Raw PCM Data (N B)    │
│   16kHz, 16-bit, mono  │
└─────────────────────────┘
```

### 2.2 录音结束 (JSON)

```json
{"type": "audio_end"}
```

### 2.3 状态上报

```json
{"type": "status", "state": "listening" | "recording" | "thinking" | "speaking" | "idle"}
```

---

## 3. 服务器 → 客户端

### 3.1 ASR 结果

```json
{"type": "asr_result", "code": 0, "data": "识别的文字内容"}
```

### 3.2 Bot 回复

```json
{"type": "bot_reply", "code": 0, "data": "AI 回复内容"}
```

### 3.3 TTS 开始

```json
{"type": "status", "state": "speaking"}
```

### 3.4 TTS 音频 (二进制)

```
WebSocket Binary Frame
┌─────────────────────────┐
│   Raw PCM Data (N B)    │
│   24kHz, 16-bit, mono  │
└─────────────────────────┘
```

### 3.5 TTS 结束

```json
{"type": "tts_end", "code": 0, "data": "ok"}
```

### 3.6 错误消息

```json
{"type": "error", "code": 1, "data": "错误描述"}
```

---

## 4. 完整流程

```
[S3]                                    [Server]
  │                                        │
  │  1. 唤醒词检测成功 / 按键按下           │
  │                                        │
  │  2. Raw PCM chunks (16kHz)            │
  │  ─────────────────────►                │
  │                                        │
  │  3. {"type: "audio_end"}              │
  │  ─────────────────────►                │
  │                                        │
  │                    4. ASR 识别         │
  │  ◄─────────────────────                │
  │     {"type: "asr_result", ...}         │
  │                                        │
  │                    5. LLM 处理         │
  │                                        │
  │  6. {"type: "bot_reply", ...}         │
  │  ◄─────────────────────                │
  │                                        │
  │  7. {"type: "status", "speaking"}     │
  │  ◄─────────────────────                │
  │                                        │
  │  8. TTS 音频流 (24kHz)                 │
  │  ◄─────────────────────                │
  │     Raw PCM chunks                     │
  │                                        │
  │  9. {"type: "tts_end"}                │
  │  ◄─────────────────────                │
  │                                        │
  │  10. 恢复唤醒词检测                     │
  ▼                                        ▼
```

---

## 5. S3 固件实现

### 5.1 发送音频

```c
// firmware/s3/main/ws_client.c

// 发送 PCM 数据
ws_send_audio(pcm_data, len);

// 发送结束标记
ws_send_audio_end();

// 状态上报
ws_send_status("recording");
```

### 5.2 接收处理

```c
// 处理 JSON 文本消息
void ws_handle_text(const char *msg) {
    cJSON *root = cJSON_Parse(msg);
    const char *type = cJSON_GetObjectItem(root, "type")->valuestring;

    if (strcmp(type, "asr_result") == 0) {
        // 显示识别结果
    }
    else if (strcmp(type, "tts_end") == 0) {
        // TTS 播放完成，恢复唤醒词
        voice_recorder_resume_wake_word();
    }
    else if (strcmp(type, "status") == 0) {
        // 状态更新
    }
}

// 处理二进制消息 (TTS 音频)
void ws_handle_tts_binary(const uint8_t *data, int len) {
    hal_audio_write(data, len);  // 直接播放 Raw PCM 24kHz
}
```

---

## 6. 唤醒词模式

### 6.1 检测流程

1. `hal_wake_word_init()` 初始化 AFE 模型
2. `voice_recorder_task` 轮询喂入 PCM 数据
3. 检测到唤醒词 "你好小智"
4. 触发回调 `on_wake_word_detected()`
5. 启动录音，启用 VAD 静音检测

### 6.2 VAD 静音检测

- 静音超时: 1500ms (默认)
- RMS 阈值: 100
- 最小语音: 300ms

---

## 7. 服务发现

- UDP 广播端口: 8767
- WebSocket 端口: 8766

---

*版本 2.0 - 适配 watcher-server v2.0 协议*
*详细协议见 COMMUNICATION_PROTOCOL.md*
