# MVP-W v1.5.0

**发布日期**: 2026-03-15

MVP-W v1.5.0 是首个开源发布版本，包含完整的语音交互功能。

## 版本亮点

- 端到端语音交互（按键触发 → ASR → LLM → TTS）
- ESP-SR 离线唤醒词检测 ("Hi 乐鑫")
- VAD 静音自动停止
- PNG 动画显示（24帧表情）
- WebSocket 双向通信
- UDP 服务发现
- 双轴舵机云台控制（通过 MCU）

## 固件文件

| 文件 | 地址 | 大小 | 校验 |
|------|------|------|------|
| `firmware/bootloader.bin` | 0x0 | 21KB | ESP-IDF 引导程序 |
| `firmware/partition-table.bin` | 0x8000 | 3KB | 分区表 |
| `firmware/MVP-W-S3.bin` | 0x10000 | 1.89MB | 应用固件 |
| `firmware/srmodels.bin` | 0x410000 | 285KB | 唤醒词模型 |
| `firmware/storage.bin` | 0x460000 | 10.5MB | PNG 动画资源 |

## 系统要求

### 硬件
- ESP32-S3 开发板（16MB Flash + 8MB PSRAM）
- I2S 麦克风（DMIC）
- I2S 扬声器
- LCD 显示屏（可选）
- ESP32 MCU（舵机控制，可选）

### 软件
- Python 3.8+
- esptool 4.6+

## 烧录步骤

### 方法 1: 使用烧录脚本（推荐）

#### Windows
```powershell
# 安装 esptool
pip install esptool

# 进入固件目录
cd releases\v1.5.0

# 完整烧录（默认 115200 波特率，约 3 分钟）
.\flash.ps1 -Port COM3

# 或仅更新应用
.\flash.ps1 -Port COM3 -AppOnly
```

#### Linux/macOS
```bash
# 安装 esptool
pip install esptool

# 进入固件目录
cd releases/v1.5.0

# 添加执行权限
chmod +x flash.sh

# 完整烧录（默认 115200 波特率，约 3 分钟）
./flash.sh /dev/ttyUSB0

# 或仅更新应用
./flash.sh --app-only /dev/ttyUSB0
```

### 方法 2: 手动烧录

```bash
# 进入下载模式：按住 BOOT，按 RST，松开 BOOT

# 完整烧录（默认 115200 波特率，约 3 分钟）
esptool.py --chip esp32s3 --port COM3 --baud 115200 \
    --before default_reset --after hard_reset write_flash \
    0x0 firmware/bootloader.bin \
    0x8000 firmware/partition-table.bin \
    0x10000 firmware/MVP-W-S3.bin \
    0x410000 firmware/srmodels.bin \
    0x460000 firmware/storage.bin

# 重启设备
```

### 方法 3: 合并固件（单文件）

如果提供了 `MVP-W-S3-combined.bin`：

```bash
esptool.py --chip esp32s3 --port COM3 --baud 115200 \
    write_flash 0x0 MVP-W-S3-combined.bin
```

## 配置

### WiFi 配置

**当前版本 WiFi 为硬编码**：
- SSID: `TEST`
- 密码: `12345678`

**使用前请确保**：
1. 创建手机热点：SSID 为 `TEST`，密码 `12345678`
2. 或修改源码 `firmware/s3/main/wifi_client.c` 中的 `WIFI_SSID` 和 `WIFI_PASS` 后重新编译
3. 电脑服务器和 ESP32 必须在同一 WiFi 网络下

### 云端服务器

设备默认通过 UDP 服务发现自动寻找云端服务器。

启动云端服务器：
```bash
cd watcher-server
pip install -r requirements.txt
python src/main.py
```

## 验证

烧录完成后，验证功能：

1. **启动检查**
   - 按 RST 重启
   - 串口监视器（115200 8N1）应显示启动日志
   - 显示屏应显示启动动画

2. **唤醒词检测**
   - 说 "Hi 乐鑫"
   - 设备应响应并开始录音

3. **语音交互**
   - 长按按键 0.5s 开始录音
   - 松开按键结束录音
   - 等待 AI 回复

4. **动画显示**
   - 说话时显示 "listening" 表情
   - 回复时显示 "speaking" 表情

## 已知问题

- Opus 编解码未实现（使用 PCM 直传）
- 摄像头功能未集成
- OTA 升级未实现

## 变更日志

### v1.5.0 (2026-03-15)

**新增功能**
- 完整语音交互流程
- ESP-SR 唤醒词检测
- VAD 静音检测
- PNG 动画显示
- UDP 服务发现

**修复**
- 修复 TTS 后唤醒词不恢复的问题
- 修复 PNG 动画花屏/白屏问题
- 修复 WiFi DMA 与 SPI 冲突问题

## 故障排除

### 烧录失败

**问题**: `Failed to connect`
**解决**:
- 确保设备处于下载模式
- 尝试降低波特率: `-Baud 460800`
- 更换 USB 线/端口

### 启动失败

**问题**: 设备不断重启
**解决**:
- 使用 `-Full` 模式完整烧录
- 确保烧录了 `srmodels.bin` 和 `storage.bin`

### 唤醒词无响应

**问题**: 说 "Hi 乐鑫" 无反应
**解决**:
- 检查麦克风连接
- 确保 `srmodels.bin` 正确烧录
- 查看串口日志确认 AFE 初始化成功

### 动画不显示

**问题**: 屏幕黑屏或白屏
**解决**:
- 确保 `storage.bin` 正确烧录
- 检查 SPIFFS 分区是否正确挂载

## 下一步

- [ ] 配置云端服务器 API Keys
- [ ] 连接 MCU 舵机控制器
- [ ] 自定义唤醒词

## 反馈

如有问题，请在项目 Issues 中反馈。
