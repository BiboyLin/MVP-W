# MVP-W 架构文档 v2.0

> 本文档描述 Watcher MVP-W 的**目标架构**（重构后），供 Claude Code 在开发时遵循。
> 上一版本：v1.0 (2026-03-11) — 描述当前已实现架构
> 当前版本：v2.0 (2026-03-11) — 描述重构目标架构（**先不动代码，仅作设计指导**）

---

## 一、项目概述

**MVP-W** 是 Watcher AI 助理机器人的最小可行产品，单芯片架构：

| 大脑 | 硬件 | 功能 |
|------|------|------|
| **视觉+肢体脑** | ESP32-S3 | 显示、语音、舵机直驱、BLE、摄像头 |
| **AI 大脑** | PC（Cloud） | Claude API、ASR、TTS、WebSocket 控制 |

> MCU（ESP32 舵机板）已废弃，舵机改由 S3 GPIO 19/20 直驱。

---

## 二、仓库顶层结构

遵循 OSHWA 开源硬件规范 + ESP-IDF 项目标准：

```
MVP-W/
├── README.md                      # 项目总览（必需）
├── LICENSE                        # Apache-2.0（固件）/ CERN-OHL-P（硬件）
├── CONTRIBUTING.md                # 贡献指南
├── CHANGELOG.md                   # 项目级变更日志
├── CODE_OF_CONDUCT.md             # 行为准则
│
├── docs/                          # 项目文档
│   ├── architecture.md            # 系统架构图（本文摘要版）
│   ├── getting-started.md         # 快速上手（30分钟内能跑起来）
│   ├── hardware/
│   │   ├── gpio-mapping.md        # GPIO 引脚分配表
│   │   └── bom.md                 # 硬件清单
│   └── api/                       # Doxygen 自动生成（CI 产物）
│
├── hardware/                      # 硬件设计源文件（KiCad 原始格式）
│   └── .gitkeep
│
├── firmware/
│   └── s3/                        # ESP32-S3 固件（主要开发目录）
│       ├── CMakeLists.txt
│       ├── sdkconfig.defaults      # 默认配置（关键，勿随意改）
│       ├── sdkconfig.defaults.esp32s3
│       ├── partitions.csv          # 分区表（含双 OTA 分区）
│       ├── main/                   # 入口（≤ 100 行，只做初始化编排）
│       ├── components/             # 所有自定义组件（见第三节）
│       └── managed_components/     # IDF Component Manager 拉取（.gitignore）
│
├── firmware/mcu/                   # ⚠️ DEPRECATED，保留不删除
│   └── README_DEPRECATED.md
│
├── tools/                          # PC 端工具脚本
│   ├── png2rgba.py                 # PNG → RGB565 binary 转换工具
│   ├── anim_pack.py                # 动画资源打包工具
│   └── flash.sh                    # 烧录辅助脚本
│
└── .github/
    ├── workflows/
    │   └── build.yml               # GitHub Actions CI（多目标并行构建）
    ├── ISSUE_TEMPLATE/
    │   ├── bug_report.md
    │   └── feature_request.md
    └── PULL_REQUEST_TEMPLATE.md
```

---

## 三、固件组件分层架构

遵循 ESP-IDF 官方三层 + 业务层的四层模型：

```
firmware/s3/components/
│
├── drivers/                    ← 第一层：板级驱动（BSP）
│   └── bsp_watcher/            # Watcher S3 板级支持包
│
├── hal/                        ← 第二层：硬件抽象层
│   ├── hal_audio/              # I2S 录音 + 播放（16kHz/24kHz 切换）
│   ├── hal_display/            # LCD（SPD2010 QSPI）+ LVGL 初始化
│   ├── hal_button/             # GPIO 按键（长按/多击）
│   ├── hal_servo/              # LEDC PWM 舵机直驱（GPIO 19/20）[新增]
│   └── hal_camera/             # SSCMA 客户端封装 → Himax JPEG 帧 [新增]
│
├── protocols/                  ← 第三层：通信协议层
│   ├── ws_client/              # WebSocket 客户端 + 协议路由 + 消息处理器
│   ├── discovery/              # UDP 服务发现客户端
│   └── ble_service/            # BLE GATT 控制 + WiFi Provisioning [新增]
│
├── services/                   ← 第四层：业务逻辑层
│   ├── voice_service/          # 语音状态机 + 唤醒词检测
│   ├── anim_service/           # 动画引擎（30fps，PNG→PSRAM，lv_animimg）
│   ├── camera_service/         # 摄像头视频推流调度 [新增]
│   └── ota_service/            # 固件 OTA（HTTP 下载 + WS 通知）[新增]
│
└── utils/                      ← 通用工具（跨层复用）
    ├── wifi_manager/           # WiFi 连接 + 自动重连
    └── boot_anim/              # 启动动画（arc 进度条）
```

