# 语音流协议规范

> **版本**: 1.1
> **日期**: 2026-03-01
> **范围**: 仅语音流音频数据格式

---

## 1. 音频格式

| 参数 | 值 |
|------|-----|
| 格式 | PCM (Raw Binary) |
| 采样率 | 16kHz |
| 位深 | 16-bit |
| 声道 | Mono |
| 字节序 | Little-endian |

**带宽估算**: 16000 × 2 × 1 = 32KB/s ≈ 256kbps

---

## 2. 音频上传 (S3 → Server)

### 2.1 音频数据 - Raw PCM

```
WebSocket Binary Frame
┌─────────────────────────┐
│   Raw PCM Data (N B)    │  直接发送，无帧头
└─────────────────────────┘
```

**客户端实现**:
```c
// ws_client.c
int ws_send_audio(const uint8_t *pcm_data, int len) {
    // 直接发送原始 PCM，无 AUD1 帧头
    return esp_websocket_client_send_bin(ws_client, (const char *)pcm_data, len, portMAX_DELAY);
}
```

### 2.2 录音结束标记 - 保持原协议

```
WebSocket Text Frame (JSON)
┌─────────────────────────────────────┐
│  {"type":"audio_end"}               │  保持原 JSON 格式
└─────────────────────────────────────┘
```

**客户端实现** (保持不变):
```c
// ws_client.c
int ws_send_audio_end(void) {
    const char *msg = "{\"type\":\"audio_end\"}";
    return esp_websocket_client_send_text(ws_client, msg, strlen(msg), portMAX_DELAY);
}
```

---

## 3. TTS 下载 (Server → S3)

### 3.1 TTS 音频数据 - Raw PCM

```
WebSocket Binary Frame
┌─────────────────────────┐
│   Raw PCM Data (N B)    │  直接播放，无帧头
└─────────────────────────┘
```

**客户端实现**:
```c
// ws_client.c - 处理二进制消息
void ws_handle_tts_binary(const uint8_t *data, int len) {
    // 直接播放 PCM，无帧头解析
    hal_audio_write(data, len);
}
```

### 3.2 TTS 结束 - 保持原协议

服务器发送状态更新（JSON）：
```json
{"type": "status", "state": "idle", "message": "播放完成"}
```

---

## 4. ASR 结果 - 保持原协议

```
WebSocket Text Frame (JSON)
┌─────────────────────────────────────────────┐
│  {"type":"status","state":"...","text":"识别内容"} │
└─────────────────────────────────────────────┘
```

---

## 5. 完整流程

```
┌──────────────────────────────────────────────────────────────────┐
│                        语音交互流程                               │
├──────────────────────────────────────────────────────────────────┤
│                                                                  │
│  [S3]                              [Server]                      │
│    │                                    │                        │
│    │  1. 用户长按按钮                    │                        │
│    │  ─────────────────────►            │                        │
│    │     Binary: Raw PCM chunks         │  ← 只有这里改          │
│    │                                    │                        │
│    │  2. 用户松开按钮                    │                        │
│    │  ─────────────────────►            │                        │
│    │     JSON: {"type":"audio_end"}     │  ← 保持原协议          │
│    │                                    │                        │
│    │                    3. ASR + LLM     │                        │
│    │  ◄─────────────────────            │                        │
│    │     JSON: {"type":"status",...}    │  ← 保持原协议          │
│    │                                    │                        │
│    │  4. TTS 音频流                      │                        │
│    │  ◄─────────────────────            │                        │
│    │     Binary: Raw PCM chunks         │  ← 只有这里改          │
│    │                                    │                        │
│    │  5. 播放完成                       │                        │
│    │  ◄─────────────────────            │                        │
│    │     JSON: {"type":"status","state":"idle"}                  │
│    │                                    │                        │
│    ▼                                    ▼                        │
└──────────────────────────────────────────────────────────────────┘
```

---

## 6. 协议变更摘要

### ✅ 需要修改（仅音频数据）

| 功能 | 原协议 | 新协议 |
|------|--------|--------|
| 音频上传 | AUD1 帧 + PCM | **Raw PCM** |
| TTS 音频 | AUD1 帧 + PCM | **Raw PCM** |

### ❌ 保持不变（所有 JSON 协议）

| 消息类型 | 方向 | 格式 |
|----------|------|------|
| 录音结束 | S3 → Server | `{"type":"audio_end"}` |
| ASR 结果 | Server → S3 | `{"type":"status",...}` |
| 舵机控制 | Server → S3 | `{"type":"servo","x":90,"y":90}` |
| 显示更新 | Server → S3 | `{"type":"display","text":"...","emoji":"happy"}` |
| 状态更新 | Server → S3 | `{"type":"status","state":"idle",...}` |
| 拍照请求 | Server → S3 | `{"type":"capture","quality":80}` |

---

## 7. 客户端修改清单

| 文件 | 修改内容 |
|------|----------|
| `ws_client.c` | 音频上传：移除 AUD1 帧头，直接发 PCM |
| `ws_client.c` | TTS 接收：移除 AUD1 解析，直接播放 PCM |

---

## 8. 服务器端修改清单

| 文件 | 修改内容 |
|------|----------|
| `websocket_server.py` | 音频接收：直接处理 Raw PCM |
| `websocket_server.py` | 录音结束：识别 `{"type":"audio_end"}` |
| `websocket_server.py` | ASR 结果：返回 JSON 格式 |
| `websocket_server.py` | TTS 发送：直接发 Raw PCM |

---

*确认后开始修改。*
