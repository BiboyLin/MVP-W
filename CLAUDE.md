# 语音信息流 (完整架构)

## 整体架构

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         语音信息流架构图                                 │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  ┌──────────────┐     ┌──────────────┐     ┌──────────────────────┐   │
│  │   麦克风     │     │   I2S HAL    │     │   唤醒词检测 AFE     │   │
│  │  (I2S DMIC) │────►│ hal_audio.c  │────►│   hal_wake_word.c   │   │
│  └──────────────┘     └──────┬───────┘     └──────────┬───────────┘   │
│                               │                         │               │
│                               │ PCM 16kHz               │ 检测结果      │
│                               ▼                         ▼               │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                    button_voice.c 状态机                        │   │
│  │  ┌─────────────┐    ┌─────────────────┐    ┌───────────────┐  │   │
│  │  │ VOICE_STATE │───►│ VOICE_STATE     │───►│ VOICE_STATE   │  │   │
│  │  │ _IDLE      │    │ _RECORDING      │    │ _IDLE         │  │   │
│  │  └─────────────┘    └────────┬────────┘    └───────────────┘  │   │
│  │         ▲                     │                                   │   │
│  │         │                     │ PCM 直传 WebSocket              │   │
│  │         │                     ▼                                   │   │
│  │         │            ┌─────────────────────┐                    │   │
│  │         │            │ ws_send_audio()     │                    │   │
│  │         │            │ ws_send_audio_end() │                    │   │
│  │         │            └──────────┬──────────┘                    │   │
│  └─────────┴──────────────────────┴───────────────────────────────┘   │
│                                    │                                   │
│                                    ▼ WebSocket                         │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                         Cloud (PC)                               │   │
│  │  ┌─────────┐   ┌─────────┐   ┌─────────┐   ┌──────────────┐  │   │
│  │  │   ASR  │──►│   LLM   │──►│  Agent  │──►│     TTS      │  │   │
│  │  │(Whisper)│   │(Claude) │   │         │   │(火山引擎 24k)│  │   │
│  │  └─────────┘   └─────────┘   └─────────┘   └───────┬────────┘  │   │
│  │                                                    │            │   │
│  │                                                    │ PCM 24kHz  │   │
│  └────────────────────────────────────────────────────┴────────────┘   │
│                                    │                                   │
│                                    ▼ WebSocket 二进制                  │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                    ws_client.c TTS 处理                          │   │
│  │  ┌─────────────────┐    ┌─────────────────┐                    │   │
│  │  │ ws_handle_tts() │──►│ hal_audio_write()│──►│ I2S 喇叭    │   │
│  │  │ (二进制帧处理)   │    │ (24kHz 直传)    │    │             │   │
│  │  └─────────────────┘    └─────────────────┘                    │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

## 语音触发流程

### 唤醒词模式 (已实现)

使用乐鑫 ESP-SR 语音识别框架，实现离线唤醒词检测：

```
1. 系统启动 (app_main.c)
   └── voice_recorder_start()
       ├── hal_wake_word_init() → 加载 AFE 模型
       ├── hal_audio_start() → 启动 I2S 麦克风
       ├── voice_recorder_task 创建 (优先级 5, 60ms 轮询)
       └── hal_wake_word_start() → 启动检测

2. voice_recorder_task 轮询 (60ms 间隔)
   └── hal_audio_read() → PCM 16kHz
       └── hal_wake_word_feed() → 喂入 AFE
           └── 检测任务 (优先级 4) 调用 fetch()

3. 唤醒词检测成功 ("你好小智")
   └── on_wake_word_detected() 回调
       ├── display_update("Listening...", "listening")
       ├── voice_recorder_process_event(VOICE_EVENT_WAKE_WORD)
       └── 状态切换: IDLE → RECORDING

4. VAD 静音检测 (唤醒词触发后启用)
   └── vad_process_frame() 计算 RMS
       └── 静音超时 → voice_recorder_process_event(VOICE_EVENT_TIMEOUT)

5. 录音结束
   └── ws_send_audio_end() → {"type":"audio_end"}
   └── 状态切换: RECORDING → IDLE
   └── hal_wake_word_start() → 恢复唤醒词检测
```

### 按键模式 (已实现)