### 层间依赖规则（严格遵守）

```
services   →  可依赖 hal、protocols、utils
protocols  →  可依赖 hal、utils
hal        →  可依赖 drivers、utils（不可反向依赖）
drivers    →  仅依赖 ESP-IDF 官方组件
utils      →  仅依赖 ESP-IDF 官方组件（不可依赖其他自定义层）

❌ 禁止：hal 依赖 services / protocols 依赖 services
❌ 禁止：utils 依赖任何自定义组件
```

---

## 四、单组件标准结构

每个组件必须符合以下结构（开源 ESP-IDF 组件规范）：

```
my_component/
├── CMakeLists.txt              # 构建配置（必需）
├── idf_component.yml           # 组件清单（版本/描述/依赖/许可证）
├── Kconfig                     # menuconfig 可配置选项
├── README.md                   # 功能说明、安装、使用示例
├── CHANGELOG.md                # 版本变更记录
├── include/                    # 公共头文件（对外 API，含 Doxygen 注释）
│   └── my_component.h
├── src/                        # 源文件（实现细节）
│   ├── my_component.c
│   └── my_component_internal.h # 内部头文件（不对外暴露）
└── test_apps/                  # Unity 单元测试
    └── main/
        ├── CMakeLists.txt
        └── test_my_component.c
```

### idf_component.yml 规范

```yaml
version: "1.0.0"           # SemVer，必填
description: "..."          # 一句话描述，必填
url: "https://github.com/your-org/watcher"
license: "Apache-2.0"
targets:
  - esp32s3
dependencies:
  idf: ">=5.2"
  # 明确声明所有直接依赖
maintainers:
  - "your@email.com"
```

### CMakeLists.txt 依赖声明规范

```cmake
idf_component_register(
    SRCS "src/my_component.c"
    INCLUDE_DIRS "include"
    REQUIRES    "..."    # 公共头文件中引用的组件（会传递给使用者）
    PRIV_REQUIRES "..."  # 仅 .c 文件内部使用的组件（不传递）
)
# 原则：REQUIRES 尽量精简，大部分依赖放 PRIV_REQUIRES
```

### Kconfig 规范

所有可配置参数**必须**通过 Kconfig 暴露，禁止在源码中硬编码：

```kconfig
menu "Watcher HAL Servo"
    config WATCHER_SERVO_X_GPIO
        int "Servo X axis GPIO number"
        default 19
        help
            GPIO pin for X-axis servo PWM output.
            Default 19 (previously used as UART TX to MCU).

    config WATCHER_SERVO_Y_GPIO
        int "Servo Y axis GPIO number"
        default 20
        help
            GPIO pin for Y-axis servo PWM output.
            Default 20 (previously used as UART RX from MCU).

    config WATCHER_SERVO_Y_MIN_DEG
        int "Servo Y axis minimum angle (degrees)"
        default 90
        range 0 180
        help
            Mechanical protection lower limit for Y axis.
endmenu
```

### Doxygen 公共 API 注释规范

所有 `include/` 下的公共函数必须写 Doxygen 注释：

```c
/**
 * @brief Initialize servo HAL with LEDC PWM output.
 *
 * Configures GPIO 19 (X axis) and GPIO 20 (Y axis) as LEDC PWM channels
 * and starts the smooth-move background task.
 *
 * @note Must be called before any servo_set_angle() calls.
 * @note GPIO 19/20 are repurposed from UART (MCU communication removed).
 *
 * @return
 *   - ESP_OK    on success
 *   - ESP_FAIL  if LEDC timer/channel configuration fails
 */
esp_err_t hal_servo_init(void);
```

---

