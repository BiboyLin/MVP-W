# MVP-W 联调测试计划

> 创建日期：2026-03-01
> 状态：待执行

## 1. 协议对比分析

### 1.1 服务器端 (watcher-server) 协议

| 方向 | 消息类型 | 格式 | 说明 |
|------|---------|------|------|
| S3 → Server | 音频数据 | **纯二进制 PCM** | 直接发送，无帧头 |
| S3 → Server | 录音结束 | `"over"` | 纯文本 |
| Server → S3 | ASR 结果 | `"result: {text}"` | 纯文本前缀 |
| Server → S3 | TTS 开始 | `"tts:start"` | 纯文本标记 |
| Server → S3 | TTS 音频 | **纯二进制** | 直接发送，无帧头 |
| Server → S3 | 错误 | `"error: {msg}"` | 纯文本前缀 |

### 1.2 客户端 (MVP-W) 协议

| 方向 | 消息类型 | 格式 | 说明 |
|------|---------|------|------|
| S3 → Server | 音频数据 | **AUD1 帧** + PCM | `[AUD1][len][data]` |
| S3 → Server | 录音结束 | `{"type":"audio_end"}` | JSON |
| Server → S3 | ASR 结果 | `{"type":"status",...}` | JSON |
| Server → S3 | TTS 音频 | **AUD1 帧** + PCM | `[AUD1][len][data]` |
| Server → S3 | 舵机控制 | `{"type":"servo","x":90,"y":90}` | JSON |
| Server → S3 | 显示更新 | `{"type":"display",...}` | JSON |

### 1.3 差异汇总

| 功能 | watcher-server | MVP-W | 对齐状态 |
|------|----------------|-------|----------|
| 音频上传格式 | 纯二进制 PCM | AUD1 帧 + PCM | ❌ **不对齐** |
| 录音结束标记 | `"over"` | `{"type":"audio_end"}` | ❌ **不对齐** |
| ASR 结果格式 | `"result: {text}"` | JSON status | ❌ **不对齐** |
| TTS 音频格式 | 纯二进制 | AUD1 帧 + PCM | ❌ **不对齐** |
| TTS 开始标记 | `"tts:start"` | 无 | ❌ **不对齐** |
| 舵机控制 | 无 | JSON | ⚠️ 服务端未实现 |
| 显示控制 | 无 | JSON | ⚠️ 服务端未实现 |

---

## 2. 解决方案

### 方案 A: 修改 MVP-W 客户端（推荐）

修改客户端以适配服务器现有协议，改动最小。

**需要修改的文件**：

| 文件 | 修改内容 |
|------|----------|
| `ws_client.c` | 音频发送：移除 AUD1 帧头，直接发 PCM |
| `ws_client.c` | 音频结束：改为发送 `"over"` |
| `ws_client.c` | TTS 接收：移除 AUD1 帧解析，直接播放 |
| `ws_handlers.c` | 解析 `"result:"` 和 `"tts:start"` 格式 |

### 方案 B: 修改服务器端

修改服务器以适配 MVP-W 协议。

**需要修改的文件**：

| 文件 | 修改内容 |
|------|----------|
| `websocket_server.py` | 音频接收：解析 AUD1 帧 |
| `websocket_server.py` | 录音结束：识别 `{"type":"audio_end"}` |
| `websocket_server.py` | ASR 结果：返回 JSON 格式 |
| `websocket_server.py` | TTS 发送：添加 AUD1 帧头 |
| `websocket_server.py` | 新增：舵机控制、显示更新 |

---

## 3. 测试用例

### 3.1 连接测试

| # | 测试项 | 预期结果 | 状态 |
|---|--------|---------|------|
| 1.1 | ESP32-S3 连接 WebSocket 服务器 | 连接成功，日志显示 "WebSocket connected" | ⬜ |
| 1.2 | 服务器显示新客户端连接 | 服务器日志显示连接信息 | ⬜ |