```
1. 用户短按旋钮按钮
   └── button_callback(true)
       └── voice_recorder_process_event(VOICE_EVENT_BUTTON_PRESS)
           └── start_recording()
               ├── hal_audio_start() → 确保 I2S 运行
               ├── hal_wake_word_stop() → 停止唤醒词检测
               └── display_update("Recording...", "normal")

2. voice_recorder_task 轮询 (60ms 间隔)
   └── hal_audio_read() → PCM 16kHz
       └── ws_send_audio() → WebSocket 二进制帧

3. 用户松开按钮
   └── button_callback(false)
       └── voice_recorder_process_event(VOICE_EVENT_BUTTON_RELEASE)
           └── stop_recording()
               └── ws_send_audio_end() → {"type":"audio_end"}
               └── display_update("Processing...", "analyzing")

4. 等待云端响应
   └── waiting_for_response = true
   └── 收到 Bot 回复 → TTS 播放
```

### TTS 播放流程 (已实现)

```
1. 云端发送 TTS 音频 (WebSocket 二进制帧)
   └── ws_client.c:ws_handle_tts_binary()
       └── 第一帧:
           ├── tts_playing = true
           ├── hal_audio_set_sample_rate(24000) → 切换到 24kHz
           ├── hal_audio_start()
           └── display_update("", "speaking")

2. 播放 PCM 数据
   └── hal_audio_write() → I2S DMA → 喇叭
       └── 持续接收直到 tts_end

3. TTS 播放完成
   └── on_tts_end_handler() → ws_tts_complete()
       ├── hal_audio_stop()
       ├── hal_audio_set_sample_rate(16000) → 切换回 16kHz
       ├── display_update(NULL, "happy")
       └── voice_recorder_resume_wake_word() → 恢复唤醒词检测
```

## 关键模块说明

| 模块 | 文件 | 职责 |
|------|------|------|
| **音频 HAL** | `hal_audio.c` | I2S 麦克风/喇叭驱动，采样率切换 (16kHz/24kHz) |
| **唤醒词检测** | `hal_wake_word.c` | ESP-SR AFE 离线唤醒词检测 ("你好小智") |
| **语音状态机** | `button_voice.c` | 录音状态管理，VAD 静音检测，按键/唤醒词触发 |
| **WebSocket 客户端** | `ws_client.c` | 音频上传/下载，TTS 二进制帧处理 |
| **消息路由** | `ws_router.c` | JSON 消息分发到处理器 |
| **消息处理** | `ws_handlers.c` | 指令处理器 (servo, display, status, tts_end) |

## 音频参数

| 参数 | 录音 (ASR) | 播放 (TTS) |
|------|------------|------------|
| 采样率 | 16kHz | 24kHz |
| 位深 | 16-bit | 16-bit |
| 声道 | mono | mono |
| 编码 | Raw PCM (无压缩) | Raw PCM (无压缩) |
| 帧大小 | 1920 bytes (60ms) | 可变 |
| 带宽 | ~256kbps | ~384kbps |

## 状态转换图

```
                         ┌─────────────────────────────────────────┐
                         │         VOICE_STATE_IDLE                │
                         │  - 等待唤醒词/按键                       │
                         │  - AFE 持续监听                          │
                         │  - voice_recorder_task 轮询             │
                         └──────────────┬──────────────────────────┘
                                        │
          ┌─────────────────────────────┼─────────────────────────────┐
          │                             │                             │
          ▼                             ▼                             │
┌─────────────────────┐     ┌─────────────────────────┐     ┌─────────────────┐
│  VOICE_EVENT_      │     │   VOICE_EVENT_          │     │   WS 收到       │
│  WAKE_WORD         │     │   BUTTON_PRESS          │     │   bot_reply     │
│  (检测到唤醒词)     │     │   (按键触发)            │     │   (TTS 开始)    │
└─────────┬───────────┘     └────────────┬────────────┘     └────────┬────────┘
          │                              │                             │
          │                              ▼                             │
          │                   ┌─────────────────────────┐              │
          │                   │  VOICE_STATE_RECORDING │              │
          │                   │  - 录音中               │              │
          │                   │  - PCM 发送到 WebSocket │              │
          │                   │  - AFE 暂停             │              │
          │                   │  - VAD 启用 (唤醒词模式)│              │
          │                   └────────────┬────────────┘              │
          │                              │                             │
          │     ┌─────────────────────────┼─────────────────────────┐  │
          │     │                         │                         │  │
          │     ▼                         ▼                         ▼  │
          │ ┌───────────────┐   ┌───────────────────┐   ┌────────────┐│
          │ │ VOICE_EVENT_  │   │ VOICE_EVENT_      │   │ 收到       ││
          │ │ TIMEOUT       │   │ BUTTON_RELEASE    │   │ tts_end    ││
          │ │ (VAD 静音超时) │   │ (按键松开)         │   │ 消息       ││
          │ └───────┬───────┘   └─────────┬─────────┘   └──────┬─────┘│
          │         │                       │                    │     │
          └─────────┴───────────────────────┴────────────────────┘     │
                                        │                                │
                                        ▼                                │
                         ┌─────────────────────────────────────────┐      │
                         │         VOICE_STATE_IDLE                │◄─────┘
                         │  - 等待唤醒词/按键                       │
                         │  - AFE 恢复检测                          │
                         └─────────────────────────────────────────┘
```