## 五、各组件详细设计

### 5.1 `drivers/bsp_watcher` — 板级支持包

**职责**：封装 Watcher S3 硬件引脚定义和外设初始化，上层代码不直接操作寄存器。

```c
// include/bsp_watcher.h
#define BSP_SERVO_X_GPIO    CONFIG_WATCHER_SERVO_X_GPIO   // 19
#define BSP_SERVO_Y_GPIO    CONFIG_WATCHER_SERVO_Y_GPIO   // 20
#define BSP_BUTTON_GPIO     CONFIG_WATCHER_BUTTON_GPIO    // SDK 管理
#define BSP_I2S_MIC_SCK     ...                           // 麦克风
#define BSP_I2S_SPK_BCLK    ...                           // 扬声器

esp_err_t bsp_board_init(void);   // 初始化所有外设
i2c_port_t bsp_i2c_get_port(void);
```

**依赖**：`sensecap-watcher`（Seeed SDK，保持现有 components/ 下的版本）

---

### 5.2 `hal/hal_servo` — 舵机直驱 HAL [新增]

**职责**：LEDC PWM 输出控制双轴舵机，含平滑插值任务。

**关键设计**：
- GPIO 19 → 舵机 X 轴（原 UART TX to MCU）
- GPIO 20 → 舵机 Y 轴（原 UART RX from MCU）
- 平滑移动：FreeRTOS 后台任务，10ms 步进插值
- Y 轴机械保护限位：90°~150°（Kconfig 可配）

```c
// include/hal_servo.h
esp_err_t hal_servo_init(void);
esp_err_t hal_servo_set_angle(servo_axis_t axis, int angle_deg);
esp_err_t hal_servo_move_smooth(servo_axis_t axis, int angle_deg, int duration_ms);
esp_err_t hal_servo_move_sync(int x_deg, int y_deg, int duration_ms);
int       hal_servo_get_angle(servo_axis_t axis);
```

**迁移来源**：`firmware/mcu/main/servo_control.c`（逻辑直接移植，GPIO 从 MCU 引脚改为 S3 引脚）

---

### 5.3 `hal/hal_camera` — 摄像头 HAL [新增]

**职责**：封装 SSCMA 客户端（Himax 视觉芯片通信），提供 JPEG 帧回调接口。

```c
// include/hal_camera.h
typedef void (*hal_camera_frame_cb_t)(const uint8_t *jpeg, size_t size,
                                       uint32_t timestamp_ms, void *ctx);

esp_err_t hal_camera_init(void);
esp_err_t hal_camera_start(int fps, hal_camera_frame_cb_t cb, void *ctx);
esp_err_t hal_camera_stop(void);
esp_err_t hal_camera_capture_once(hal_camera_frame_cb_t cb, void *ctx);
```

**依赖**：`sscma_client`（已在 components/ 中）

---

### 5.4 `protocols/ws_client` — WebSocket 协议层

**职责**：WebSocket 连接管理、消息路由、协议 v2.x 序列化/反序列化。

**从 `main/` 迁移**：`ws_client.c`、`ws_router.c`、`ws_handlers.c`

```
src/
├── ws_client.c        # 连接管理、TTS 二进制帧处理
├── ws_router.c        # JSON 消息路由框架
├── ws_handlers.c      # 各类型消息处理器实现
└── ws_protocol.c      # 消息序列化/反序列化（替代内嵌 cJSON 调用）
```

**cJSON 处理**：`PRIV_REQUIRES "json"` 使用 ESP-IDF 内置组件，移除 `main/cJSON.c/h`。

---

### 5.5 `protocols/ble_service` — 蓝牙 LE [新增]

**职责**：BLE GATT 控制服务（舵机/显示/状态）+ WiFi Provisioning。

**Kconfig 选项**：
```kconfig
config WATCHER_BLE_ENABLE
    bool "Enable BLE service"
    default n        # 默认关闭，避免增加首次编译固件体积
```

---

### 5.6 `services/anim_service` — 动画引擎（30fps）

**职责**：动画资源加载（PNG → PSRAM 解码）+ lv_animimg 播放器。

**从 `main/` 迁移**：`emoji_png.c`、`emoji_anim.c`、`display_ui.c`

