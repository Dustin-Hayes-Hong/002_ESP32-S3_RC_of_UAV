#ifndef __DISP_SPI_H
#define __DISP_SPI_H

#include <stdint.h>
#include "driver/spi_master.h"
#include "driver/gpio.h"

#define LCD_SPI_HOST       SPI2_HOST
#define LCD_SPI_MOSI       35
#define LCD_SPI_SCLK       32
#define LCD_SPI_CS         34
#define LCD_SPI_DC         33
#define LCD_SPI_RST        31
#define LCD_BK_LIGHT_ON_LEVEL 0
#define PARALLEL_LINES     16

typedef enum {
    DISP_SPI_SEND_QUEUED        = 0x00000000,
    DISP_SPI_SEND_POLLING       = 0x00000001,
    DISP_SPI_SEND_SYNCHRONOUS   = 0x00000002,
    DISP_SPI_SIGNAL_FLUSH       = 0x00000004,
    DISP_SPI_RECEIVE            = 0x00000008,
    DISP_SPI_ADDRESS_8          = 0x00000040,
    DISP_SPI_ADDRESS_16         = 0x00000080,
    DISP_SPI_ADDRESS_24         = 0x00000100,
    DISP_SPI_ADDRESS_32         = 0x00000200,
    DISP_SPI_MODE_DIO           = 0x00000400,
    DISP_SPI_MODE_QIO           = 0x00000800,
    DISP_SPI_MODE_DIOQIO_ADDR   = 0x00001000,
    DISP_SPI_VARIABLE_DUMMY     = 0x00002000,
} disp_spi_send_flag_t;

extern spi_device_handle_t disp_spi_handle;

void disp_spi_init_config(void);
void disp_spi_send_data(uint8_t *data, size_t length);
void disp_spi_send_colors(uint8_t *data, size_t length);
void disp_wait_for_pending_transactions(void);

#endif /* __DISP_SPI_H */