## VAD 静音检测配置

```c
// firmware/s3/main/button_voice.c
#define VAD_FRAME_MS            60      /* 每帧 60ms */
#define VAD_SILENCE_FRAMES      (CONFIG_VAD_SILENCE_TIMEOUT_MS / VAD_FRAME_MS)
#define VAD_RMS_THRESHOLD       CONFIG_VAD_RMS_THRESHOLD
#define VAD_MIN_SPEECH_FRAMES   (CONFIG_VAD_MIN_SPEECH_MS / VAD_FRAME_MS)
```

默认配置:
- 静音超时: 1500ms (25 帧)
- RMS 阈值: 100
- 最小语音时长: 300ms (5 帧)

# 开发环境

## Watcher 开发环境 (ESP32-S3)

### 软件要求

| 软件 | 版本 | 说明 |
|------|------|------|
| ESP-IDF | v5.2+ | 官方 SDK |
| Python | 3.8+ | 构建工具 |
| CMake | 3.16+ | 构建系统 |
| Ninja | 1.10+ | 构建加速 |

### 安装步骤

```bash
# 1. 克隆 ESP-IDF
git clone -b v5.2.1 --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32s3

# 2. 激活环境 (Windows)
.\env.ps1

# 3. 进入项目目录
cd MVP-W/firmware/s3

# 4. 设置目标芯片
idf.py set-target esp32s3

# 5. 配置项目
idf.py menuconfig
# 需要启用:
# - PSRAM
# - WiFi
# - WebSocket
# - I2S

# 6. 编译
idf.py build

# 7. 烧录 (Windows COM 口)
idf.py -p COM3 flash monitor

# 8. 监视日志
idf.py -p COM3 monitor
```

### 注意事项

- **GPIO 19/20**: 烧录时需断开与 ESP32-MCU 的连接，否则会干扰烧录
- **分区表**: 使用默认 `singleapp` 分区，或自定义 `nvs` + `app` + `spiffs`
- **PSRAM**: 建议启用以支持音频缓冲区

## MCU 开发环境 (ESP32)

```bash
# 1. 进入 MCU 项目
cd MVP-W/firmware/mcu

# 2. 设置目标芯片
idf.py set-target esp32

# 3. 编译
idf.py build

# 4. 烧录 (Windows COM 口)
idf.py -p COM4 flash monitor
```

## Cloud 开发环境 (PC)

```bash
# 进入服务器目录
cd MVP-W/server

# 创建虚拟环境 (推荐)
python -m venv venv
source venv/bin/activate  # Linux/Mac
# 或
.\venv\Scripts\activate  # Windows

# 安装依赖
pip install -r requirements.txt

# 启动 WebSocket 服务器
python main.py --host 0.0.0.0 --port 8766
```

---

# 实施计划

## Phase 1: 基础通信 (10h) ✅

- [x] ESP32 WebSocket 客户端
- [x] 双向文本消息通信
- [x] 连接状态管理
- [x] **可靠性机制**
  - [x] 看门狗 (Watchdog) 配置
  - [x] WebSocket 自动重连
  - [x] 异常日志输出