**关键改动**：
- `anim_storage.c`：PNG 加载后立即解码为 RGB565 写入 PSRAM，释放 PNG 压缩数据
- `anim_player.c`：使用 `lv_animimg` 原生 widget，帧切换 < 1ms，支持 30fps
- `anim_meta.c`：解析 `/spiffs/anim/anim_meta.json`（帧数、fps 配置化）

**PSRAM 内存规划**：
- 单帧：412 × 412 × 2（RGB565）= 332KB
- 策略：懒加载，仅保留当前激活动画类型的所有帧（最多 ~18 帧上限）

---

### 5.7 `services/voice_service` — 语音服务

**从 `main/` 迁移**：`button_voice.c`（重命名 `voice_fsm.c`）、`hal_wake_word.c`（移入 `src/`）

---

### 5.8 `services/camera_service` — 视频推流服务 [新增]

**职责**：调度摄像头帧推流，处理带宽竞争，封装 WS 二进制帧格式。

**带宽优先级规则**：
```
TTS 播放中    → 推流暂停（音频优先）
语音录音中    → 推流降至 1fps
WebSocket 断连 → 推流停止，重连后不自动恢复
```

**WS 二进制帧格式**：
```
[4B magic "WVID"][4B timestamp_ms][2B width][2B height][4B jpeg_size][JPEG data]
```

---

### 5.9 `services/ota_service` — 固件 OTA [新增]

**职责**：WebSocket 通知触发 + HTTP 下载固件 + `esp_ota_ops` 写分区。

**方案**：HTTP OTA（`esp_https_ota`）+ WebSocket 通知，不自实现下载可靠性。

```c
// include/ota_service.h
esp_err_t ota_service_init(void);
esp_err_t ota_service_start(const char *url, const char *expected_version);
void      ota_service_mark_valid(void);   // app_main 末尾调用，防 rollback
const char* ota_service_get_fw_version(void);
```

---

### 5.10 `utils/wifi_manager` — WiFi 管理

**从 `main/` 迁移**：`wifi_client.c`

---

### 5.11 `utils/boot_anim` — 启动动画

**从 `main/` 迁移**：`boot_animation.c`

---

## 六、分区表（重构后）

支持双 OTA 分区（固件热更新）+ 动画资源存储：

```csv
# Name,    Type, SubType,  Offset,    Size      Comment
nvs,       data, nvs,      0x9000,    0x6000    # 24KB  NVS
phy_init,  data, phy,      0xF000,    0x1000    # 4KB   PHY 校准
otadata,   data, ota,      0x10000,   0x2000    # 8KB   OTA 启动选择
ota_0,     app,  ota_0,    0x20000,   0x500000  # 5MB   固件分区 A（当前运行）
ota_1,     app,  ota_1,    0x520000,  0x500000  # 5MB   固件分区 B（OTA 目标）
model,     data, spiffs,   0xA20000,  0x50000   # 320KB ESP-SR 唤醒词模型
storage,   data, spiffs,   0xA70000,  0x580000  # 5.5MB 动画资源 + OTA 缓冲
# 合计 ≈ 15.9MB，适配 16MB Flash
```

**存储分区目录结构**（SPIFFS `/spiffs/`）：
```
/spiffs/
├── anim/
│   ├── anim_meta.json          # 动画元数据（帧数、fps、版本）
│   ├── greeting/001.png, 002.png, ...
│   ├── listening/
│   ├── speaking/
│   ├── analyzing/
│   ├── standby/
│   ├── detected/
│   └── thinking/
└── anim_ota/
    └── .tmp/                   # OTA 接收临时区
```

---

## 七、WebSocket 协议（v2.3）

### 下行（Server → Device）

| type | 格式 | 说明 |
|------|------|------|
| `servo` | `{"type":"servo","id":"X","angle":90,"time":500}` | 舵机控制 |
| `display` | `{"type":"display","text":"...","emoji":"speaking"}` | 显示更新 |
| `status` | `{"type":"status","data":"processing"}` | 状态同步 |
| `camera_start` | `{"type":"camera_start","fps":5}` | 开始推流 |
| `camera_stop` | `{"type":"camera_stop"}` | 停止推流 |
| `camera_capture` | `{"type":"camera_capture"}` | 单帧抓拍 |
| `fw_ota_notify` | `{"type":"fw_ota_notify","version":"2.1.0","url":"/fw/...","sha256":"..."}` | 固件更新通知 |
| `anim_ota_start` | `{"type":"anim_ota_start","anim_name":"speaking","frame_count":10,...}` | 动画资源更新 |
| `reboot` | `{"type":"reboot"}` | 远程重启 |

