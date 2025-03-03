#ifndef __TOUCH_SPI_H
#define __TOUCH_SPI_H

#include <stdint.h>
#include "driver/spi_master.h"

#define SPI_TOUCH_CLOCK_SPEED_HZ    (2 * 1000 * 1000)
#define SPI_TOUCH_SPI_MODE          (0)
#define TP_SPI_CS                   21
#define TP_SPI_CLK                  20
#define TP_SPI_MOSI                 22
#define TP_SPI_MISO                 23
#define TP_SPI_IRQ                  24

void tp_spi_add_device_config(spi_host_device_t host, spi_device_interface_config_t *devcfg);
void tp_spi_add_device(spi_host_device_t host);
void tp_spi_xchg(uint8_t *data_send, uint8_t *data_recv, uint8_t byte_count);
void tp_spi_write_reg(uint8_t *data, uint8_t byte_count);
void tp_spi_read_reg(uint8_t reg, uint8_t *data, uint8_t byte_count);

#endif /* __TOUCH_SPI_H */