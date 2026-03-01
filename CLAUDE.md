# CLAUDE.md - MVP-W

# 角色设定
你是一个顶级的嵌入式 C 语言专家和坚定的 TDD（测试驱动开发）原教旨主义者。当前我们正在开发 MVP-W 项目的嵌入式固件部分（包含 ESP32-S3 和 ESP32 两个子工程），基于 ESP-IDF v5.2+ 框架。云端 Python 代码由其他团队负责，我们只关注设备端逻辑。

# 核心开发哲学：软硬解耦
嵌入式 TDD 的最大痛点是硬件依赖。为了实现不间断的自动化测试，你必须严格遵守"硬件抽象层 (HAL)"原则：
1. **禁止在业务逻辑中直接调用 ESP-IDF 的硬件驱动 API**（如 `uart_write_bytes`, `i2s_channel_read` 等）。
2. 所有的硬件操作必须封装成接口（例如 `hal_uart_send()`, `hal_audio_read()`）。
3. 在编写业务逻辑（如 JSON 解析、协议转换、队列管理）时，完全依赖这些抽象接口。

# TDD 纪律 (红 - 绿 - 重构)
1. **编写测试（红）**：在 `test/` 目录下使用 Unity 框架编写测试用例。在这个阶段，你必须 Mock（模拟）掉所有的 HAL 层接口和 FreeRTOS 依赖。
2. **宿主机运行测试**：必须配置 CMake 使测试能够在 Host（Linux/macOS/Windows）环境下编译运行，而不是烧录到真实的 ESP32 上。
   - 运行命令：`idf.py --preview set-target linux && idf.py build && ./build/MVP-W_test`（根据实际配置调整宿主机编译命令）。
3. **实现功能（绿）**：用最简洁的 C 代码使测试通过。
4. **验证与修复**：如果测试失败，自主阅读报错信息并修复，连续重试不超过 5 次。如果发生段错误 (Segfault) 或内存泄漏，立即停止并检查指针和内存分配（使用 cJSON 时极其容易发生）。
5. **重构**：消除魔术数字，优化内存占用，确保符合 MISRA C 基础规范。

