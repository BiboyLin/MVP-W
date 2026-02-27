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

---
*更新时间: 2026-02-27*
