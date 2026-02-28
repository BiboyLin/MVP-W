# MVP-W 通讯协议文档 v1.1

> 文档日期：2026-02-28
> 状态：已确认
> 更新：音频格式从 Opus 改为 PCM（简化实现）

## 1. 系统架构

```
┌─────────────────────┐      WebSocket      ┌─────────────────────┐
│   Watcher (S3)      │◄──────────────────►│   Cloud (PC)        │
│   ESP32-S3          │   ws://IP:8766     │   Python Server     │
├─────────────────────┤                    ├─────────────────────┤
│ - 音频采集/播放     │                    │ - ASR (语音识别)    │
│ - 屏幕显示/表情     │                    │ - LLM (Claude)      │
│ - 按键语音触发      │                    │ - TTS (语音合成)    │
│ - UART → MCU        │                    │ - Agent 调度        │
└─────────┬───────────┘                    └─────────────────────┘
          │ UART 115200
          ▼
┌─────────────────────┐
│  MCU (身体)         │
│  ESP32 舵机控制     │
├─────────────────────┤
│ - 舵机 X/Y 轴       │
│ - LED 状态指示      │
└─────────────────────┘
```

## 2. WebSocket 消息协议

### 2.1 控制指令 (Cloud → Watcher)

| type | 字段 | 说明 |
|------|------|------|
| **servo** | `x`, `y` | 舵机控制 (0-180°) |
| **tts** | 二进制帧 | TTS 音频播放 (AUD1 协议，见 3.2) |
| **display** | `text`, `emoji?`, `size?` | 屏幕显示更新 |
| **status** | `state`, `message` | AI 状态反馈 |
| **capture** | `quality` | 拍照请求 (MVP 暂不实现) |
| **reboot** | - | 重启设备 |

### 2.2 媒体流 (双向)

| 方向 | type | 字段 | 说明 |
|------|------|------|------|
| Watcher → Cloud | **audio** | 二进制帧 | 录音上传 (AUD1 协议) |
| Watcher → Cloud | **audio_end** | JSON | 录音结束标记 |
| Cloud → Watcher | **tts** | 二进制帧 | TTS 播放 (AUD1 协议) |
| Watcher → Cloud | **video** | - | 视频流 (MVP 暂不实现) |
| Watcher → Cloud | **sensor** | - | 传感器数据 (MVP 暂不实现) |

## 3. 详细消息格式

### 3.1 舵机控制

```json
// Cloud → Watcher
{"type": "servo", "x": 90, "y": 45}
```

**字段说明**：
- `x`: X 轴角度 (0-180)，控制左右
- `y`: Y 轴角度 (0-180)，控制上下

**Watcher 内部处理流程**：
```
ws_router → on_servo_handler() → uart_bridge_send_servo(x, y)
                                     ↓
                              UART: "X:90\r\nY:45\r\n"
```

### 3.2 TTS 播放 (Cloud → Watcher)

**二进制帧格式**（与音频上传一致）：
```
┌──────────┬────────────┬─────────────────┐
│  魔数    │   数据长度  │   PCM 数据      │
│  4 字节  │   4 字节   │    N 字节       │
└──────────┴────────────┴─────────────────┘

魔数: "AUD1" (0x41 0x55 0x44 0x31)
长度: uint32_t little-endian
音频: 16-bit signed PCM, 16kHz, mono
```

**Watcher 内部处理流程**：
```
WebSocket 二进制消息 → 检查魔数 AUD1 → 解析长度 → I2S 播放
```

**说明**：
- TTS 音频与上传音频使用相同的二进制帧格式
- **音频格式**：16-bit signed PCM, 16kHz 采样率, 单声道
- 带宽：约 256 kbps（相比 Opus 24kbps 更高，但无需编解码）

### 3.3 屏幕显示

```json
// Cloud → Watcher
{
    "type": "display",
    "text": "你好，我是小微",
    "emoji": "happy",
    "size": 24
}
```

**字段说明**：
- `text`: 显示文本 (必填)
- `emoji`: 表情类型 (可选，默认 `normal`)
- `size`: 字体大小 (可选，默认 24)

**Watcher 内部处理流程**：
```
ws_router → on_display_handler() → display_update(text, emoji, size, NULL)
```

### 3.4 状态反馈

```json
// Cloud → Watcher
{
    "type": "status",
    "state": "thinking",
    "message": "正在思考..."
}
```

**字段说明**：
- `state`: 状态类型
  - `thinking`: AI 思考中
  - `speaking`: AI 正在回复
  - `idle`: 空闲状态
  - `error`: 错误状态
- `message`: 状态描述文本

**Watcher 内部处理流程**：
```
ws_router → on_status_handler() → display_update(message, emoji_from_state, 0, NULL)

emoji 映射:
  thinking  → analyzing
  speaking  → speaking
  idle      → standby
  error     → sad
```