# 约束与红线
- **不要修改 PRD 中定义好的任何协议结构**
- **所有的动态内存分配 (`malloc`, `cJSON_Parse`) 必须有对应的释放逻辑**，并在 Unity 测试中通过自定义的内存分配器跟踪内存泄漏
- **遇到无法在 Host 端 Mock 的底层强耦合逻辑时，立即停止执行并向我询问，严禁强行硬编码**
- **安全红线**：
  - MVP 阶段 WiFi 凭证允许硬编码（`wifi_config.h`），但 API Keys 严禁硬编码
  - MVP 阶段使用明文 WebSocket (ws://)，生产版本必须升级为 WSS
  - 生产版本严禁硬编码 WiFi 密码（改用 NVS 存储）
  - 设备离线时需实现 Device Shadow（设备影子）同步状态
  - 必须实现 Last Will and Testament (LWT) 检测意外断连
  - 考虑低功耗场景时使用 Deep Sleep 循环

---

# 项目概述

MVP-W (Minimum Viable Product - Watcher) 是一个边缘设备 + 云端大脑的最小可行版本。

```
┌─────────────────────┐      ┌─────────────────────┐
│      Watcher        │      │    Cloud (PC)      │
│   (ESP32-S3)        │◄────►│    (WebSocket)     │
├─────────────────────┤      ├─────────────────────┤
│ - 音视频采集        │      │ - ASR 语音识别      │
│ - 屏幕显示/表情     │      │ - LLM 分析          │
│ - 按键语音触发      │      │ - TTS 语音合成      │
│ - WiFi/WebSocket    │      │ - Agent 意图理解    │
│ - UART → MCU        │      └─────────────────────┘
└─────────┬───────────┘
          │ UART 115200
          ▼
┌─────────────────────┐
│  MCU (身体)         │
│  (ESP32 舵机控制)   │
├─────────────────────┤
│ - 舵机 X/Y 轴      │
│ - LED 状态指示      │
└─────────────────────┘
```

## 功能边界

| 功能 | 位置 | 说明 |
|------|------|------|
| **语音采集** | Watcher | I2S 麦克风 → Opus 编码 → WebSocket |
| **语音播放** | Watcher | WebSocket ← Opus 解码 → I2S 喇叭 |
| **视频采集** | Watcher | Himax 摄像头 → JPEG → WebSocket |
| **屏幕显示** | Watcher | LVGL UI + 表情显示 |
| **按键触发** | Watcher | 长按开始/结束语音输入 |
| **WiFi/通信** | Watcher | WiFi 连接 + WebSocket 客户端 |
| **运动控制** | MCU | GPIO PWM → 舵机云台 |
| **LLM/Agent** | Cloud | Claude API / 本地 LLM |
| **ASR/STT** | Cloud | 语音转文字 |
| **TTS** | Cloud | 文字转语音 |

## 项目结构

```
MVP-W/
├── PRD.md                   # 项目需求文档
├── CLAUDE.md                # 本文件
├── docs/
│   └── COMMUNICATION_PROTOCOL.md  # 通信协议详细文档
├── firmware/
│   ├── s3/                  # ESP32-S3 固件 (主控)
│   │   ├── CMakeLists.txt
│   │   ├── sdkconfig.defaults     # 构建配置 (关键！需与 factory_firmware 同步)
│   │   ├── spiffs/                # PNG 动画资源 (24 帧)
│   │   └── main/
│   │       ├── app_main.c           # 入口 + 系统初始化
│   │       ├── ws_client.c/.h       # WebSocket 客户端 + TTS 二进制帧处理
│   │       ├── ws_router.c/.h       # 消息路由框架
│   │       ├── ws_handlers.c/.h     # 消息处理器实现
│   │       ├── button_voice.c       # 按键语音触发状态机
│   │       ├── display_ui.c         # LVGL 屏幕显示 + 表情动画
│   │       ├── hal_audio.c/.h       # 音频 HAL (I2S)
│   │       ├── hal_display.c/.h     # 显示 HAL (LVGL)
│   │       ├── hal_opus.c/.h        # Opus 编解码 HAL (待实现)
│   │       ├── hal_uart.c/.h        # UART HAL
│   │       ├── uart_bridge.c        # UART 转发到 MCU
│   │       ├── emoji_png.c          # SPIFFS PNG 加载
│   │       ├── emoji_anim.c         # 动画定时器系统
│   │       └── camera_capture.c     # Himax 拍照 (待实现)
│   ├── mcu/                 # ESP32-MCU 固件 (身体)
│   │   ├── CMakeLists.txt
│   │   └── main/
│   │       ├── main.c                 # 入口 + 核心逻辑
│   │       ├── uart_handler.c         # UART 指令处理
│   │       └── servo_control.c        # 舵机控制
│   └── test_host/           # Host 端单元测试 (Unity 框架)
│       ├── CMakeLists.txt
│       ├── test_ws_router.c
│       ├── test_ws_handlers.c
│       ├── test_uart_bridge.c
│       ├── test_button_voice.c
│       ├── test_display_ui.c
│       └── mock_dependencies.h/.c
└── server/                  # PC 云端 (其他团队负责)
    ├── websocket_server.py  # WebSocket 服务
    └── ai/
        ├── agent.py         # AI 逻辑
        ├── asr.py           # 语音识别
        └── tts.py           # 语音合成
```

---

# 硬件架构

## Watcher GPIO 分配 (ESP32-S3)

> **参考**: `Watcher-Firmware/examples/helloworld/local_components/sensecap-watcher/include/sensecap-watcher.h`

| GPIO | 功能 | 说明 |
|------|------|------|
| GPIO 10 | I2S MCLK | 音频时钟 |
| GPIO 11 | I2S BCLK | 位时钟 |
| GPIO 12 | I2S LRCK | 左右声道 |
| GPIO 15 | I2S DOUT | 喇叭输出 |
| GPIO 16 | I2S DIN | 麦克风输入 |
| GPIO 4,5,6 | LCD SPI | 显示器 (412x412) |
| GPIO 21 | Himax CS | 摄像头 |
| GPIO 38,39 | Touch I2C | 触摸屏 |
| GPIO 41,42 | 旋钮 Encoder | 按钮输入 (语音触发) |
| GPIO 19 | UART0 TX | → ESP32-MCU RX (烧录时需断开) |
| GPIO 20 | UART0 RX | ← ESP32-MCU TX (烧录时需断开) |

## MCU GPIO 分配 (ESP32)

> **参考**: `WatcherOld/MCU/main/main.c`

| GPIO | 功能 | 说明 |
|------|------|------|
| GPIO 17 | UART TX | 发送响应 → S3 RX |
| GPIO 16 | UART RX | 接收指令 ← S3 TX |
| GPIO 12 | 舵机 X 轴 | PWM 输出 (左右 0-180°) |
| GPIO 13 | 舵机 Y 轴 | PWM 输出 (上下 0-180°) |
| GPIO 2 | LED | 状态指示灯 |

## UART 通信连接

```
ESP32-S3 (主控)          ESP32-MCU (身体)
─────────────────       ─────────────────
GPIO 19 (TX)  ─────────► GPIO 16 (RX)
GPIO 20 (RX)  ◄───────── GPIO 17 (TX)
GND         ───────────► GND
```

- **波特率**: 115200
- **数据位**: 8
- **停止位**: 1
- **校验位**: None

---

# 通信协议

> **详细协议文档**: `docs/COMMUNICATION_PROTOCOL.md`

## WebSocket 消息格式 (Cloud ↔ Watcher)

### 文本消息 (JSON)

所有控制指令和状态消息使用 JSON 文本帧格式。

#### 控制指令 (Cloud → Watcher)

```json
// 舵机控制
{
    "type": "servo",
    "x": 90,
    "y": 45
}

// 文字显示 + 表情 (emoji 和 size 均为可选字段)
{
    "type": "display",
    "text": "你好",
    "emoji": "happy",   // 可选: happy/sad/surprised/angry/normal
    "size": 24          // 可选: 字体大小 (默认 24)
}

// 拍照请求
{
    "type": "capture",
    "quality": 80
}

// 状态反馈 (AI 思考中/回复中)
{
    "type": "status",
    "state": "thinking",    // 或 "speaking", "idle", "error"
    "message": "正在思考..."
}

// 重启指令
{
    "type": "reboot"
}
```

#### 媒体流 (Watcher → Cloud)

```json
// 音频结束标记
{
    "type": "audio_end"
}

// 视频流 (JPEG)
{
    "type": "video",
    "format": "jpeg",
    "width": 412,
    "height": 412,
    "data": "<base64 encoded jpeg>"
}

// 传感器数据
{
    "type": "sensor",
    "co2": 400,
    "temperature": 25.5,
    "humidity": 60
}
```

### 二进制帧 (Binary Frame)

音频数据（上传和下载）统一使用二进制帧格式，效率更高。

#### AUD1 帧格式

```
┌──────────────┬──────────────┬─────────────────────┐
│  Magic (4B)  │ Length (4B)  │   Opus Data (N B)   │
│   "AUD1"     │  uint32_t LE │   编码音频数据       │
└──────────────┴──────────────┴─────────────────────┘
     [0-3]           [4-7]            [8-N]
```

- **Magic**: 4 字节 ASCII "AUD1"
- **Length**: 4 字节 uint32_t 小端序，表示 Opus 数据长度
- **Opus Data**: 实际的 Opus 编码音频数据

#### 用途

| 方向 | 场景 | 帧类型 |
|------|------|--------|
| Watcher → Cloud | 语音上传 | AUD1 二进制帧 |
| Cloud → Watcher | TTS 播放 | AUD1 二进制帧 |
| Watcher → Cloud | 录音结束 | `{"type":"audio_end"}` JSON 文本 |

## UART 协议 (Watcher → MCU)

```
舵机指令:
X:90\r\n
Y:45\r\n

格式："<axis>:<angle>\r\n"
- axis: X 或 Y
- angle: 0-180
```

---

# 软件架构

## Watcher 架构 (ESP32-S3)

```
firmware/s3/main/
│
├── app_main.c              # 入口 + 系统初始化
│   - wifi_init()
│   - ws_client_init()
│   - display_ui_init()
│   - ws_router_init()
│
├── ws_client.c             # WebSocket 客户端
│   - ws_connect()
│   - ws_send_audio()       # AUD1 二进制帧上传
│   - ws_send_audio_end()   # 录音结束标记
│   - ws_handle_tts_binary() # TTS 二进制帧处理
│   - ws_tts_complete()     # TTS 播放完成
│
├── ws_router.c             # 消息路由框架
│   - ws_router_init()      # 注册处理器
│   - ws_route_message()    # JSON 消息分发
│
├── ws_handlers.c           # 消息处理器实现
│   - on_servo_handler()    # 舵机控制
│   - on_display_handler()  # 屏幕显示
│   - on_status_handler()   # 状态更新
│   - on_capture_handler()  # 拍照请求
│   - ws_state_to_emoji()   # 状态→表情映射
│
├── button_voice.c          # 按键语音触发
│   - voice_recorder_init()
│   - voice_recorder_process_event()
│   - voice_recorder_tick()
│
├── display_ui.c            # LVGL 屏幕显示
│   - display_ui_init()
│   - display_update()      # 文字 + 表情更新
│
├── emoji_anim.c            # PNG 动画系统
│   - emoji_anim_start()    # 启动动画定时器
│   - emoji_anim_stop()     # 停止动画
│
├── emoji_png.c             # SPIFFS PNG 加载
│   - emoji_png_load()      # 从 SPIFFS 加载 PNG
│
├── hal_audio.c             # 音频 HAL (I2S)
│   - hal_audio_start()     # 启动 I2S
│   - hal_audio_read()      # 读取麦克风数据
│   - hal_audio_write()     # 写入喇叭数据
│   - hal_audio_stop()      # 停止 I2S
│
├── hal_opus.c              # Opus 编解码 HAL (待实现)
│   - hal_opus_encode()     # PCM → Opus
│   - hal_opus_decode()     # Opus → PCM
│
├── hal_display.c           # 显示 HAL (LVGL)
│   - hal_display_init()
│   - hal_display_update()
│
├── hal_uart.c              # UART HAL
│   - hal_uart_init()
│   - hal_uart_send()
│
├── uart_bridge.c           # UART 转发到 MCU
│   - uart_bridge_init()
│   - uart_bridge_send_servo()
│
└── camera_capture.c        # Himax 拍照 (待实现)
```

## MCU 架构 (ESP32)

```
firmware/mcu/main/
│
├── main.c                  # 入口 + 核心逻辑
│   - uart_init()
│   - servo_init()
│   - main_loop()
│
├── uart_handler.c          # UART 指令处理
│   ├── uart_init()         # UART2 初始化 (GPIO 16 RX / GPIO 17 TX)
│   └── process_cmd()       # 指令解析; X/Y 缓存后原子执行，避免中间姿态
│
└── servo_control.c         # 舵机控制
    ├── ledc_init()         # LEDC 初始化 (50Hz PWM)
    ├── set_angle()         # 设置角度
    ├── smooth_move()       # 平滑移动
    └── angle_to_duty()     # 角度 → PWM 占空比转换
```

## 舵机参数

```c
#define SERVO_FREQ     50       // 50Hz (20ms 周期)
#define SERVO_RES      13       // 13 位分辨率 (0-8191)
#define SERVO_MIN_US   500      // 0.5ms = 0°
#define SERVO_MAX_US   2500     // 2.5ms = 180°

// 角度到 PWM 转换公式
// 周期 = 1000000µs / 50Hz = 20000µs，13位分辨率 → 8192 counts
// 0°  : 500µs  → duty ≈ 205
// 180°: 2500µs → duty = 1024
uint32_t angle_to_duty(int angle) {
    if (angle < 0) angle = 0;
    if (angle > 180) angle = 180;
    uint32_t pulse_us = SERVO_MIN_US + (angle * (SERVO_MAX_US - SERVO_MIN_US) / 180);
    return (pulse_us * (1 << SERVO_RES)) / (1000000 / SERVO_FREQ);
}
```

---

# 语音触发流程

## 唤醒词模式 (推荐)

使用乐鑫 ESP-SR 语音识别框架，实现离线唤醒词检测：

```
1. 监听麦克风 (VAD 持续检测)
   └── esp_sr_init() → 启动语音识别

2. 检测唤醒词 ("小微小微" / "你好小微")
   └── esp_wn_register_callback() → 回调

3. 唤醒成功 → 本地反馈
   ├── 舵机微动 (抬头/眨眼)
   ├── 屏幕显示 "我在听"
   └── 蜂鸣器短响

4. 开始录音 → 云端 ASR
   └── 后续流程同按键模式
```

## 按键模式 (已实现)

```
1. 用户长按旋钮按钮
   └── voice_recorder_process_event(VOICE_EVENT_BUTTON_PRESS)
   └── display_update("Listening...", "listening", ...)

2. 开始录音
   └── hal_audio_read() → PCM 数据
   └── hal_opus_encode() → Opus 数据 (待实现)
   └── ws_send_audio() → AUD1 二进制帧

3. 用户松开按钮
   └── voice_recorder_process_event(VOICE_EVENT_BUTTON_RELEASE)
   └── display_update("Ready", "happy", ...)

4. 结束录音 → 发送结束标记
   └── ws_send_audio_end() → {"type": "audio_end"}
```

## TTS 播放流程 (已实现)

```
1. 云端发送 TTS 音频
   └── WebSocket 二进制帧 (AUD1 格式)
   └── [Magic: "AUD1"][Length: N][Opus Data]

2. 设备接收并解码
   └── ws_handle_tts_binary() → 解析帧头
   └── hal_opus_decode() → PCM 数据 (待实现)
   └── hal_audio_write() → 播放

3. 云端发送状态更新
   └── {"type": "status", "state": "idle", ...}
   └── ws_tts_complete() → 停止音频
```

---

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
| ~~TTS 播放状态切换~~ | ✅ | **已完成** | 2秒超时自动切换到 happy (2026-03-01) |
| ~~动画状态映射优化~~ | ✅ | **已完成** | 7种表情状态 + 150ms 切换间隔 (2026-03-01) |
| **摄像头集成** | 🟢 低 | 可选 | Himax 拍照 + JPEG + WebSocket 发送 |
| **传感器数据上报** | 🟢 低 | 可选 | CO2/温湿度数据上报 |
| **唤醒词检测** | 🟢 低 | 可选 | ESP-SR 离线唤醒 "小微小微" |
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

*文档版本：2.5*
*更新日期：2026-03-01*

## 变更历史

| 版本 | 日期 | 变更内容 |
|------|------|----------|
| 2.5 | 2026-03-01 | **MVP v1.0.1 完成** - TTS 状态切换修复、动画优化、文字显示修复 |
| 2.4 | 2026-03-01 | **MVP v1.0 完成** - 端到端语音交互联调成功 |
| 2.3 | 2026-03-01 | S3 ↔ MCU UART 闭环完成，更新待开发功能清单 |
| 2.2 | 2026-02-28 | 更新通信协议为 AUD1 二进制帧格式；添加待开发功能清单 |
| 2.1 | 2026-02-27 | 添加 PNG 动画花屏修复记录 |
| 2.0 | 2026-02-26 | 重构文档结构，添加 TDD 任务切片 |
