# 语音流协议规范

> **版本**: 1.2
> **日期**: 2026-03-01
> **范围**: 语音流（适配 watcher-server）

---

## 1. 音频格式

| 参数 | 值 |
|------|-----|
| 格式 | PCM (Raw Binary) |
| 采样率 | 16kHz |
| 位深 | 16-bit |
| 声道 | Mono |
| 字节序 | Little-endian |

---

## 2. 客户端 → 服务器

### 2.1 音频数据

```
WebSocket Binary Frame
┌─────────────────────────┐
│   Raw PCM Data (N B)    │
└─────────────────────────┘
```

### 2.2 录音结束

```
WebSocket Text Frame
┌─────────────────────────┐
│        "over"           │
└─────────────────────────┘
```

---

## 3. 服务器 → 客户端

### 3.1 ASR 结果

```
WebSocket Text Frame
┌─────────────────────────────────┐
│   "result: 识别的文字内容"       │
└─────────────────────────────────┘
```

### 3.2 TTS 开始

```
WebSocket Text Frame
┌─────────────────────────┐
│     "tts:start"         │
└─────────────────────────┘
```

### 3.3 TTS 音频

```
WebSocket Binary Frame
┌─────────────────────────┐
│   Raw PCM Data (N B)    │
└─────────────────────────┘
```

### 3.4 错误消息

```
WebSocket Text Frame
┌─────────────────────────────────┐
│   "error: 错误描述"              │
└─────────────────────────────────┘
```

---

## 4. 完整流程

```
[S3]                                    [Server]
  │                                        │
  │  1. 长按按钮 → Raw PCM chunks          │
  │  ─────────────────────►                │
  │                                        │
  │  2. 松开按钮 → "over"                  │
  │  ─────────────────────►                │
  │                                        │
  │                    3. ASR 识别          │
  │  ◄─────────────────────                │
  │     "result: 你好"                     │
  │                                        │
  │                    4. LLM 处理          │
  │                                        │
  │  5. TTS 准备                           │
  │  ◄─────────────────────                │
  │     "tts:start"                        │
  │                                        │
  │  6. TTS 音频流                          │
  │  ◄─────────────────────                │
  │     Raw PCM chunks                     │
  │                                        │
  ▼                                        ▼
```

---

## 5. 客户端实现 (ws_client.c)

```c
// 录音结束
int ws_send_audio_end(void) {
    return ws_client_send_text("over");
}

// 处理文本消息
if (strncmp(msg, "result:", 7) == 0) {
    display_update(msg + 7, "analyzing", 0, NULL);
}
else if (strcmp(msg, "tts:start") == 0) {
    display_update(NULL, "speaking", 0, NULL);
    hal_audio_start();
}
else if (strncmp(msg, "error:", 6) == 0) {
    display_update("Error", "sad", 0, NULL);
}

// 处理二进制消息 (TTS 音频)
void ws_handle_tts_binary(const uint8_t *data, int len) {
    hal_audio_write(data, len);  // 直接播放 Raw PCM
}
```

---

## 6. 服务器配置

| 配置项 | 值 |
|--------|-----|
| 端口 | **8766** (需修改 .env) |
| IP | 192.168.31.10 |

---

*版本 1.2 - 适配 watcher-server 协议*
