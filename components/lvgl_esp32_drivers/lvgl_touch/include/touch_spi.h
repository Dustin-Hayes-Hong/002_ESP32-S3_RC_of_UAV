#include <stdint.h>
#include "driver/spi_master.h"  // SPI 相关功能需要
#include "driver/gpio.h"       // GPIO 配置需要

#define SPI_TOUCH_CLOCK_SPEED_HZ    (2*1000*1000)
#define SPI_TOUCH_SPI_MODE          (0)

#define TP_SPI_CS                   21
#define TP_SPI_CLK                  20
#define TP_SPI_MOSI                 22
#define TP_SPI_MISO                 23
#define TP_SPI_IRQ                  24

void tp_spi_read_reg(uint8_t reg, uint8_t* data, uint8_t byte_count);