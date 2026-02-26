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
├── firmware/
│   ├── s3/                  # ESP32-S3 固件 (主控)
│   │   ├── CMakeLists.txt
│   │   └── main/
│   │       ├── app_main.c           # 入口 + 系统初始化
│   │       ├── ws_client.c          # WebSocket 客户端
│   │       ├── audio_pipeline.c     # 音频 pipeline
│   │       ├── button_voice.c       # 按键语音触发
│   │       ├── display_ui.c         # LVGL 屏幕显示
│   │       ├── camera_capture.c     # Himax 拍照
│   │       └── uart_bridge.c        # UART 转发到 MCU
│   └── mcu/                 # ESP32-MCU 固件 (身体)
│       ├── CMakeLists.txt
│       └── main/
│           ├── main.c                 # 入口 + 核心逻辑
│           ├── uart_handler.c         # UART 指令处理
│           └── servo_control.c        # 舵机控制
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

## WebSocket 消息格式 (Cloud ↔ Watcher)

### 控制指令 (Cloud → Watcher)

```json
// 舵机控制
{
    "type": "servo",
    "x": 90,
    "y": 45
}

// TTS 播放 (Opus 音频流)
{
    "type": "tts",
    "format": "opus",
    "data": "<base64 encoded opus>"
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
```

### 媒体流 (Watcher → Cloud)

```json
// 音频流
{
    "type": "audio",
    "format": "opus",
    "sample_rate": 16000,
    "data": "<base64 encoded opus>"
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
│   - audio_pipeline_init()
│   - display_ui_init()
│
├── ws_client.c             # WebSocket 客户端
│   - ws_connect()
│   - ws_send()
│   - ws_receive_loop()
│   - message_router()      # 根据 type 字段路由
│
├── audio_pipeline.c        # 音频 pipeline
│   ├── i2s_capture_init()  # 麦克风初始化
│   ├── opus_encode()       # Opus 编码
│   ├── opus_decode()       # Opus 解码
│   └── i2s_play_init()     # 喇叭初始化
│
├── button_voice.c          # 按键语音触发
│   ├── bsp_knob_btn_init() # 旋钮按钮初始化
│   ├── long_press_start()  # 长按开始录音
│   └── long_release_stop() # 长按结束录音
│
├── display_ui.c            # LVGL 屏幕显示
│   ├── bsp_lvgl_init()     # LVGL 初始化
│   ├── show_text()         # 显示文字
│   └── show_emoji()        # 显示表情 (happy/sad/surprised/angry/normal)
│
├── camera_capture.c        # Himax 拍照
│   └── sscma_client_sample() # 图像采集
│
└── uart_bridge.c           # UART 转发到 MCU
    ├── uart_init()         # UART 初始化
    └── uart_forward()      # 转发指令到 MCU
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

## 按键模式 (备用)

```
1. 用户长按旋钮按钮
   └── bsp_knob_btn_init() → 按钮中断

2. 开始录音
   └── bsp_i2s_read() → PCM 数据

3. 实时 Opus 编码 → WebSocket 发送
   └── opus_encode() → ws_send(audio)

4. 用户松开按钮
   └── button_release 事件

5. 结束录音 → 发送结束标记
   └── ws_send({"type": "audio_end"})
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

## Phase 1: 基础通信 (10h)

- [ ] ESP32 WebSocket 客户端
- [ ] PC WebSocket 服务器
- [ ] 双向文本消息通信
- [ ] 连接状态管理
- [ ] **可靠性机制**
  - [ ] 看门狗 (Watchdog) 配置
  - [ ] WebSocket 自动重连
  - [ ] 异常日志输出

## Phase 2: 音频通路 (10h)

- [ ] ESP32 I2S 麦克风采集
- [ ] ESP32 Opus 编码
- [ ] PC 端 Opus 解码
- [ ] PC 端音频录制测试

## Phase 3: 云端 AI 集成 (10h)

- [ ] Python ASR 接口 (Whisper 等)
- [ ] Python LLM 接口 (Claude/OpenAI)
- [ ] Agent 逻辑
- [ ] TTS 接口

## Phase 4: 控制闭环 (10h)

- [ ] 舵机控制指令
- [ ] TTS 播放通路
- [ ] 视频采集 (可选)
- [ ] 整体联调

---

# 关键文件参考

| 模块 | 文件 | 说明 |
|------|------|------|
| S3 入口 | `firmware/s3/main/app_main.c` | 系统初始化 |
| WS 客户端 | `firmware/s3/main/ws_client.c` | 通信核心 |
| 音频 pipeline | `firmware/s3/main/audio_pipeline.c` | 音视频处理 |
| 按键触发 | `firmware/s3/main/button_voice.c` | 语音输入 |
| 屏幕显示 | `firmware/s3/main/display_ui.c` | LVGL 显示 |
| UART 转发 | `firmware/s3/main/uart_bridge.c` | UART 转发到 MCU |
| MCU 入口 | `firmware/mcu/main/main.c` | 舵机控制 |
| UART 处理 | `firmware/mcu/main/uart_handler.c` | 指令解析 |
| 舵机控制 | `firmware/mcu/main/servo_control.c` | PWM 控制 |
| WS 服务器 | `server/websocket_server.py` | 云端入口 |
| Agent | `server/ai/agent.py` | AI 逻辑 |

---

# 技术参考

