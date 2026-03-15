# MVP-W: Watcher 最小可行产品

> ESP32-S3 AI 助手机器人 - 固件 + 硬件

[![License](https://img.shields.io/badge/License-Apache--2.0-blue.svg)](./LICENSE)
[![ESP-IDF](https://img.shields.io/badge/ESP--IDF-v5.2.1-orange.svg)](https://docs.espressif.com/projects/esp-idf/)

## 概述

MVP-W 是 Watcher AI 助手机器人的最小可行产品实现，采用三脑架构：

| 大脑 | 硬件 | 功能 |
|------|------|------|
| **肢体脑** | ESP32 (MCU) | 双轴舵机云台控制 |
| **视觉脑** | ESP32-S3 | 语音、显示、网络通信 |
| **AI 大脑** | PC (Cloud) | Claude API 集成 |

## 系统架构

```
┌──────────────────────────────────────────────────────────────────┐
│                    ESP32-S3 (S3 固件)                            │
│  ┌────────────┐ ┌────────────┐ ┌────────────┐ ┌──────────────┐ │
│  │ 音频 HAL   │ │ 显示屏 HAL │ │ WebSocket  │ │ 唤醒词检测   │ │
│  └────────────┘ └────────────┘ └────────────┘ └──────────────┘ │
│                              │                                   │
│                    UART (GPIO 19/20)                             │
│                              ▼                                   │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │                  ESP32 (MCU 固件)                         │   │
│  │  ┌────────────┐ ┌────────────┐ ┌────────────────────┐   │   │
│  │  │ UART 处理  │ │ 舵机控制   │ │ 运动计算           │   │   │
│  │  └────────────┘ └────────────┘ └────────────────────┘   │   │
│  │                    │                                      │   │
│  │                    ▼                                      │   │
│  │           双轴云台 (X: GPIO12, Y: GPIO13)                 │   │
│  └──────────────────────────────────────────────────────────┘   │
└──────────────────────────────────────────────────────────────────┘
                           │ WebSocket (WiFi)
                           ▼
┌──────────────────────────────────────────────────────────────────┐
│                    Cloud (PC Python)                             │
│  ┌────────────┐ ┌────────────┐ ┌────────────┐ ┌──────────────┐ │
│  │ ASR 阿里云 │ │ Claude API │ │ Agent      │ │ TTS 火山引擎 │ │
│  └────────────┘ └────────────┘ └────────────┘ └──────────────┘ │
└──────────────────────────────────────────────────────────────────┘
```

## 快速开始

### 前置要求

- ESP32-S3 开发板 (SenseCAP Watcher 或兼容板)
- ESP32 开发板 (MCU 舵机控制)
- ESP-IDF v5.2.1
- 16MB Flash + 8MB PSRAM

### 构建 S3 固件

```powershell
# 在 ESP-IDF PowerShell 中
C:\Espressif\frameworks\esp-idf-v5.2.1\export.ps1
cd firmware\s3
idf.py set-target esp32s3
idf.py build
idf.py -p COM3 flash monitor
```

### 构建 MCU 固件

```powershell
cd firmware\mcu
idf.py set-target esp32
idf.py -p COM4 flash monitor
```

### 运行云端服务器

云端服务器位于独立的 [watcher-server](https://github.com/BiboyLin/watcher-server) 仓库：

```bash
git clone https://github.com/BiboyLin/watcher-server.git
cd watcher-server
pip install -r requirements.txt
cp .env.example .env
# 编辑 .env 填入 API Keys
python src/main.py
```

## 目录结构

```
MVP-W/
├── README.md                    # 本文件
│
├── firmware/
│   ├── s3/                      # ESP32-S3 主控固件
│   │   ├── main/                # 主程序源码
│   │   │   ├── app_main.c       # 系统入口
│   │   │   ├── button_voice.c   # 语音状态机
│   │   │   ├── ws_client.c      # WebSocket 客户端
│   │   │   ├── hal_audio.c      # 音频 HAL
│   │   │   ├── hal_wake_word.c  # 唤醒词检测
│   │   │   ├── hal_display.c    # 显示 HAL
│   │   │   └── ...
│   │   ├── components/          # ESP-IDF 组件
│   │   ├── spiffs/              # PNG 动画资源
│   │   └── test_host/           # Host 单元测试
│   │
│   └── mcu/                     # ESP32 MCU 舵机控制固件
│       ├── main/                # 主程序源码
│       │   ├── main.c           # 系统入口
│       │   ├── servo_control.c  # 舵机控制
│       │   └── uart_handler.c   # UART 处理
│       └── test_host/           # Host 单元测试
│
├── docs/                        # 技术文档
│   ├── COMMUNICATION_PROTOCOL.md # WebSocket 协议
│   ├── VOICE_STREAM_PROTOCOL.md  # 语音流协议
│   └── INTEGRATION_TEST_PLAN.md  # 集成测试计划
│
└── server/                      # 边缘服务器 (旧版)
```

## 功能特性

| 功能 | 状态 | 说明 |
|------|------|------|
| 语音唤醒 | ✅ | ESP-SR "你好小智" |
| 语音录制 | ✅ | 16kHz PCM |
| VAD 静音检测 | ✅ | 自动停止录音 |
| ASR 语音识别 | ✅ | 阿里云 ASR |
| LLM 对话 | ✅ | Claude API |
| TTS 语音合成 | ✅ | 火山引擎 24kHz |
| 连续对话 | ✅ | TTS 后自动恢复唤醒词 |
| 显示动画 | ✅ | LVGL + PNG |
| 舵机控制 | ✅ | 双轴云台 |
| WebSocket | ✅ | 双向通信 |
| 服务发现 | ✅ | UDP 广播 |

## 语音信息流

```
用户语音 → 麦克风 → 唤醒词检测 → VAD → WebSocket
                                          ↓
                            Cloud: ASR → LLM → TTS
                                          ↓
用户 ← 扬声器 ← I2S ← TTS 二进制帧 ← WebSocket
```

## 硬件要求

| 组件 | 规格 |
|------|------|
| 主控 MCU | ESP32-S3-WROOM-1 |
| Flash | 16MB |
| PSRAM | 8MB (OPI) |
| 显示屏 | 412×412 SPD2010 QSPI |
| 麦克风 | I2S MEMS |
| 扬声器 | I2S DAC |
| 舵机 MCU | ESP32 |
| 舵机 X | GPIO 12 (0-180°) |
| 舵机 Y | GPIO 13 (0-180°) |

## 文档

- [架构文档](./docs/ARCHITECTURE.md) - 系统架构概述 (EN/CN)
- [通信协议](./docs/COMMUNICATION_PROTOCOL.md) - WebSocket 协议规范
- [构建指南](./firmware/s3/BUILD_INSTRUCTIONS.md) - 详细构建步骤
- [更新日志](./CHANGELOG.md) - 版本历史

## 贡献

欢迎提交 Issue 和 Pull Request！

## 许可证

Apache License 2.0，详见 [LICENSE](./LICENSE)。
