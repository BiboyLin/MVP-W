# MVP-W 项目记忆

## 当前状态 (2026-03-01)

### MVP v1.0 已完成 ✅
- 端到端语音交互（按键 → ASR → LLM → TTS）
- Raw PCM 24kHz TTS 播放
- S3 ↔ MCU UART 闭环
- PNG 动画显示

### 已知 Bug 列表

| # | Bug | 优先级 | 说明 |
|---|-----|--------|------|
| 1 | **文字显示在 PNG 背后** | 🔴 高 | 文字被动画覆盖，需要调整 Z-order 或布局 |
| 2 | **中文显示为方块** | 🔴 高 | 缺少中文字体，需要添加中文字体文件 |
| 3 | **动画切换响应慢** | 🟡 中 | 状态切换时有延迟，可能需要优化动画定时器 |

---

## 动画状态映射表

### 代码层 → SPIFFS 动画映射

| emoji_type_t | 值 | 代码调用 | SPIFFS 动画 | 帧数 |
|--------------|---|----------|-------------|------|
| EMOJI_NORMAL | 0 | `display_update(..., "normal", ...)` | standby | 4 |
| EMOJI_HAPPY | 1 | `display_update(..., "happy", ...)` | greeting | 3 |
| EMOJI_SAD | 2 | `display_update(..., "sad", ...)` | detected | ? |
| EMOJI_SURPRISED | 3 | `display_update(..., "surprised", ...)` | detecting | ? |
| EMOJI_ANGRY | 4 | `display_update(..., "angry", ...)` | analyzing | ? |
| EMOJI_LISTENING | 5 | (未使用) | listening | ? |
| EMOJI_ANALYZING | 6 | `display_update(..., "analyzing", ...)` | analyzing | ? |
| EMOJI_SPEAKING | 7 | (TTS 时自动) | speaking | ? |
| EMOJI_STANDBY | 8 | (待机) | standby | 4 |

### 实际使用场景

| 场景 | 调用位置 | emoji 参数 | 动画 |
|------|----------|------------|------|
| 启动完成 | `app_main.c:187` | "happy" | greeting |
| WebSocket 连接成功 | `ws_client.c:32` | "happy" | greeting |
| WebSocket 断开 | `ws_client.c:39` | "standby" | standby |
| 开始录音 | `button_voice.c:199` | "normal" | standby |
| 结束录音 | `button_voice.c:203` | "thinking" | analyzing |
| ASR 结果 | `ws_client.c:56` | "analyzing" | analyzing |
| TTS 播放 | `ws_client.c:223` | "speaking" | speaking |
| TTS 完成 | `ws_client.c:248` | "happy" | greeting |
| 错误 | `ws_client.c:66` | "sad" | detected |

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
