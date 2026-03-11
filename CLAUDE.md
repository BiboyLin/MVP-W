# MVP-W 完整架构文档

> 本文档描述 Watcher 项目 MVP-W (Minimum Viable Product) 的完整技术架构
> 最后更新: 2026-03-11

## 目录

1. [项目概述](#项目概述)
2. [系统架构](#系统架构)
3. [目录结构](#目录结构)
4. [S3 固件详解](#s3-固件详解)
5. [MCU 固件详解](#mcu-固件详解)
6. [云端服务器详解](#云端服务器详解)
7. [通信协议](#通信协议)
8. [语音信息流](#语音信息流)
9. [关键模块说明](#关键模块说明)
10. [开发环境](#开发环境)
11. [待开发功能](#待开发功能)

---

## 项目概述

**MVP-W** 是 Watcher 项目的最小可行产品，集成三个核心"大脑"：

| 大脑 | 硬件 | 功能 |
|------|------|------|
| **肢体脑** | ESP32 (MCU) | 双轴舵机云台控制、电机驱动 |
| **视觉脑** | ESP32-S3 | Himax AI 视觉、LVGL UI、传感器 |
| **AI 大脑** | PC (Cloud) | Claude API 集成、ReAct Agent、Telegram/WS 控制 |

---

## 系统架构

```
┌─────────────────────────────────────────────────────────────────────────┐
│                           MVP-W 系统架构                                  │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  ┌─────────────────┐         ┌─────────────────┐         ┌───────────┐ │
│  │   麦克风 (I2S)   │         │   扬声器 (I2S)   │         │  Himax   │ │
│  │   I2S DMIC      │         │                 │         │  Camera  │ │
│  └────────┬────────┘         └────────┬────────┘         └─────┬─────┘ │
│           │                          │                        │        │
│           ▼                          ▼                        ▼        │
│  ┌─────────────────────────────────────────────────────────────────┐  │
│  │                    ESP32-S3 (S3 固件)                           │  │
│  │  ┌──────────────┐  ┌──────────────┐  ┌───────────────────────┐  │  │
│  │  │ 音频 HAL    │  │  显示屏 HAL  │  │  WebSocket 客户端    │  │  │
│  │  │ hal_audio.c │  │ hal_display │  │   ws_client.c       │  │  │
│  │  └──────────────┘  └──────────────┘  └───────────────────────┘  │  │
│  │  ┌──────────────┐  ┌──────────────┐  ┌───────────────────────┐  │  │
│  │  │ 唤醒词检测   │  │  语音状态机  │  │  消息路由/处理器      │  │  │
│  │  │ hal_wake_   │  │ button_voice│  │ ws_router/ws_handlers│  │  │
│  │  │ word.c      │  │ .c          │  │                      │  │  │
│  │  └──────────────┘  └──────────────┘  └───────────────────────┘  │  │
│  │  ┌──────────────┐  ┌──────────────┐  ┌───────────────────────┐  │  │
│  │  │ UART 桥接    │  │  PNG 动画    │  │  服务发现 (UDP)       │  │  │
│  │  │ uart_bridge │  │ emoji_png.c  │  │  discovery_client.c   │  │  │
│  │  └──────────────┘  └──────────────┘  └───────────────────────┘  │  │
│  └─────────────────────────────────────────────────────────────────┘  │
│                                    │                                    │
│                        UART (GPIO 19/20)                               │
│                                    ▼                                    │
│  ┌─────────────────────────────────────────────────────────────────┐  │
│  │                    ESP32 (MCU 固件)                              │  │
│  │  ┌──────────────┐  ┌──────────────┐  ┌───────────────────────┐  │  │
│  │  │ UART 处理    │  │  舵机控制    │  │  运动计算             │  │  │
│  │  │ uart_handler │  │ servo_control│  │   servo_math.c        │  │  │
│  │  └──────────────┘  └──────────────┘  └───────────────────────┘  │  │
│  └─────────────────────────────────────────────────────────────────┘  │
│                                    │                                    │
│                                    ▼                                    │
│  ┌─────────────────────────────────────────────────────────────────┐  │
│  │                      双轴云台 (舵机)                             │  │
│  │   X 轴: GPIO 12 (左右 0-180°)                                  │  │
│  │   Y 轴: GPIO 13 (上下 0-180°)                                  │  │
│  └─────────────────────────────────────────────────────────────────┘  │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
                                    │
                           WebSocket (WiFi)
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                         Cloud (PC Python)                              │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                    watcher-server (Python)                       │   │
│  │  ┌──────────────┐  ┌──────────────┐  ┌───────────────────────┐  │   │
│  │  │ WebSocket   │  │  消息处理    │  │  服务发现             │  │   │
│  │  │ Server      │  │ message_     │  │  discovery_server    │  │   │
│  │  │             │  │ handler.py   │  │                      │  │   │
│  │  └──────────────┘  └──────────────┘  └───────────────────────┘  │   │
│  │  ┌──────────────┐  ┌──────────────┐  ┌───────────────────────┐  │   │
│  │  │ ASR 模块     │  │ TTS 模块     │  │  OpenClaw Agent      │  │   │
│  │  │ 阿里云 ASR   │  │ 火山引擎 TTS │  │  Claude API          │  │   │
│  │  └──────────────┘  └──────────────┘  └───────────────────────┘  │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 目录结构

```
MVP-W/
├── CLAUDE.md                    # 本文档
├── PRD.md                       # 产品需求文档
├── .gitignore
│
├── firmware/                    # 嵌入式固件
│   ├── s3/                      # ESP32-S3 主控固件
│   │   ├── main/                # 主程序源码
│   │   ├── components/           # 组件
│   │   ├── spiffs/              # SPIFFS 分区资源
│   │   ├── sdkconfig            # ESP-IDF 配置
│   │   └── sdkconfig.defaults   # 默认配置 (关键!)
│   │
│   └── mcu/                     # ESP32 MCU 舵机控制固件
│       ├── main/                # 主程序源码
│       ├── test_host/           # Host 单元测试
│       └── sdkconfig
│
├── server/                      # Python 边缘服务器 (旧版)
│   ├── main.py                  # 主入口
│   └── websocket_server.py      # WS 服务器
│
└── docs/                        # 技术文档
    ├── COMMUNICATION_PROTOCOL.md # 通信协议
    ├── VOICE_STREAM_PROTOCOL.md  # 语音流协议
    ├── INTEGRATION_TEST_PLAN.md  # 集成测试计划
    └── AFE_PRIORITY_ISSUE.md     # 唤醒词优先级问题
```

---

## S3 固件详解

### 核心源文件

| 文件 | 行数 | 功能描述 |
|------|------|----------|
| **app_main.c** | ~300 | 系统入口，组件初始化 |
| **button_voice.c** | ~600 | 语音状态机 (唤醒词/按键触发) |
| **ws_client.c** | ~400 | WebSocket 客户端 + TTS 二进制帧 |
| **ws_router.c** | ~350 | JSON 消息路由框架 |
| **ws_handlers.c** | ~250 | 消息处理器实现 |
| **hal_audio.c** | ~200 | I2S 音频抽象层 (16kHz/24kHz 切换) |
| **hal_wake_word.c** | ~500 | ESP-SR 唤醒词检测 AFE |
| **hal_display.c** | ~400 | LVGL 显示 + 表情动画 |
| **emoji_png.c** | ~300 | SPIFFS PNG 加载 |
| **emoji_anim.c** | ~150 | PNG 动画定时器 |
| **uart_bridge.c** | ~150 | UART 转发到 MCU |
| **wifi_client.c** | ~100 | WiFi 连接 |
| **discovery_client.c** | ~300 | UDP 服务发现客户端 |
| **boot_animation.c** | ~250 | 启动动画 (arc 进度条) |
| **hal_button.c** | ~100 | GPIO 按键处理 |
| **hal_opus.c** | ~50 | Opus 空壳 (未实现) |

### 组件结构

```
firmware/s3/main/
├── CMakeLists.txt              # 组件编译配置
├── Kconfig.projbuild           # menuconfig 选项
├── app_main.c                  # 系统初始化入口
│
├── hal_*.c                     # 硬件抽象层
│   ├── hal_audio.c/h           # I2S 音频 (录音/播放)
│   ├── hal_display.c/h          # LCD 显示 (SPD2010 QSPI)
│   ├── hal_button.c/h          # GPIO 按键
│   ├── hal_uart.c/h            # UART 初始化
│   ├── hal_wake_word.c/h       # ESP-SR 唤醒词检测
│   └── hal_opus.c/h            # Opus 编解码 (待实现)
│
├── voice/                      # 语音处理
│   ├── button_voice.c/h        # 语音状态机 + VAD
│   └── (唤醒词和录音逻辑)
│
├── network/                     # 网络通信
│   ├── ws_client.c/h           # WebSocket 客户端
│   ├── ws_router.c/h           # 消息路由
│   ├── ws_handlers.c/h         # 消息处理器
│   ├── wifi_client.c/h         # WiFi 连接
│   └── discovery_client.c/h    # UDP 服务发现
│
├── display/                     # 显示界面
│   ├── display_ui.c/h          # LVGL UI 更新
│   ├── emoji_png.c/h           # PNG 加载
│   ├── emoji_anim.c/h          # 动画控制
│   └── boot_animation.c/h      # 启动动画
│
├── bridge/                      # 协议转换
│   ├── uart_bridge.c/h         # JSON → UART 转发
│   └── (转发到 MCU)
│
└── third_party/                 # 第三方库
    ├── cJSON.c/h               # JSON 解析
    └── (esp-sr, lvgl, etc.)
```

---

## MCU 固件详解

### 核心源文件

| 文件 | 功能 |
|------|------|
| **main.c** | 系统入口，GPIO 初始化 |
| **uart_handler.c** | UART 指令解析 (`X:90\r\nY:45\r\n`) |
| **servo_control.c** | PWM 舵机控制 |
| **servo_math.c** | 角度到 PWM 占空比转换 |

### 舵机控制

- **X 轴**: GPIO 12, 范围 0-180°
- **Y 轴**: GPIO 13, 范围 0-180°
- **通信**: UART (115200 8N1)

---

## 云端服务器详解

### watcher-server 结构

```
watcher-server/
├── main.py                     # 入口脚本
├── websocket_server.py         # WebSocket 服务器
├── requirements.txt             # Python 依赖
├── .env                         # 环境变量 (API Keys)
├── .env.example                 # 配置模板
│
├── src/
│   ├── main.py                  # 主入口
│   │
│   ├── core/                    # 核心模块
│   │   ├── websocket_server.py  # WS 服务器实现
│   │   └── thread_pool.py       # 线程池
│   │
│   ├── modules/                 # 功能模块
│   │   ├── asr/                 # 语音识别
│   │   │   ├── base.py          # ASR 基类
│   │   │   ├── factory.py       # ASR 工厂
│   │   │   └── aliyun_asr.py    # 阿里云 ASR 实现
│   │   │
│   │   ├── tts/                 # 语音合成
│   │   │   ├── base.py          # TTS 基类
│   │   │   └── huoshan_tts.py   # 火山引擎 TTS 实现
│   │   │
│   │   ├── openclaw/            # Claude Agent
│   │   │   ├── base.py          # Agent 基类
│   │   │   ├── local_claw.py    # 本地 Agent
│   │   │   └── tmux_claw.py     # Tmux Agent
│   │   │
│   │   ├── servo/               # 舵机控制
│   │   │   └── controller.py    # 舵机控制器
│   │   │
│   │   └── discovery/           # 服务发现
│   │       └── discovery_server.py  # UDP 广播服务器
│   │
│   ├── utils/                   # 工具函数
│   │   ├── logger.py            # 日志
│   │   ├── message_handler.py   # 消息处理
│   │   └── message_receiver.py  # 消息接收
│   │
│   └── config/                  # 配置
│       └── env.py               # 环境变量加载
│
├── config/                      # 配置文件
│
├── docs/                        # 文档
│   ├── websocket_message_protocol.md
│   ├── ALIYUN_ASR.md
│   ├── AI_CONTEXT.md
│   ├── ESP32_SERVICE_DISCOVERY.md
│   └── context/                 # 上下文文档
│
└── tests/                       # 测试
```

---

## 通信协议

### WebSocket 消息格式 (v2.0)

#### 设备 → 云端

**1. 音频数据 (二进制)**
```
原始 PCM 16kHz, 16-bit, mono
帧大小: 1920 bytes (60ms)
```

**2. 录音结束**
```json
{"type": "audio_end"}
```

**3. 状态上报**
```json
{"type": "status", "state": "listening" | "thinking" | "speaking" | "idle"}
```

#### 云端 → 设备

**1. 舵机指令**
```json
{"type": "servo", "x": 90, "y": 45}
```

**2. 屏幕显示**
```json
{"type": "display", "text": "你好!", "emoji": "happy"}
```

**3. TTS 音频 (二进制)**
```
原始 PCM 24kHz, 16-bit, mono
火山引擎 TTS 直接输出
```

**4. TTS 结束**
```json
{"type": "tts_end"}
```

### UART 协议 (S3 → MCU)

```
X:90\r\nY:45\r\n
```

---

## 语音信息流

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         语音信息流架构图                                 │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  ┌──────────────┐     ┌──────────────┐     ┌──────────────────────┐   │
│  │   麦克风     │     │   I2S HAL    │     │   唤醒词检测 AFE     │   │
│  │  (I2S DMIC) │────►│  hal_audio.c │────►│  hal_wake_word.c    │   │
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
│                                    ▼ WebSocket                          │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                         Cloud (PC)                               │   │
│  │  ┌─────────┐   ┌─────────┐   ┌─────────┐   ┌──────────────┐  │   │
│  │  │   ASR  │──►│   LLM   │──►│  Agent  │──►│     TTS      │  │   │
│  │  │(阿里云) │   │(Claude) │   │         │   │(火山引擎 24k)│  │   │
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

---

## 关键模块说明

### 1. 语音状态机 (button_voice.c)

负责管理语音输入的完整生命周期：

- **唤醒词模式**: "你好小智" 触发 → VAD 静音检测 → 自动停止
- **按键模式**: 长按 0.5s 触发 → 松开停止
- **连续对话**: TTS 播放后自动恢复唤醒词检测

### 2. WebSocket 客户端 (ws_client.c)

核心通信模块：

- 连接管理 (自动重连)
- 音频二进制帧发送
- TTS 二进制帧接收和播放
- JSON 消息路由分发

### 3. 消息路由 (ws_router.c + ws_handlers.c)

```
ws_route_message()
    ├── on_servo_handler()      → 转发 UART 到 MCU
    ├── on_display_handler()    → 更新 LVGL 屏幕
    ├── on_status_handler()     → 更新状态机
    └── on_capture_handler()   → 摄像头拍照 (待实现)
```

### 4. 服务发现 (discovery_client.c + discovery_server.py)

UDP 广播机制：

1. S3 发送 UDP 广播 (255.255.255.255:8767)
2. Server 响应 IP 地址
3. S3 连接 WebSocket

---

## 开发环境

### ESP-IDF 构建 (S3)

```powershell
# 激活 ESP-IDF
C:\Espressif\frameworks\esp-idf-v5.2.1\export.ps1

# 进入 S3 项目
cd D:\GithubRep\WatcherProject\MVP-W\firmware\s3

# 设置目标
idf.py set-target esp32s3

# 编译
idf.py build

# 烧录
idf.py -p COM3 flash monitor
```

### MCU 构建

```powershell
cd D:\GithubRep\WatcherProject\MVP-W\firmware\mcu
idf.py set-target esp32
idf.py -p COM4 flash monitor
```

### 服务器运行

```bash
cd D:\GithubRep\WatcherProject\watcher-server
pip install -r requirements.txt
python src/main.py
```

### Host 单元测试

```bash
# S3 测试
cd firmware/s3/test_host
cmake -B build -G "MinGW Makefiles"
cmake --build build
ctest --test-dir build -V

# MCU 测试
cd firmware/mcu/test_host
cmake -B build -G "MinGW Makefiles"
cmake --build build
ctest --test-dir build -V
```

---

## 待开发功能

| 功能 | 优先级 | 状态 |
|------|--------|------|
| 端到端语音交互 | ✅ | 已完成 |
| 唤醒词检测 | ✅ | 已完成 (ESP-SR) |
| VAD 静音检测 | ✅ | 已完成 |
| 服务发现 (UDP) | ✅ | 已完成 |
| TTS 采样率修复 | ✅ | 已完成 (24kHz) |
| 物理重启功能 | ✅ | 已完成 |
| 按键响应优化 | ✅ | 已完成 |
| S3 ↔ MCU UART 闭环 | ✅ | 已完成 |
| 摄像头集成 | 🟡 低 | 待开发 |
| 传感器数据上报 | 🟡 低 | 待开发 |
| Opus 编解码 | 🟡 中 | 待优化 |
| 离线模式 | 🟡 中 | 待增强 |

---

## 可靠性设计

### 看门狗 (Watchdog)

```c
// app_main.c
esp_task_wdt_init(30, true);
esp_task_wdt_add(NULL);
// 主循环中定期喂狗
esp_task_wdt_reset();
```

### WebSocket 重连

```c
// ws_client.c
// 断线后自动创建重连任务
ws_start_reconnect_task();
```

---

## 文档变更历史

| 版本 | 日期 | 变更内容 |
|------|------|----------|
| 1.0 | 2026-03-11 | 首次创建，基于最新代码结构全面梳理 |
