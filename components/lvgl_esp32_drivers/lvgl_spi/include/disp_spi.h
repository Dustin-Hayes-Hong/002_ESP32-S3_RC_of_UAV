#ifndef __DISP_SPI
#define __DISP_SPI

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"


#define LCD_SPI_HOST							SPI2_HOST

#define LCD_SPI_MISO							GPIO_NUM_12  	
#define LCD_SPI_MOSI							GPIO_NUM_13	
#define LCD_SPI_SCLK							GPIO_NUM_14	
#define LCD_SPI_CS							    GPIO_NUM_15		

#define LCD_SPI_DC                             21
#define LCD_SPI_RST                            18
#define LCD_SPI_BCKL                           5

#define LCD_BK_LIGHT_ON_LEVEL					0

#define PARALLEL_LINES                          16

void disp_spi_init_config(spi_device_handle_t handle);
void disp_spi_pre_transfer_callback(spi_transaction_t *t);


extern void disp_spi_init(void);

#endif  //__DISP_SPI