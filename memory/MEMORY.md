# MVP-W 项目记忆

## 当前状态 (2026-03-03)

### MVP v1.1.0 Demo 版本已完成 ✅

完整的端到端语音交互演示版本，可用于产品展示。

#### 核心功能
- **端到端语音交互** - 按键 → ASR → LLM → TTS 完整链路
- **协议 v2.0** - 统一 JSON 消息格式 + Raw PCM 直传
- **PNG 动画显示** - 7 种表情动画
- **舵机控制** - S3 ↔ MCU UART 闭环 + 后台平滑移动
- **物理重启** - 5 次点击旋钮触发重启

#### v1.1.0 新增优化
- I2S DMA 缓冲区增大 6 倍，减少播放卡顿
- 音量优化 (80%)，减少破音
- 采样率切换时清空 DMA，避免切换噪声
- 高频日志禁用，减少 UART 瓶颈
- 按键响应时间 2s → 0.5s
- TTS 正确以 24kHz 播放

### 版本历史

| 版本 | 日期 | 主要变更 |
|------|------|----------|
| **v1.1.0** | 2026-03-03 | Demo 版本 - 完整优化 + 物理重启 |
| v1.0.5 | 2026-03-03 | TTS 采样率修复 |
| v1.0.4 | 2026-03-02 | 物理重启功能 (5次点击) |
| v1.0.3 | 2026-03-02 | 音频播放优化 + 按键响应加快 |
| v1.0.2 | 2026-03-01 | 协议 v2.0 升级、全流程打通 |
| v1.0.1 | 2026-03-01 | TTS 状态切换修复 |
| v1.0.0 | 2026-03-01 | 端到端语音交互联调成功 |

---

## 已修复 Bug 列表

| # | Bug | 状态 | 解决方案 |
|---|-----|------|----------|
| 1 | **文字显示在 PNG 背后** | ✅ 已修复 | 调整 LVGL 对象创建顺序 |
| 2 | **中文显示为方块** | ⏳ 部分修复 | SimSun 16 CJK 字体支持有限 |
| 3 | **动画切换响应慢** | ✅ 已修复 | 定时器复用 + 150ms 间隔 |
| 4 | **TTS 播放后不切换状态** | ✅ 已修复 | tts_end 消息触发 |
| 5 | **Speaking 动画花屏** | ⏳ 临时方案 | 临时使用 listening 动画 |
| 6 | **TTS 播放卡顿** | ✅ 已修复 | DMA 缓冲区增大 + 日志优化 |
| 7 | **播放破音** | ✅ 已修复 | 音量 100→80 |
| 8 | **舵机队列溢出** | ✅ 已修复 | 改为后台平滑任务 |
| 9 | **按键响应慢** | ✅ 已修复 | 长按判定 2s→0.5s |
| 10 | **TTS 采样率错误** | ✅ 已修复 | 移除 bsp_codec_dev_resume() |

---

## 动画状态映射表 (v2.0)

### 协议状态 → 动画映射 (ws_handlers.c)

| 协议状态关键词 | 动画 | 说明 |
|----------------|------|------|
| `processing` | analyzing | AI 正在处理 |
| `thinking` | analyzing | AI 正在思考 |
| `speaking` | speaking | AI 正在说话 |
| `idle` | standby | 空闲状态 |
| `done` | happy | 处理完成 |
| `error` | sad | 错误状态 |

### 代码层 → SPIFFS 动画映射

| emoji_type_t | 代码调用 | SPIFFS 动画 | 帧数 |
|--------------|----------|-------------|------|
| EMOJI_STANDBY | "standby"/"normal"/"idle" | standby | 4 |
| EMOJI_HAPPY | "happy"/"success" | greeting | 3 |
| EMOJI_SAD | "sad"/"error" | detected | 24 |
| EMOJI_LISTENING | "listening" | listening | 24 |
| EMOJI_ANALYZING | "analyzing"/"thinking" | analyzing | 24 |
| EMOJI_SPEAKING | "speaking" | listening (temp) | 24 |

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

### MCU GPIO
| 轴 | GPIO | 说明 |
|----|------|------|
| X (左右) | GPIO 12 | 0°=最左, 180°=最右 |
| Y (上下) | GPIO 15 | 90°=最下, 150°=最上 (限位保护) |

---

## 物理重启功能

- **触发方式**: 连续点击旋钮按钮 **5 次**
- **效果**: 显示 "Rebooting..." → 设备重启
- **代码位置**: `app_main.c:on_button_multi_click_restart()`

---

## ESP-IDF 开发注意事项

### 构建命令 (不要在 Bash 中运行)
```powershell
# 在 ESP-IDF PowerShell 中
cd D:\GithubRep\WatcherProject\MVP-W\firmware\s3
idf.py build
idf.py -p COM3 flash monitor
```

### 常见问题
1. **SPI 驱动头文件缺失** → 包含 `driver/spi_master.h`
2. **LVGL 线程安全** → 所有 LVGL 操作必须在任务中执行
3. **采样率切换** → 调用 `hal_audio_set_sample_rate()` 后自动清空 DMA

---

## ⚠️ 危险操作必须确认

**在执行以下操作前，必须先向用户确认**：
- `git reset`
- `git push --force`
- `git rebase`
- `rm -rf`

---

*更新时间: 2026-03-03*