### 3.5 重启设备

```json
// Cloud → Watcher
{"type": "reboot"}
```

**Watcher 内部处理流程**：
```
ws_router → on_reboot_handler() → esp_restart()
```

### 3.6 拍照请求 (MVP 暂不实现)

```json
// Cloud → Watcher
{"type": "capture", "quality": 80}
```

### 3.7 音频二进制帧格式（双向通用）

**用于**：
- Watcher → Cloud：录音上传
- Cloud → Watcher：TTS 播放

**帧结构**：
```
┌──────────┬────────────┬─────────────────┐
│  魔数    │   数据长度  │   PCM 数据      │
│  4 字节  │   4 字节   │    N 字节       │
└──────────┴────────────┴─────────────────┘

魔数: "AUD1" (0x41 0x55 0x44 0x31)
长度: uint32_t little-endian
音频: 16-bit signed PCM, 16kHz, mono
```

**音频参数**：
- 采样率：16000 Hz
- 位深：16-bit signed
- 声道：mono (单声道)
- 帧大小：60ms = 960 samples = 1920 bytes
- 带宽：~256 kbps

**Cloud 端解析示例 (Python)**：
```python
def parse_audio_frame(data: bytes) -> bytes:
    if len(data) < 8:
        raise ValueError("Frame too short")

    magic = data[:4]
    if magic != b'AUD1':
        raise ValueError(f"Invalid magic: {magic}")

    length = struct.unpack('<I', data[4:8])[0]
    pcm_data = data[8:8+length]

    return pcm_data  # 16-bit PCM, 16kHz, mono
```

**Watcher 端解析示例 (C)**：
```c
int parse_audio_frame(const uint8_t *data, int len, uint32_t *out_len) {
    if (len < 8) return -1;

    if (memcmp(data, "AUD1", 4) != 0) {
        return -1;  // Invalid magic
    }

    memcpy(out_len, data + 4, 4);  // Little-endian
    return 8;  // Return header size, data starts at offset 8
}
```

**构建音频帧 (Python)**：
```python
def build_audio_frame(pcm_data: bytes) -> bytes:
    """Build AUD1 frame from PCM data"""
    header = b'AUD1' + struct.pack('<I', len(pcm_data))
    return header + pcm_data
```

### 3.8 录音结束

```json
// Watcher → Cloud
{"type": "audio_end"}
```

## 4. 表情/动画映射

| emoji 值 | 动画类型 | 使用场景 |
|----------|---------|---------|
| `happy` | GREETING | 欢迎状态、正常待机 |
| `sad` | DETECTED | 错误、遗憾 |
| `surprised` | DETECTING | 检测到异常 |
| `angry` | ANALYZING | 警告状态 |
| `normal` | STANDBY | 默认状态 |
| `listening` | LISTENING | 录音中 |
| `analyzing` / `thinking` | ANALYZING | AI 处理中 |
| `speaking` | SPEAKING | TTS 播放中 |
| `standby` | STANDBY | 离线/空闲 |

## 5. 状态流转图

```
┌─────────┐  按下按钮   ┌───────────┐  松开按钮   ┌──────────┐
│ STANDBY │ ─────────► │ LISTENING │ ─────────► │  HAPPY   │
└────┬────┘            └───────────┘            └────┬─────┘
     │                                                   │
     │ WS断开                          WS收到status消息   │
     ▼                                                   ▼
┌──────────┐                                    ┌────────────┐
│ STANDBY  │                                    │ ANALYZING  │
│(屏幕提示)│                                    │ /SPEAKING  │
└──────────┘                                    └────────────┘
```

## 6. UART 协议 (Watcher → MCU)

**格式**：`<axis>:<angle>\r\n`

**示例**：
```
X:90\r\n
Y:45\r\n
```

**说明**：
- `axis`: 轴标识，`X` 或 `Y`
- `angle`: 角度值 (0-180)
- 每条指令以 `\r\n` 结尾

## 7. 待确认事项

- [ ] 拍照功能：MVP 是否需要？
- [ ] 传感器数据：MVP 是否需要？
- [ ] 错误处理：是否需要 `{"type": "error", "code": ..., "message": ...}` 消息？
- [x] ~~TTS 音频格式~~ → 已确定使用二进制帧格式（与上传一致）
- [x] ~~音频编解码~~ → MVP 使用 PCM 直传（无压缩），后续可升级 Opus

---

*文档版本：1.1*
*更新日期：2026-02-28*

## 变更历史

| 版本 | 日期 | 变更内容 |
|------|------|----------|
| 1.1 | 2026-02-28 | 音频格式从 Opus 改为 PCM 直传 |
| 1.0 | 2026-02-28 | 初始版本 |