### 上行（Device → Server）

| type | 格式 | 说明 |
|------|------|------|
| `audio_end` | `{"type":"audio_end"}` | 录音结束 |
| `status` | `{"type":"status","state":"listening"}` | 状态上报 |
| `device_info` | `{"type":"device_info","fw_version":"2.0.0","hw_id":"Watcher-XXXX"}` | 设备上线上报 |
| `fw_ota_progress` | `{"type":"fw_ota_progress","percent":45}` | OTA 进度 |
| `fw_ota_complete` | `{"type":"fw_ota_complete","status":"ok"}` | OTA 完成 |
| `camera_ai_result` | `{"type":"camera_ai_result","boxes":[...],"timestamp":...}` | AI 检测结果 |

### 二进制帧

| 方向 | 格式 | 说明 |
|------|------|------|
| 上行 | 裸 PCM 16kHz 16-bit mono | 语音数据 |
| 下行 | 裸 PCM 24kHz 16-bit mono | TTS 音频 |
| 上行 | `[WVID][ts][w][h][size][JPEG]` | 视频帧 |

---

## 八、文件迁移对照表

| 当前文件 | 迁移目标 | 动作 |
|----------|----------|------|
| `main/hal_audio.c/h` | `hal/hal_audio/src/ + include/` | 迁移 |
| `main/hal_display.c/h` | `hal/hal_display/src/ + include/` | 迁移 |
| `main/hal_button.c/h` | `hal/hal_button/src/ + include/` | 迁移 |
| `main/hal_uart.c/h` | — | **删除**（舵机改 GPIO 直驱） |
| `main/hal_opus.c/h` | `hal/hal_audio/src/`（子模块） | 迁移合并 |
| `main/hal_wake_word.c/h` | `services/voice_service/src/` | 迁移 |
| `main/button_voice.c/h` | `services/voice_service/src/voice_fsm.c` | 迁移重命名 |
| `main/emoji_png.c/h` | `services/anim_service/src/anim_storage.c` | 迁移重命名 |
| `main/emoji_anim.c/h` | `services/anim_service/src/anim_player.c` | 迁移重命名 |
| `main/display_ui.c/h` | `services/anim_service/src/` | 迁移 |
| `main/boot_animation.c/h` | `utils/boot_anim/src/` | 迁移 |
| `main/ws_client.c/h` | `protocols/ws_client/src/` | 迁移 |
| `main/ws_router.c/h` | `protocols/ws_client/src/` | 迁移 |
| `main/ws_handlers.c/h` | `protocols/ws_client/src/` | 迁移 |
| `main/wifi_client.c/h` | `utils/wifi_manager/src/` | 迁移 |
| `main/discovery_client.c/h` | `protocols/discovery/src/` | 迁移 |
| `main/uart_bridge.c/h` | — | **删除**（UART 通信废弃） |
| `main/cJSON.c/h` | — | **删除**，改用 `PRIV_REQUIRES "json"` |
| `firmware/mcu/` 全部 | `firmware/mcu/README_DEPRECATED.md` | 保留+标记废弃 |

**新增文件**：
| 新文件 | 位置 | 说明 |
|--------|------|------|
| `hal_servo.c/h` | `hal/hal_servo/` | 舵机 GPIO 直驱 |
| `hal_camera.c/h` | `hal/hal_camera/` | Himax SSCMA 封装 |
| `ble_service.c/h` | `protocols/ble_service/` | BLE GATT + Provisioning |
| `camera_service.c/h` | `services/camera_service/` | 视频推流调度 |
| `ota_service.c/h` | `services/ota_service/` | 固件 OTA |

---

## 九、app_main.c 设计约束

`main/app_main.c` 是唯一不在组件中的源文件，必须：

- **行数 ≤ 100 行**
- 只做初始化编排（调用各组件 `_init()` 函数）+ 主循环
- 不包含任何业务逻辑
- 所有回调通过组件接口注册，不直接实现

