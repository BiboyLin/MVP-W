# CLAUDE.md - MVP-W

## 项目概述

MVP-W (Minimum Viable Product - Watcher) 是一个边缘设备 + 云端大脑的最小可行版本。

- **ESP32-S3 (主控)**: 音视频采集、WebSocket通信、屏幕显示、按键触发
- **ESP32-MCU (身体)**: 舵机控制，通过UART与主控通信
- **PC 端**: Python后端，处理ASR、LLM Agent、TTS

## 项目结构

```
MVP-W/
├── PRD.md                   # 项目需求文档
├── firmware/
│   ├── s3/                 # ESP32-S3 固件 (主控)
│   │   └── main/          # 基于 Watcher-Firmware SDK
│   └── mcu/               # ESP32-MCU 固件 (身体)
│       └── main/          # 参考 WatcherOld/MCU
└── server/                 # PC 云端
    ├── server/            # WebSocket 服务
    └── ai/               # AI 模块
```

## 快速开始

### 1. ESP32-S3 固件 (主控)

```bash
cd firmware/s3
idf.py set-target esp32s3
idf.py build
idf.py -p COM3 flash monitor
```

### 2. ESP32-MCU 固件 (身体)

```bash
cd firmware/mcu
idf.py set-target esp32
idf.py build
idf.py -p COM4 flash monitor
```

### 3. PC 云端

```bash
cd server
pip install -r requirements.txt
python main.py
```

## 关键文件

| 模块 | 文件 | 说明 |
|------|------|------|
| S3 入口 | `firmware/s3/main/app_main.c` | 系统初始化 |
| WS客户端 | `firmware/s3/main/ws_client.c` | 通信核心 |
| 按键触发 | `firmware/s3/main/button_voice.c` | 语音输入 |
| 屏幕显示 | `firmware/s3/main/display_ui.c` | LVGL 显示 |
| MCU入口 | `firmware/mcu/main/main.c` | 舵机控制 |
| WS服务器 | `server/websocket_server.py` | 云端入口 |
| Agent | `server/ai/agent.py` | AI 逻辑 |

## 通信协议

见 `PRD.md` 第三章 "通信协议"

## 技术参考

- ESP32-S3 SDK: `Watcher-Firmware/examples/helloworld/local_components/sensecap-watcher/`
- MCU参考: `WatcherOld/MCU/main/main.c`