### 3.2 音频上传测试

| # | 测试项 | 预期结果 | 状态 |
|---|--------|---------|------|
| 2.1 | 长按按钮开始录音 | 屏幕显示 "Listening..." | ⬜ |
| 2.2 | 录音数据发送到服务器 | 服务器收到音频数据 | ⬜ |
| 2.3 | 松开按钮结束录音 | 发送结束标记 | ⬜ |
| 2.4 | 服务器 ASR 识别 | 返回识别文本 | ⬜ |

### 3.3 TTS 播放测试

| # | 测试项 | 预期结果 | 状态 |
|---|--------|---------|------|
| 3.1 | 服务器返回 TTS 音频 | ESP32 收到音频数据 | ⬜ |
| 3.2 | ESP32 播放 TTS | 喇叭输出语音 | ⬜ |
| 3.3 | 屏幕显示 speaking 动画 | 播放期间显示 speaking | ⬜ |

### 3.4 舵机控制测试

| # | 测试项 | 预期结果 | 状态 |
|---|--------|---------|------|
| 4.1 | 服务器发送舵机指令 | `{"type":"servo","x":90,"y":90}` | ⬜ |
| 4.2 | ESP32 转发到 MCU | UART 发送 `X:90\r\nY:90\r\n` | ✅ 已验证 |
| 4.3 | MCU 舵机动作 | 舵机移动到指定角度 | ✅ 已验证 |

### 3.5 端到端测试

| # | 测试项 | 预期结果 | 状态 |
|---|--------|---------|------|
| 5.1 | 完整语音交互 | 按键 → ASR → LLM → TTS → 舵机 | ⬜ |
| 5.2 | 屏幕状态同步 | 各阶段显示对应表情 | ⬜ |

---

## 4. 执行步骤

### Step 1: 协议对齐（选择方案）

**推荐方案 A** - 修改客户端适配服务器：

```c
// ws_client.c - 音频发送（移除 AUD1 帧）
int ws_send_audio(const uint8_t *data, int len) {
    // 直接发送 PCM 数据，不添加帧头
    return esp_websocket_client_send_bin(ws_client, (const char *)data, len, pdMS_TO_TICKS(1000));
}

// 音频结束标记
int ws_send_audio_end(void) {
    return ws_client_send_text("over");  // 改为 "over"
}
```

### Step 2: 修改 TTS 接收

```c
// ws_client.c - 处理文本消息
if (strncmp(msg, "tts:start", 9) == 0) {
    // 准备接收 TTS 音频
    display_update(NULL, "speaking", 0, NULL);
}
else if (strncmp(msg, "result:", 7) == 0) {
    // ASR 结果
    display_update(msg + 7, "analyzing", 0, NULL);
}
```

### Step 3: 修改 TTS 二进制处理

```c
// ws_client.c - TTS 音频直接播放（无 AUD1 帧）
void ws_handle_tts_binary(const uint8_t *data, int len) {
    // 直接播放 PCM，无帧头解析
    hal_audio_start();
    hal_audio_write(data, len);
}
```

### Step 4: 联调测试

1. 启动服务器：`python main.py`
2. 烧录 ESP32-S3：`idf.py -p COM3 flash monitor`
3. 执行测试用例 3.1 - 3.5

---

## 5. 注意事项

1. **音频格式**：确保双方都是 16-bit PCM, 16kHz, mono
2. **端口**：服务器默认 8765，MVP-W 配置 8766，需要统一
3. **IP 地址**：MVP-W 硬编码为 `192.168.31.10`，确认服务器在此 IP

---

## 6. 联调环境

| 项目 | 配置 |
|------|------|
| 服务器 IP | `192.168.31.10` |
| 服务器端口 | `8765`（需确认） |
| ESP32-S3 端口 | `COM3` |
| ESP32-MCU 端口 | `COM4` |

---

*文档版本：1.0*
*创建日期：2026-03-01*