```c
// app_main.c 结构示意
void app_main(void) {
    // 1. BSP 板级初始化
    bsp_board_init();

    // 2. HAL 层初始化（硬件就绪）
    hal_display_init();
    hal_audio_init();
    hal_servo_init();

    // 3. 工具层
    boot_anim_start();
    wifi_manager_connect();

    // 4. 协议层
    discovery_start(&server_info);
    ws_client_connect(url);
    ble_service_init();         // 若启用

    // 5. 业务服务层
    anim_service_init();
    voice_service_start();
    ota_service_mark_valid();   // 防止 rollback

    // 6. 启动完成
    boot_anim_finish();

    // 主循环：看门狗 + TTS 超时检测
    while (1) {
        esp_task_wdt_reset();
        ws_tts_timeout_check();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
```

---

## 十、开发规范

### 代码风格
- 遵循 ESP-IDF 官方代码风格（`clang-format`，LLVM 基准）
- 项目根目录放置 `.clang-format` 文件
- 函数命名：`{组件前缀}_{动词}_{名词}`，如 `hal_servo_set_angle()`
- 错误处理：所有函数返回 `esp_err_t`，调用方使用 `ESP_ERROR_CHECK` 或显式处理

### 提交规范（Conventional Commits）
```
feat(hal_servo): add synchronized dual-axis move
fix(anim_service): release PSRAM before loading new type
refactor(ws_client): move protocol parsing to ws_protocol.c
docs(hal_camera): add Doxygen for capture_once API
test(voice_service): add unity test for VAD timeout
```

### 测试规范
- 每个组件的 `test_apps/` 目录下编写 Unity 测试
- Host 测试（CMake + MinGW）用于纯逻辑模块
- 新功能 PR 必须附带对应测试

### CI/CD（GitHub Actions）
```yaml
# .github/workflows/build.yml
- 触发：push / PR 到 main
- 任务：esp32s3 目标构建 + clang-format 格式检查
- 工具：espressif/esp-idf-ci-action@v1，IDF v5.2.1
```

---

## 十一、实施阶段（Phase 计划）

| Phase | 内容 | 优先级 | 前置条件 |
|-------|------|--------|----------|
| **Phase 1** | 架构重构（组件化，代码迁移） | P0 | 无 |
| **Phase 2** | 舵机 GPIO 直驱（`hal_servo`） | P0 | Phase 1 |
| **Phase 3** | 动画引擎 30fps（`anim_service`） | P0 | Phase 1 |
| **Phase 4** | 分区表重构（Factory → OTA 双分区） | P1 | Phase 1 |
| **Phase 5** | 固件 OTA（`ota_service`） | P1 | Phase 4 |
| **Phase 6** | BLE 模块（`ble_service`） | P1 | Phase 1 |
| **Phase 7** | 摄像头视频推流（`hal_camera` + `camera_service`） | P1 | Phase 1 + 硬件验证 |
| **Phase 8** | 动画资源 OTA（`anim_ota`） | P2 | Phase 3 |

---

## 十二、重要注意事项

1. **ESP-IDF 构建环境**：不要在 Claude Code Bash 中运行 `idf.py`，需在 ESP-IDF PowerShell 中执行
2. **PSRAM 依赖**：`hal_display`、`anim_service`、`hal_camera` 均需 PSRAM，`sdkconfig.defaults` 中 PSRAM 配置不可随意改动
3. **WiFi + BLE 共存**：启用 `CONFIG_ESP32_WIFI_SW_COEXIST_ENABLE`，TTS 播放期间限制 BLE 广播
4. **sensecap-watcher SDK**：`components/sensecap-watcher/` 是第三方 SDK，不修改其内部代码，通过 `bsp_watcher` 封装使用
5. **分区表变更**：从 `factory` 切换到 `ota_0/ota_1` 需要全量重烧，首次需要手动操作
6. **MCU 目录**：`firmware/mcu/` 保留不删除，仅添加 `README_DEPRECATED.md`

---

> 文档变更历史：
> - v1.0 (2026-03-11): 首次创建，基于已实现代码梳理
> - v2.0 (2026-03-11): 重构目标架构，对齐开源 ESP-IDF 组件规范（PRD v2.0）
