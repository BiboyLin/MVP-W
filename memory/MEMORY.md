# MVP-W 项目记忆

## 当前状态 (2026-03-01)

### MVP v1.0.1 已完成 ✅
- 端到端语音交互（按键 → ASR → LLM → TTS）
- Raw PCM 24kHz TTS 播放
- S3 ↔ MCU UART 闭环
- PNG 动画显示
- **新增**: TTS 2秒超时自动切换到 happy 状态
- **新增**: 动画状态映射优化（7种表情 + 150ms 切换）
- **新增**: 文字居中显示 + 30字符截断

### 已修复 Bug 列表

| # | Bug | 状态 | 解决方案 |
|---|-----|------|----------|
| 1 | **文字显示在 PNG 背后** | ✅ 已修复 | 调整 LVGL 对象创建顺序 |
| 2 | **中文显示为方块** | ⏳ 部分修复 | SimSun 16 CJK 字体支持有限，需自定义字体 |
| 3 | **动画切换响应慢** | ✅ 已修复 | 定时器复用 + 150ms 间隔 |
| 4 | **TTS 播放后不切换状态** | ✅ 已修复 | 2秒超时自动检测 |
| 5 | **Speaking 动画花屏** | ⏳ 临时方案 | 临时使用 listening 动画（PNG 尺寸不匹配）|

---

## 动画状态映射表 (v2.0)

### 协议状态 → 动画映射 (ws_handlers.c)

| 协议状态关键词 | 动画 | 说明 |
|----------------|------|------|
| `processing` | analyzing | AI 正在处理 |
| `thinking` | analyzing | AI 正在思考 |
| `[thinking]` | analyzing | AI 思考中 (带标签) |
| `speaking` | speaking | AI 正在说话 |
| `idle` | standby | 空闲状态 |
| `done` | happy | 处理完成 |
| `error` | sad | 错误状态 |
| `舵机动画` | NULL (不变) | 舵机动画状态 |

### 代码层 → SPIFFS 动画映射

| emoji_type_t | 值 | 代码调用 | SPIFFS 动画 | 帧数 |
|--------------|---|----------|-------------|------|
| EMOJI_STANDBY | 0 | `display_update(..., "standby"/"normal"/"idle", ...)` | standby | 4 |
| EMOJI_HAPPY | 1 | `display_update(..., "happy"/"success", ...)` | greeting | 3 |
| EMOJI_SAD | 2 | `display_update(..., "sad"/"error", ...)` | detected | 24 |
| EMOJI_SURPRISED | 3 | `display_update(..., "surprised", ...)` | detecting | 24 |
| EMOJI_ANGRY | 4 | `display_update(..., "angry", ...)` | analyzing | 24 |
| EMOJI_LISTENING | 5 | `display_update(..., "listening", ...)` | listening | 24 |
| EMOJI_ANALYZING | 6 | `display_update(..., "analyzing"/"thinking", ...)` | analyzing | 24 |
| EMOJI_SPEAKING | 7 | `display_update(..., "speaking", ...)` | listening (temp) | 24 |

> **注意**: Speaking 动画临时使用 listening，因为 speaking PNG 文件是 240x240（其他是 412x412）

### 实际使用场景 (Protocol v2.0)

| 场景 | 消息来源 | 消息类型 | emoji 参数 | 动画 |
|------|----------|----------|------------|------|
| 启动完成 | `app_main.c` | - | "happy" | greeting |
| WebSocket 连接成功 | `ws_client.c` | - | "happy" | greeting |
| WebSocket 断开 | `ws_client.c` | - | "standby" | standby |
| 按键按下（开始录音） | `app_main.c` | - | "listening" | listening |
| 按键松开（处理中） | `app_main.c` | - | "analyzing" | analyzing |
| ASR 识别结果 | 服务器 | `asr_result` | "analyzing" | analyzing |
| Bot 回复 | 服务器 | `bot_reply` | 不变 | - |
| 状态更新 (processing) | 服务器 | `status` | "analyzing" | analyzing |
| 状态更新 (thinking) | 服务器 | `status` | "analyzing" | analyzing |
| 状态更新 (speaking) | 服务器 | `status` | "speaking" | listening |
| TTS 播放 | 服务器 | 二进制 PCM | "speaking" | listening |
| TTS 完成 | 服务器 | `tts_end` | "happy" | greeting |
| 错误 | 服务器 | `error` | "sad" | detected |

### 关键文件

| 文件 | 作用 |
|------|------|
| `display_ui.h` | emoji_type_t 枚举定义 |
| `display_ui.c` | display_update() 实现 |
| `hal_display.c:25-42` | map_emoji_type() 映射函数 |
| `emoji_anim.c` | 动画定时器 |
| `emoji_png.c` | SPIFFS PNG 加载 |
| `spiffs/*.png` | 动画帧图片 |

---

## ESP-IDF 开发常见错误

### 1. SPI 驱动头文件缺失
**错误**:
```
error: unknown type name 'spi_bus_config_t'
```

**解决**: 包含 `driver/spi_master.h`

### 2. ESP_RETURN_ON_ERROR 未定义
**解决**: 包含 `esp_check.h`

### 3. 分区表配置
创建 `partitions.csv` 并配置:
```
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"
```

### 4. LVGL 线程安全
**错误**: 在 ISR 中调用 `display_update()` 导致崩溃

**解决**: 所有 LVGL 操作必须在任务中执行

---

## 硬件配置

### Watcher ESP32-S3 GPIO
| 功能 | GPIO |
|------|------|
| LCD QSPI | PCLK=7, DATA0=9, DATA1=1, DATA2=14, DATA3=13, CS=45, BL=8 |
| UART | TX=19, RX=20 |
| I2S | MCLK=10, BCLK=11, LRCK=12, DOUT=15, DIN=16 |
| 按钮 | IO_EXPANDER_PIN_3 (通过 I2C PCA9535) |

### 音频配置
- 录音: 16kHz, 16-bit, mono (ASR)
- 播放: 24kHz, 16-bit, mono (火山引擎 TTS)
- 动态切换: `hal_audio_set_sample_rate()`

---

## ⚠️ 危险操作必须确认

**在执行以下操作前，必须先向用户确认**：
- `git reset`
- `git push --force`
- `git rebase`
- `rm -rf`

---

*更新时间: 2026-03-01*
