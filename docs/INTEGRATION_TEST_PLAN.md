# MVP-W 联调测试计划

> 创建日期：2026-03-01
> 状态：进行中

## 1. 协议对齐状态

### 1.1 音频流协议（已完成 ✅）

| 功能 | 服务器 | 客户端 | 状态 |
|------|--------|--------|------|
| 音频上传 | Raw PCM | Raw PCM | ✅ 已对齐 |
| TTS 音频 | Raw PCM | Raw PCM | ✅ 已对齐 |

### 1.2 文字协议（服务器需适配）

| 功能 | 客户端期望 | 服务器当前 | 需要服务器修改 |
|------|-----------|-----------|---------------|
| 录音结束 | `{"type":"audio_end"}` | `"over"` | ✅ 服务器适配 |
| ASR 结果 | `{"type":"status",...}` | `"result: {text}"` | ✅ 服务器适配 |
| TTS 开始 | 无（直接收二进制） | `"tts:start"` | ✅ 服务器不发 |
| 舵机控制 | `{"type":"servo",...}` | 无 | 🔜 待实现 |
| 显示更新 | `{"type":"display",...}` | 无 | 🔜 待实现 |

---

## 2. 测试用例

### 2.1 连接测试

| # | 测试项 | 预期结果 | 状态 |
|---|--------|---------|------|
| 1.1 | ESP32-S3 连接 WebSocket 服务器 | 连接成功，日志显示 "WebSocket connected" | ⬜ |
| 1.2 | 服务器显示新客户端连接 | 服务器日志显示连接信息 | ⬜ |

### 2.2 音频上传测试

| # | 测试项 | 预期结果 | 状态 |
|---|--------|---------|------|
| 2.1 | 长按按钮开始录音 | 屏幕显示 "Listening..." | ⬜ |
| 2.2 | 录音数据发送到服务器 | 服务器收到 Raw PCM 数据 | ⬜ |
| 2.3 | 松开按钮结束录音 | 发送 `{"type":"audio_end"}` | ⬜ |
| 2.4 | 服务器 ASR 识别 | 返回 JSON 状态 | ⬜ |

### 2.3 TTS 播放测试

| # | 测试项 | 预期结果 | 状态 |
|---|--------|---------|------|
| 3.1 | 服务器返回 TTS 音频 | ESP32 收到 Raw PCM 数据 | ⬜ |
| 3.2 | ESP32 播放 TTS | 喇叭输出语音 | ⬜ |
| 3.3 | 屏幕显示 speaking 动画 | 播放期间显示 speaking | ⬜ |

---

## 3. 执行步骤

### Step 1: 客户端修改 ✅ 已完成

修改 `ws_client.c`：
- ✅ 音频上传：移除 AUD1 帧头，直接发 Raw PCM
- ✅ TTS 接收：移除 AUD1 解析，直接播放 Raw PCM

### Step 2: 服务器修改（待执行）

修改 `watcher-server/src/core/websocket_server.py`：

```python
# 1. 录音结束识别
if message == "over" or message == '{"type":"audio_end"}':
    # 停止 ASR

# 2. ASR 结果返回 JSON 格式
await websocket.send(json.dumps({
    "type": "status",
    "state": "analyzing",
    "text": recognized_text
}))

# 3. TTS 直接发送 Raw PCM（无 tts:start 标记）
await websocket.send(tts_result.audio_data)  # 直接发二进制
```

### Step 3: 联调测试

1. 启动服务器：`python main.py`
2. 烧录 ESP32-S3：`idf.py -p COM3 flash monitor`
3. 执行测试用例 2.1 - 2.3

---

## 4. 注意事项

1. **音频格式**：16-bit PCM, 16kHz, mono
2. **端口**：MVP-W 使用 8766，服务器需确认
3. **IP 地址**：MVP-W 硬编码为 `192.168.31.10`

---

## 5. 联调环境

| 项目 | 配置 |
|------|------|
| 服务器 IP | `192.168.31.10` |
| 服务器端口 | `8766` |
| ESP32-S3 端口 | `COM3` |
| ESP32-MCU 端口 | `COM4` |

---

*文档版本：1.1*
*更新日期：2026-03-01*
