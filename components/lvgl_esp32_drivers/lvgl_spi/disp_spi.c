#include "disp_spi.h"
#include "ili9341.h"

spi_device_handle_t disp_spi_handle;

void disp_spi_init(void)
{
    disp_spi_init_config(disp_spi_handle);
    lcd_init(disp_spi_handle);
}

void disp_spi_init_config(spi_device_handle_t handle)
{

    esp_err_t ret;
    static spi_bus_config_t bus_cfg = {
        .miso_io_num = LCD_SPI_MISO,
        .mosi_io_num = LCD_SPI_MOSI,
        .sclk_io_num = LCD_SPI_SCLK,
        .quadhd_io_num = -1,
        .quadwp_io_num = -1,
        .max_transfer_sz = PARALLEL_LINES *320 *2 + 8
    };

    spi_device_interface_config_t dev_cfg = {
        .clock_speed_hz = 10 * 1000 * 1000,
        .mode = 0,
        .spics_io_num = LCD_SPI_CS,
        .queue_size = 7,
        .pre_cb = disp_spi_pre_transfer_callback,
    };

    ret = spi_bus_initialize(LCD_SPI_HOST,&bus_cfg,SPI_DMA_CH_AUTO);
    ESP_ERROR_CHECK(ret);
    ret = spi_bus_add_device(LCD_SPI_HOST, &dev_cfg, &handle);
    ESP_ERROR_CHECK(ret);
}

void disp_spi_pre_transfer_callback(spi_transaction_t *t)
{
    int dc = (int)t->user;
    gpio_set_level(LCD_SPI_DC,dc);
}