## Phase 2: 音频通路 (10h) ✅

- [x] ESP32 I2S 麦克风采集
- [ ] ESP32 Opus 编码 (HAL 空壳，待实现)
- [ ] PC 端 Opus 解码 (云端负责)
- [x] TTS 二进制帧处理 (Raw PCM 24kHz 直传)

## Phase 3: 云端 AI 集成 (10h) ⏳ (云端负责)

- [ ] Python ASR 接口 (Whisper 等)
- [ ] Python LLM 接口 (Claude/OpenAI)
- [ ] Agent 逻辑
- [ ] TTS 接口 + AUD1 二进制帧发送

## Phase 4: 控制闭环 (10h) ✅

- [x] 舵机控制指令处理
- [x] TTS 播放通路 (Raw PCM 24kHz)
- [ ] 视频采集 (可选)
- [x] 整体联调

---

# 关键文件参考

| 模块 | 文件 | 说明 |
|------|------|------|
| S3 入口 | `firmware/s3/main/app_main.c` | 系统初始化 |
| WS 客户端 | `firmware/s3/main/ws_client.c` | WebSocket + TTS 二进制帧处理 |
| WS 路由 | `firmware/s3/main/ws_router.c` | 消息分发框架 |
| WS 处理器 | `firmware/s3/main/ws_handlers.c` | 消息处理器实现 |
| 按键触发 | `firmware/s3/main/button_voice.c` | 语音输入状态机 |
| 屏幕显示 | `firmware/s3/main/display_ui.c` | LVGL 显示 + 表情 |
| 表情动画 | `firmware/s3/main/emoji_anim.c` | PNG 动画定时器 |
| 表情加载 | `firmware/s3/main/emoji_png.c` | SPIFFS PNG 加载 |
| 音频 HAL | `firmware/s3/main/hal_audio.c` | I2S 音频抽象层 |
| Opus HAL | `firmware/s3/main/hal_opus.c` | Opus 编解码 (待实现) |
| UART 转发 | `firmware/s3/main/uart_bridge.c` | UART 转发到 MCU |
| 构建配置 | `firmware/s3/sdkconfig.defaults` | **关键！需与 factory_firmware 同步** |
| 通信协议 | `docs/COMMUNICATION_PROTOCOL.md` | 协议详细文档 |
| MCU 入口 | `firmware/mcu/main/main.c` | 舵机控制 |
| UART 处理 | `firmware/mcu/main/uart_handler.c` | 指令解析 |
| 舵机控制 | `firmware/mcu/main/servo_control.c` | PWM 控制 |
| 测试目录 | `firmware/s3/test_host/` | Host 端单元测试 |

---

# 技术参考

- **ESP32-S3 SDK**: `Watcher-Firmware/examples/helloworld/local_components/sensecap-watcher/`
- **MCU 参考**: `WatcherOld/MCU/main/main.c`
- **Himax 驱动**: `Watcher-Firmware/components/sscma_client/`
- **LVGL**: `Watcher-Firmware/components/lvgl/`
- **Opus**: `Watcher-Firmware/components/opus/`

---

# TDD 任务切片

## Phase 1: 纯软件逻辑与基础设施 (Host 测试环境) ✅

这个阶段完全剥离硬件，专注于数据结构的解析和基础算法。

### Task 1.1: TDD 环境初始化 ✅

Host 测试环境已验证可用。

**运行命令**：
```bash
export PATH="/c/Espressif/tools/cmake/3.24.0/bin:/c/ProgramData/mingw64/mingw64/bin:$PATH"
cd firmware/s3/test_host
cmake -B build -G "MinGW Makefiles" -DCMAKE_C_COMPILER=gcc
cmake --build build
ctest --test-dir build -V
```

**结果**: **45+ Tests  0 Failures** (WS Router 21 + WS Handlers 24)

### Task 1.2: MCU 运动计算层 (Math) ✅

实现于 `servo_math.c` 的 `angle_to_duty(int angle)`。

### Task 1.3: MCU UART 指令解析器 (Parser) ✅

实现于 `uart_protocol.c` 的 `parse_axis_cmd()`。

### Task 1.4: Watcher WebSocket 消息路由 (Router) ✅

