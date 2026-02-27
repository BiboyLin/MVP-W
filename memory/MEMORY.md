# MVP-W 项目记忆

## ESP-IDF 开发常见错误

### 1. SPI 驱动头文件缺失
**错误**:
```
error: unknown type name 'spi_bus_config_t'
error: implicit declaration of function 'spi_bus_initialize'
error: 'SPI3_HOST' undeclared
error: 'SPI_DMA_CH_AUTO' undeclared
```

**解决**: 必须包含 `driver/spi_master.h` 头文件：
```c
#include "driver/spi_master.h"
```

### 2. ESP_RETURN_ON_ERROR 未定义
**错误**:
```
error: implicit declaration of function 'ESP_RETURN_ON_ERROR'
```

**解决**: 包含 `esp_check.h` 头文件：
```c
#include "esp_check.h"
```

### 3. 分区表配置
**问题**: 默认分区表只有 1MB，应用超过大小会报错。

**解决**: 创建 `partitions.csv` 并在 `sdkconfig.defaults` 中配置：
```
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"
CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y
CONFIG_ESPTOOLPY_FLASHSIZE="16MB"
```

**注意**: 删除旧的 `sdkconfig` 文件后重新配置才能生效。

### 4. 组件管理器依赖
**问题**: LVGL 组件名称是 `lvgl/lvgl`，不是 `espressif/lvgl`。

**正确配置** (`idf_component.yml`):
```yaml
dependencies:
  lvgl/lvgl:
    version: "==8.3.9"
  espressif/esp_lcd_spd2010:
    version: "^1.0.0"
```

### 5. SPD2010 LCD 驱动
使用专用宏配置 QSPI：
```c
#include "driver/spi_master.h"
#include "esp_lcd_spd2010.h"

const spi_bus_config_t buscfg = SPD2010_PANEL_BUS_QSPI_CONFIG(
    LCD_SPI_PCLK,
    LCD_SPI_DATA0,
    LCD_SPI_DATA1,
    LCD_SPI_DATA2,
    LCD_SPI_DATA3,
    max_transfer_size
);

const esp_lcd_panel_io_spi_config_t io_config = SPD2010_PANEL_IO_QSPI_CONFIG(
    LCD_SPI_CS,
    callback,
    user_ctx
);
```

## 硬件配置

### Watcher ESP32-S3 GPIO
| 功能 | GPIO |
|------|------|
| LCD QSPI PCLK | 7 |
| LCD QSPI DATA0 | 9 |
| LCD QSPI DATA1 | 1 |
| LCD QSPI DATA2 | 14 |
| LCD QSPI DATA3 | 13 |
| LCD CS | 45 |
| LCD Backlight | 8 |
| UART TX (→MCU) | 19 |
| UART RX (←MCU) | 20 |
| I2S MCLK | 10 |
| I2S BCLK | 11 |
| I2S LRCK | 12 |
| I2S DOUT (Speaker) | 15 |
| I2S DIN (Mic) | 16 |

### 编码器与按钮 (重要!)
| 功能 | GPIO/引脚 | 说明 |
|------|----------|------|
| **编码器 A 相** | GPIO 41 | 旋转检测 |
| **编码器 B 相** | GPIO 42 | 旋转检测 |
| **按钮** | IO_EXPANDER_PIN_3 | **通过 I2C IO 扩展器!** |

**警告**: GPIO 41/42 是编码器旋转引脚，不是按钮！
按钮连接在 PCA9535 IO 扩展器上 (I2C 地址 0x24, SDA=47, SCL=48)。

### I2C 端口配置
| I2C | 用途 | SDA | SCL |
|-----|------|-----|-----|
| **I2C_NUM_0** | General (IO 扩展器) | 47 | 48 |
| **I2C_NUM_1** | Touch | 39 | 38 |

### 6. LVGL 线程安全
**错误**: 在 ISR 中调用 `display_update()` 导致崩溃
```
abort() was called at PC 0x40376f7f
```

**原因**: LVGL 不是线程安全的，不能在中断上下文中调用。

**解决**:
- 在任务上下文中轮询按钮状态
- 使用队列将事件从 ISR 传递到任务
- 所有 LVGL 操作必须在任务中执行

---
*更新时间: 2026-02-27*
