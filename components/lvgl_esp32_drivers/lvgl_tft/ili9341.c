#include "ili9341.h"
#include "disp_spi.h"

void lcd_init(spi_device_handle_t handle)
{
    int cmd = 0;
    const lcd_init_cmd_t* lcd_init_cmds;

    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = ((1ULL << LCD_SPI_DC) | (1ULL << LCD_SPI_RST) | (1ULL << LCD_SPI_BCKL));
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_up_en = true;
    gpio_config(&io_conf);

    gpio_set_level(LCD_SPI_RST, 0);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    gpio_set_level(LCD_SPI_RST, 1);
    vTaskDelay(100 / portTICK_PERIOD_MS);

    uint32_t lcd_id = lcd_get_id(handle);
    int lcd_detected_type = 0;
    int lcd_type = 0;

    printf("LCD ID: %08"PRIx32"\n", lcd_id);
    if (lcd_id == 0) {
        lcd_detected_type = LCD_TYPE_ILI;
        printf("ILI9341 detected.\n");
    } else {
        lcd_detected_type = LCD_TYPE_ST;
        printf("ST7789V detected.\n");
    }

    lcd_type = lcd_detected_type;

    if(lcd_type == LCD_TYPE_ILI)
    printf("LCD ILI9341 initialization.\n");
    lcd_init_cmds = ili_init_cmds;

    while (lcd_init_cmds[cmd].databytes != 0xff) {
        lcd_cmd(handle, lcd_init_cmds[cmd].cmd, false);
        lcd_data(handle, lcd_init_cmds[cmd].data, lcd_init_cmds[cmd].databytes & 0x1F);
        if (lcd_init_cmds[cmd].databytes & 0x80) {
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }
        cmd++;
    }

    gpio_set_level(LCD_SPI_BCKL, LCD_BK_LIGHT_ON_LEVEL);
}

void lcd_cmd(spi_device_handle_t handle, const uint8_t cmd, bool keep_cs_active)
{
    esp_err_t ret;
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));       //Zero out the transaction
    t.length = 8;                   //Command is 8 bits
    t.tx_buffer = &cmd;             //The data is the cmd itself
    t.user = (void*)0;              //D/C needs to be set to 0
    if (keep_cs_active) {
        t.flags = SPI_TRANS_CS_KEEP_ACTIVE;   //Keep CS active after data transfer
    }
    ret = spi_device_polling_transmit(handle, &t); //Transmit!
    assert(ret == ESP_OK);          //Should have had no issues.
}

void lcd_data(spi_device_handle_t handle, const uint8_t *data, int len)
{
    esp_err_t ret;
    spi_transaction_t t;
    if (len == 0) {
        return;    //no need to send anything
    }
    memset(&t, 0, sizeof(t));       //Zero out the transaction
    t.length = len * 8;             //Len is in bytes, transaction length is in bits.
    t.tx_buffer = data;             //Data
    t.user = (void*)1;              //D/C needs to be set to 1
    ret = spi_device_polling_transmit(handle, &t); //Transmit!
    assert(ret == ESP_OK);          //Should have had no issues.
}

uint32_t lcd_get_id(spi_device_handle_t handle)
{
    // When using SPI_TRANS_CS_KEEP_ACTIVE, bus must be locked/acquired
    spi_device_acquire_bus(handle, portMAX_DELAY);

    //get_id cmd
    lcd_cmd(handle, 0x04, true);

    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = 8 * 3;
    t.flags = SPI_TRANS_USE_RXDATA;
    t.user = (void*)1;

    esp_err_t ret = spi_device_polling_transmit(handle, &t);
    assert(ret == ESP_OK);

    // Release bus
    spi_device_release_bus(handle);

    return *(uint32_t*)t.rx_data;
}