**实现**: `ws_router.c` + `ws_handlers.c`
- 消息路由框架 (`ws_router_init`, `ws_route_message`)
- 消息处理器 (`on_servo_handler`, `on_display_handler`, `on_status_handler`, `on_capture_handler`)
- 状态映射 (`ws_state_to_emoji`: thinking→analyzing, speaking→speaking, idle→standby, error→sad)
- TTS 二进制帧处理 (`ws_handle_tts_binary`)

**测试覆盖**: 24 个测试用例，覆盖所有处理器和状态映射。

## Phase 2: 核心状态机与协议转换 (Host 测试 + Mock 层) ✅

### Task 2.1: Watcher UART 转发队列 ✅

实现 `uart_bridge.c`，将 JSON 舵机指令转换为 `X:90\r\nY:45\r\n` 格式。

### Task 2.2: Watcher 音频采集状态机 ✅

实现 `button_voice.c` 中的长按录音逻辑。

### Task 2.3: Watcher LVGL 状态映射 ✅

实现 `display_ui.c` 中表情和文字的更新逻辑。

## Phase 3: 硬件抽象层 (HAL) 落地 (真实 ESP32 环境) ✅

### Task 3.1: MCU 舵机 PWM 驱动 ✅

### Task 3.2: Watcher 外设大集成 ✅

**完成状态 (2026-02-28)**：
- ✅ WiFi 连接成功
- ✅ UART 初始化成功 (GPIO 19/20)
- ✅ I2S 音频初始化成功 (GPIO 10-16)
- ✅ LVGL 显示屏初始化成功 (SPD2010 QSPI)
- ✅ PNG 动画显示正常 (花屏问题已修复)
- ✅ WebSocket 客户端连接
- ✅ 消息路由和处理器集成

**关键技术点**：
- `sdkconfig.defaults` 必须与 `factory_firmware` 同步（PSRAM 80MHz, WiFi 使用 SPIRAM）
- 使用 `esp_lcd_spd2010` 组件 (v1.0.0)
- QSPI GPIO: PCLK=7, DATA0=9, DATA1=1, DATA2=14, DATA3=13, CS=45, BL=8

### Task 3.3: Watcher 网络层实现 ✅

## Phase 4: 联调与集成 (双板 + 云端) 🔄 进行中

### Task 4.1: S3 与 MCU 的 UART 通信闭环 ✅

物理连接 GPIO 19/20，验证 Watcher 能否顺畅下发 `X:90\r\nY:90\r\n`。

**完成状态 (2026-03-01)**：双板通信正常，舵机控制验证通过。

### Task 4.2: 整体端云联调

跑通"按键说话 → ASR → LLM 思考 (屏幕反馈) → 舵机动作 + TTS 播报"的完整 MVP 链路。

---

# 待开发功能

| 任务 | 优先级 | 状态 | 说明 |
|------|--------|------|------|
| ~~端到端语音交互测试~~ | ✅ | **已完成** | 完整流程验证通过 (2026-03-01) |
| ~~服务器端 TTS 协议对齐~~ | ✅ | **已完成** | Raw PCM 24kHz 直传 (2026-03-01) |
| ~~S3 ↔ MCU UART 闭环~~ | ✅ | **已完成** | 双板通信验证通过 (2026-03-01) |
| ~~TTS 播放状态切换~~ | ✅ | **已完成** | tts_end 消息触发 (2026-03-01) |
| ~~动画状态映射优化~~ | ✅ | **已完成** | processing/thinking → analyzing (2026-03-01) |
| ~~通讯协议升级 v2.0~~ | ✅ | **已完成** | 统一消息格式 + 原始 PCM (2026-03-01) |
| ~~音频播放优化~~ | ✅ | **已完成** | DMA 缓冲区 + 音量 + 日志优化 (2026-03-02) |
| ~~按键响应优化~~ | ✅ | **已完成** | 长按判定 2s → 0.5s (2026-03-02) |
| ~~物理重启功能~~ | ✅ | **已完成** | 5 次点击触发重启 (2026-03-02) |
| ~~TTS 采样率修复~~ | ✅ | **已完成** | 24kHz 正确播放 (2026-03-03) |
| ~~**唤醒词检测**~~ | ✅ | **已完成** | ESP-SR 离线唤醒 + VAD 自动停止 (2026-03-04) |
| ~~**VAD 静音检测**~~ | ✅ | **已完成** | 录音后自动停止 + TTS 后恢复检测 (2026-03-04) |
| ~~**服务发现**~~ | ✅ | **已完成** | UDP 广播自动发现服务器 IP (2026-03-04) |
| **摄像头集成** | 🟢 低 | 可选 | Himax 拍照 + JPEG + WebSocket 发送 |
| **传感器数据上报** | 🟢 低 | 可选 | CO2/温湿度数据上报 |
| **Opus 编解码** | 🟡 中 | 优化 | 当前 Raw PCM，可优化带宽 |
| **离线模式** | 🟡 中 | 增强 | 设备影子 + 断线重连增强 |
| **Speaking 动画资源** | 🟡 中 | 待替换 | 当前临时使用 listening 动画 |

