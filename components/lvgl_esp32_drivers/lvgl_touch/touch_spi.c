#include "touch_spi.h"
#include <stdint.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "TOUCH_SPI"

static spi_device_handle_t spi;

// 函数声明
void tp_spi_add_device_config(spi_host_device_t host, spi_device_interface_config_t *devcfg);
void tp_spi_add_device(spi_host_device_t host);
void tp_spi_xchg(uint8_t *data_send, uint8_t *data_recv, uint8_t byte_count);
void tp_spi_write_reg(uint8_t *data, uint8_t byte_count);
void tp_spi_read_reg(uint8_t reg, uint8_t *data, uint8_t byte_count);

/**
 * @brief 配置并添加SPI设备
 */
void tp_spi_add_device_config(spi_host_device_t host, spi_device_interface_config_t *devcfg)
{
    esp_err_t ret = spi_bus_add_device(host, devcfg, &spi);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "添加SPI设备失败: %s", esp_err_to_name(ret));
    }
    ESP_LOGI(TAG, "SPI设备添加成功");
}

/**
 * @brief 添加触摸屏SPI设备
 */
void tp_spi_add_device(spi_host_device_t host)
{
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = SPI_TOUCH_CLOCK_SPEED_HZ,
        .mode = SPI_TOUCH_SPI_MODE,
        .spics_io_num = TP_SPI_CS,
        .queue_size = 1,
        .pre_cb = NULL,
        .post_cb = NULL,
        .command_bits = 8,
        .address_bits = 0,
        .dummy_bits = 0,
        .flags = SPI_DEVICE_HALFDUPLEX | SPI_DEVICE_NO_DUMMY,
    };

    tp_spi_add_device_config(host, &devcfg);
    ESP_LOGI(TAG, "触摸屏SPI设备初始化完成");
}

/**
 * @brief SPI数据交换
 */
void tp_spi_xchg(uint8_t *data_send, uint8_t *data_recv, uint8_t byte_count)
{
    spi_transaction_t t = {
        .length = byte_count * 8,
        .tx_buffer = data_send,
        .rx_buffer = data_recv
    };

    esp_err_t ret = spi_device_transmit(spi, &t);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI数据交换失败: %s", esp_err_to_name(ret));
    }
}

/**
 * @brief 写触摸屏寄存器
 */
void tp_spi_write_reg(uint8_t *data, uint8_t byte_count)
{
    spi_transaction_t t = {
        .length = byte_count * 8,
        .tx_buffer = data,
        .flags = 0
    };

    esp_err_t ret = spi_device_transmit(spi, &t);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "写寄存器失败: %s", esp_err_to_name(ret));
    }
}

/**
 * @brief 读触摸屏寄存器
 */
void tp_spi_read_reg(uint8_t reg, uint8_t *data, uint8_t byte_count)
{
    spi_transaction_t t = {
        .length = (byte_count + sizeof(reg)) * 8,
        .rxlength = byte_count * 8,
        .cmd = reg,
        .rx_buffer = data,
        .flags = 0
    };

    esp_err_t ret = spi_device_transmit(spi, &t);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "读寄存器失败: %s", esp_err_to_name(ret));
    }
}