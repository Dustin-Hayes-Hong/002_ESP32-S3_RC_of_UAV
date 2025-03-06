#include "touch_spi.h"
#include <stdint.h>
#include "esp_log.h"        // ESP-IDF日志库
#include "freertos/FreeRTOS.h" // FreeRTOS支持
#include "freertos/task.h"  // FreeRTOS任务支持

// 日志标签
static const char *TAG = "TOUCH_SPI";

// SPI设备句柄
static spi_device_handle_t spi;

/**
 * @brief 初始化触摸屏SPI总线和设备
 */
void tp_spi_init(void)
{
    esp_err_t ret;

    ESP_LOGI(TAG, "配置触摸屏SPI总线");

    // SPI总线配置
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = TP_SPI_MOSI,      // MOSI引脚
        .miso_io_num = TP_SPI_MISO,      // MISO引脚
        .sclk_io_num = TP_SPI_CLK,       // 时钟引脚
        .quadwp_io_num = -1,             // 不使用Quad WP
        .quadhd_io_num = -1,             // 不使用Quad HD
        .max_transfer_sz = 0,            // 默认传输大小（触摸屏无需大缓冲区）
    };

    ESP_LOGI(TAG, "初始化触摸屏SPI总线...");
    ret = spi_bus_initialize(TP_SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI总线初始化失败: %s", esp_err_to_name(ret));
        return;
    }

    // 添加SPI设备
    tp_spi_add_device(TP_SPI_HOST);
    ESP_LOGI(TAG, "触摸屏SPI初始化完成");
}

/**
 * @brief 配置并添加SPI设备
 * @param host SPI主机设备
 * @param devcfg SPI设备接口配置结构体指针
 */
void tp_spi_add_device_config(spi_host_device_t host, spi_device_interface_config_t *devcfg)
{
    esp_err_t ret = spi_bus_add_device(host, devcfg, &spi);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "添加SPI设备失败: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "SPI设备添加成功");
}

/**
 * @brief 添加触摸屏SPI设备
 * @param host SPI主机设备
 */
void tp_spi_add_device(spi_host_device_t host)
{
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = SPI_TOUCH_CLOCK_SPEED_HZ, // 时钟频率
        .mode = SPI_TOUCH_SPI_MODE,                // SPI模式
        .spics_io_num = TP_SPI_CS,                 // 片选引脚
        .queue_size = 1,                           // 队列大小（触摸屏通常单次传输）
        .pre_cb = NULL,                            // 无前回调
        .post_cb = NULL,                           // 无后回调
        .command_bits = 8,                         // 命令位数
        .address_bits = 0,                         // 无地址位
        .dummy_bits = 0,                           // 无哑位
        .flags =  SPI_DEVICE_NO_DUMMY, // 无哑位
    };

    tp_spi_add_device_config(host, &devcfg);
    ESP_LOGI(TAG, "触摸屏SPI设备初始化完成");
}

/**
 * @brief SPI数据交换
 * @param data_send 发送数据缓冲区
 * @param data_recv 接收数据缓冲区
 * @param byte_count 数据字节数
 */
void tp_spi_xchg(uint8_t* data_send, uint8_t* data_recv, uint8_t byte_count)
{
	spi_transaction_t t = {
		.length = byte_count * 8, // SPI transaction length is in bits
		.tx_buffer = data_send,
		.rx_buffer = data_recv};
	
	esp_err_t ret = spi_device_transmit(spi, &t);
	assert(ret == ESP_OK);
}

/**
 * @brief 写触摸屏寄存器
 * @param data 数据缓冲区
 * @param byte_count 数据字节数
 */
void tp_spi_write_reg(uint8_t *data, uint8_t byte_count)
{
    spi_transaction_t t = {
        .length = byte_count * 8,    // 传输长度（位）
        .tx_buffer = data,           // 发送缓冲区
        .flags = 0                   // 无特殊标志
    };

    esp_err_t ret = spi_device_transmit(spi, &t);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "写寄存器失败: %s", esp_err_to_name(ret));
    }
}

/**
 * @brief 读触摸屏寄存器
 * @param reg 寄存器命令
 * @param data 接收数据缓冲区
 * @param byte_count 数据字节数
 */
void tp_spi_read_reg(uint8_t reg, uint8_t *data, uint8_t byte_count)
{
    spi_transaction_t t = {
        .length = (byte_count + sizeof(reg)) * 8,  // 总长度（命令+数据）
        .rxlength = byte_count * 8,      // 接收长度（仅数据部分）
        .cmd = reg,                      // 命令字节
        .rx_buffer = data,               // 接收缓冲区
        .flags = 0                       // 无特殊标志
    };

    esp_err_t ret = spi_device_transmit(spi, &t);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "读寄存器失败: %s", esp_err_to_name(ret));
    }
}