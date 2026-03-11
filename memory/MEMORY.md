# MVP-W 项目记忆

## 当前状态 (2026-03-11)

### MVP v1.4.0 发布 ✅

完整的可演示版本，支持唤醒词检测和连续对话。

#### 核心功能
- **端到端语音交互** - 按键/唤醒词 → ASR → LLM → TTS 完整链路
- **唤醒词检测** - ESP-SR 离线唤醒词 "你好小智"
- **VAD 静音检测** - 录音后自动停止
- **连续对话** - TTS 播放后自动恢复唤醒词检测
- **服务发现** - UDP 广播自动发现服务器 IP
- **协议 v2.1** - 统一 JSON 消息格式 + Raw PCM 直传
- **PNG 动画显示** - 7 种表情动画 + 启动动画
- **舵机控制** - S3 ↔ MCU UART 闭环
- **物理重启** - 5 次点击旋钮触发重启

#### v1.4.0 新增功能
- 唤醒词检测 "你好小智"
- VAD 静音自动停止
- UDP 服务发现
- 启动动画 (arc 进度条)
- LVGL 线程安全修复
- AFE 优先级修复

#### 版本历史

| 版本 | 日期 | 主要变更 |
|------|------|----------|
| **v1.4.0** | 2026-03-05 | 唤醒词检测 + VAD + 服务发现 |
| v1.3.0 | 2026-03-04 | UDP 服务发现自动检测服务器 IP |
| v1.2.0 | 2026-03-04 | 唤醒词检测 + VAD 静音自动停止 |
| v1.1.0 | 2026-03-03 | Demo 版本 - 完整优化 + 物理重启 |
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
| 2 | **中文显示为方块** | ✅ 已修复 | 添加 SimSun 16 CJK 字体 |
| 3 | **动画切换响应慢** | ✅ 已修复 | 定时器复用 + 150ms 间隔 |
| 4 | **TTS 播放后不切换状态** | ✅ 已修复 | tts_end 消息触发 |
| 5 | **PNG 白屏** | ✅ 已修复 | LV_IMG_CF_RAW_ALPHA + LV_MEM_CUSTOM |
| 6 | **PNG 花屏** | ✅ 已修复 | sdkconfig.defaults 与 factory_firmware 同步 |
| 7 | **TTS 播放卡顿** | ✅ 已修复 | DMA 缓冲区增大 + 日志优化 |
| 8 | **播放破音** | ✅ 已修复 | 音量 100→80 |
| 9 | **舵机队列溢出** | ✅ 已修复 | 改为后台平滑任务 |
| 10 | **按键响应慢** | ✅ 已修复 | 长按判定 2s→0.5s |
| 11 | **TTS 采样率错误** | ✅ 已修复 | 移除 bsp_codec_dev_resume() |
| 12 | **唤醒词检测后不恢复** | ✅ 已修复 | voice_recorder_resume_wake_word() |

---

## 动画状态映射表

### 协议状态 → 动画映射 (ws_handlers.c)

| 协议状态关键词 | 动画 | 说明 |
|----------------|------|------|
| `processing` | analyzing | AI 正在处理 |
| `thinking` | analyzing | AI 正在思考 |
| `speaking` | speaking | AI 正在说话 |
| `idle` / `standby` | standby | 空闲状态 |
| `listening` | listening | 录音中/唤醒词检测 |
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
| UART (S3→MCU) | TX=19, RX=20 |
| I2S (音频) | MCLK=10, BCLK=11, LRCK=12, DOUT=15, DIN=16 |
| 按钮 | IO_EXPANDER_PIN_3 (通过 I2C PCA9535) |

### 音频配置

| 用途 | 采样率 | 格式 | 说明 |
|------|--------|------|------|
| 录音 (ASR) | 16kHz | 16-bit PCM mono | WebSocket 发送 |
| 播放 (TTS) | 24kHz | 16-bit PCM mono | 火山引擎直供 |
| 切换 | hal_audio_set_sample_rate() | 自动清空 DMA | - |

### MCU GPIO

| 轴 | GPIO | 说明 |
|----|------|------|
| X (左右) | GPIO 12 | 0°=最左, 180°=最右 |
| Y (上下) | GPIO 15 | 90°=最下, 150°=最上 (限位保护) |

---

## 语音触发流程

### 唤醒词模式
```
1. 系统启动 → hal_wake_word_init() → AFE 加载
2. voice_recorder_task 轮询 (60ms)
3. hal_audio_read() → PCM 16kHz
4. hal_wake_word_feed() → 喂入 AFE
5. 检测成功 → on_wake_word_detected()
6. 状态切换: IDLE → RECORDING
7. VAD 静音检测 → 录音自动停止
8. ws_send_audio_end() → audio_end
9. TTS 播放 → tts_end
10. voice_recorder_resume_wake_word() → 恢复检测
```

### 按键模式
```
1. 按键按下 (>0.5s) → start_recording()
2. 录音中 → ws_send_audio()
3. 按键松开 → stop_recording()
4. ws_send_audio_end()
5. 后续同上
```

---

## 服务发现 (UDP)

### 设备侧
- 广播地址: 255.255.255.255:8767
- 消息: `{"type": "discovery", "device": "ESP32-S3"}`

### 服务器侧
- 监听: 0.0.0.0:8767
- 响应: `{"type": "discovery", "ip": "192.168.x.x", "port": 8766}`

---

## ESP-IDF 开发注意事项

### 构建命令 (不要在 Bash 中运行)
```powershell
# 在 ESP-IDF PowerShell 中
cd D:\GithubRep\WatcherProject\MVP-W\firmware\s3
idf.py set-target esp32s3
idf.py build
idf.py -p COM3 flash monitor
```

### 关键配置 (sdkconfig.defaults)
- CONFIG_SPIRAM_SPEED=80MHz
- CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP=y
- CONFIG_ESP32S3_DATA_CACHE_SIZE_32KB
- CONFIG_LV_USE_PNG=y
- CONFIG_LV_MEM_CUSTOM=y

### 常见问题
1. **SPI 驱动头文件缺失** → 包含 `driver/spi_master.h`
2. **LVGL 线程安全** → 所有 LVGL 操作必须在任务中执行
3. **采样率切换** → 调用 `hal_audio_set_sample_rate()` 后自动清空 DMA
4. **PNG 显示白屏** → 使用 LV_IMG_CF_RAW_ALPHA 格式
5. **PNG 花屏** → 同步 sdkconfig.defaults 与 factory_firmware

---

## ⚠️ 危险操作必须确认

**在执行以下操作前，必须先向用户确认**：
- `git reset` (任何模式)
- `git push --force`
- `git rebase`
- `rm -rf`

**教训**: 2026-02-28 执行 `git reset` 导致 15+ 个 commit 丢失

---

*更新时间: 2026-03-11*