---

# 可靠性设计

## 看门狗 (Watchdog)

```c
// app_main.c
#include "esp_task_wdt.h"

void app_main() {
    // 初始化看门狗 (30 秒超时)
    esp_task_wdt_init(30, true);
    esp_task_wdt_add(NULL);

    // 主循环中定期喂狗
    while (1) {
        // ... 业务逻辑
        esp_task_wdt_reset();  // 喂狗
    }
}
```

## WebSocket 重连机制

```c
// ws_client.c
static TaskHandle_t reconnect_task_handle = NULL;

void ws_start_reconnect_task() {
    if (reconnect_task_handle == NULL) {
        xTaskCreate(ws_reconnect_task, "ws_reconnect", 4096, NULL, 5, &reconnect_task_handle);
    }
}

void ws_reconnect_task(void *param) {
    while (!ws_is_connected()) {
        ESP_LOGI(TAG, "尝试重新连接 WebSocket...");
        esp_ws_connect();
        vTaskDelay(pdMS_TO_TICKS(5000));  // 5 秒重试
    }
    reconnect_task_handle = NULL;  // 清除句柄，允许下次断线时重新创建
    vTaskDelete(NULL);
}
```

## 异常日志

```c
#include "esp_log.h"
#include "esp_system.h"

void handle_crash() {
    esp_reset_reason_t reason = esp_reset_reason();
    ESP_LOGE(TAG, "系统重启原因：%d", reason);

    // 输出崩溃前的状态
    ESP_LOGI(TAG, "Free heap: %lu", esp_get_free_heap_size());
    ESP_LOGI(TAG, "Min free heap: %lu", esp_get_minimum_free_heap_size());
}
```

---

*文档版本：3.3*
*更新日期：2026-03-05*

## 变更历史

| 版本 | 日期 | 变更内容 |
|------|------|----------|
| 3.3 | 2026-03-05 | **v1.4.0 发布** - 启动动画（arc 进度条）；LVGL 线程安全修复；AFE 优先级修复；唤醒词更新为"你好小智" |
| 3.2 | 2026-03-04 | **v1.3.0 发布** - UDP 服务发现，自动检测服务器 IP |
| 3.1 | 2026-03-04 | **v1.2.0 发布** - 唤醒词检测 + VAD 静音自动停止 + 连续对话支持 |
| 3.0 | 2026-03-03 | **Demo v1.1.0 发布** - 完整可演示版本，物理重启，TTS 采样率修复 |
| 2.6 | 2026-03-01 | **MVP v1.0.2 完成** - 协议 v2.0 升级、全流程打通 |
| 2.5 | 2026-03-01 | **MVP v1.0.1 完成** - TTS 状态切换修复、动画优化、文字显示修复 |
| 2.4 | 2026-03-01 | **MVP v1.0 完成** - 端到端语音交互联调成功 |
| 2.3 | 2026-03-01 | S3 ↔ MCU UART 闭环完成，更新待开发功能清单 |
| 2.2 | 2026-02-28 | 更新通信协议为 AUD1 二进制帧格式；添加待开发功能清单 |
| 2.1 | 2026-02-27 | 添加 PNG 动画花屏修复记录 |
| 2.0 | 2026-02-26 | 重构文档结构，添加 TDD 任务切片 |