- **ESP32-S3 SDK**: `Watcher-Firmware/examples/helloworld/local_components/sensecap-watcher/`
- **MCU 参考**: `WatcherOld/MCU/main/main.c`
- **Himax 驱动**: `Watcher-Firmware/components/sscma_client/`
- **LVGL**: `Watcher-Firmware/components/lvgl/`
- **Opus**: `Watcher-Firmware/components/opus/`

---

# TDD 任务切片

## Phase 1: 纯软件逻辑与基础设施 (Host 测试环境)

这个阶段完全剥离硬件，专注于数据结构的解析和基础算法。Claude Code 可以在 Linux/Windows Host 环境下进行极速的"红 - 绿 - 重构"循环。

### Task 1.1: TDD 环境初始化 ✅

Host 测试环境已验证可用，运行命令：

```bash
# 构建（首次会自动拉取 Unity v2.5.2）
export PATH="/c/Espressif/tools/cmake/3.24.0/bin:/c/ProgramData/mingw64/mingw64/bin:$PATH"
cd firmware/mcu/test_host
cmake -B build -G "MinGW Makefiles" -DCMAKE_C_COMPILER=gcc
cmake --build build

# 运行所有测试
ctest --test-dir build -V
```

结果：**21 Tests  0 Failures  (ServoMath 7 + UartProtocol 14)**

### Task 1.2: MCU 运动计算层 (Math) ✅

实现于 `servo_math.c` 的 `angle_to_duty(int angle)`。
测试文件：`test_host/test_servo_math.c` — 7 个用例全部通过。
覆盖：0°/90°/180° 精确值、负值夹紧、>180 夹紧、单调性、13-bit 分辨率范围。

### Task 1.3: MCU UART 指令解析器 (Parser) ✅

实现于 `uart_protocol.c` 的 `parse_axis_cmd()`，解析文本格式 `X:90\r\n`（PRD 协议）。
测试文件：`test_host/test_uart_protocol.c` — 14 个用例全部通过。
覆盖：正常 X/Y 解析、边界 0/180、非法轴（Z/小写）、越界角度、格式错误、NULL 守卫。

### Task 1.4: Watcher WebSocket 消息路由 (Router)

实现 `ws_client.c` 中的指令分发器。

**测试要求**：根据接收到的 JSON 的 type 字段（servo, tts, display），准确调用对应的 Mock 处理函数。

## Phase 2: 核心状态机与协议转换 (Host 测试 + Mock 层)

这个阶段引入业务流程，但所有涉及外设的操作（如读串口、播放声音、刷屏幕）全部使用 Mock 函数替代。

### Task 2.1: Watcher UART 转发队列

实现 `uart_bridge.c`，将收到的 JSON 舵机指令转换为 `X:90\r\nY:45\r\n` 格式。

**测试要求**：验证字符串拼接的正确性，并确保转换后的指令成功推入发送队列（通过 Mock 的 hal_uart_send 验证）。

### Task 2.2: Watcher 音频采集状态机 (State Machine)

实现 `button_voice.c` 中的长按录音逻辑。

**测试要求**：模拟"按钮按下"事件，验证系统是否进入"录音中"状态并循环调用 Mock 的 opus_encode；模拟"按钮松开"事件，验证是否触发发送 `{"type": "audio_end"}` 标记。

### Task 2.3: Watcher LVGL 状态映射

实现 `display_ui.c` 中表情和文字的更新逻辑。

**测试要求**：传入指定的 emoji 字符串（如 "happy", "sad"），验证逻辑是否正确指向了对应的图片资源指针（Mock 掉实际的屏幕刷新 API）。

## Phase 3: 硬件抽象层 (HAL) 落地 (真实 ESP32 环境)

逻辑跑通且测试覆盖率达标后，切回真实的 ESP32-S3 和 ESP32 目标芯片，补齐底层驱动。这个阶段无法完全自动化 TDD，需要你结合逻辑分析仪或示波器进行验证。

### Task 3.1: MCU 舵机 PWM 驱动

实现 `servo_control.c` 中的 `ledc_init()`，将 GPIO 12 和 GPIO 13 配置为 50Hz PWM 输出。

### Task 3.2: Watcher 外设大集成

- **音频**：调用 sensecap-watcher SDK，打通 I2S 麦克风/喇叭与 Opus 编解码器。
- **视觉**：调用 Himax SPI 摄像头接口，实现抓图并进行 JPEG 压缩。
- **交互**：绑定真实的 GPIO 中断到按键旋钮，初始化真实的 LVGL 显示屏。

### Task 3.3: Watcher 网络层实现

配置并启动 WiFi Station 模式，连接路由器。

实现基于 `esp_websocket_client` 的真实连接、心跳维持和自动重连机制。

## Phase 4: 联调与集成 (双板 + 云端)

### Task 4.1: S3 与 MCU 的 UART 通信闭环

物理连接 GPIO 19/20，验证 Watcher 能否顺畅下发 `X:90\r\nY:45\r\n`，MCU 舵机是否准确响应。

### Task 4.2: 整体端云联调

与负责云端 Python 的同事对齐，将 Watcher 连入 WebSocket 服务器。

跑通"按键说话 -> ASR -> LLM 思考 (屏幕反馈) -> 舵机动作 + TTS 播报"的完整 MVP 体验链路。

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

*文档版本：2.0*
*更新日期：2026-02-26*
