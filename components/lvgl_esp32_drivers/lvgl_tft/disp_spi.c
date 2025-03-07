#include "disp_spi.h"
#include <stdint.h>
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// https://docs.espressif.com/projects/esp-idf/zh_CN/v5.4/esp32/api-reference/peripherals/spi_master.html#id11

// 日志标签
static const char *TAG = "DISP_SPI";

// 全局变量
static spi_device_handle_t spi;                   // SPI设备句柄
static QueueHandle_t TransactionPool = NULL;      // 事务池队列

/**
 * @brief 初始化SPI总线和设备
 */
void disp_spi_init(void)
{
    ESP_LOGI(TAG, "显示缓冲区大小: %d", DISP_BUF_SIZE);
    ESP_LOGI(TAG, "初始化SPI主机用于显示");

    // SPI总线配置
    static const spi_bus_config_t bus_cfg = {
        .mosi_io_num = LCD_SPI_MOSI,             // MOSI引脚
        .miso_io_num = -1,                       // 不使用MISO
        .sclk_io_num = LCD_SPI_SCLK,             // 时钟引脚
        .quadwp_io_num = -1,                     // 不使用Quad WP
        .quadhd_io_num = -1,                     // 不使用Quad HD
        .max_transfer_sz = SPI_BUS_MAX_TRANSFER_SZ, // 最大传输大小
    };

    ESP_LOGI(TAG, "初始化SPI总线...");
    esp_err_t ret = spi_bus_initialize(LCD_SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI总线初始化失败: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "SPI总线初始化成功");

    ESP_LOGI(TAG, "添加SPI设备...");
    // SPI设备配置
    static spi_device_interface_config_t dev_cfg = {
        .clock_speed_hz = 20 * 1000 * 1000,     // 时钟频率40MHz
        .mode = LCD_SPI_MODE,                               // SPI模式0
        .spics_io_num = LCD_SPI_CS,              // 片选引脚
        .queue_size = SPI_TRANSACTION_POOL_SIZE, // 队列大小
        .pre_cb = NULL, // 前回调函数
    };

    spi_bus_add_device(LCD_SPI_HOST,&dev_cfg,&spi);

    // 创建事务池
    TransactionPool = xQueueCreate(SPI_TRANSACTION_POOL_SIZE, sizeof(spi_transaction_ext_t *));
    if (TransactionPool == NULL) {
        ESP_LOGE(TAG, "事务池创建失败");
        return;
    }

    // 预分配事务池内存
    for (int i = 0; i < SPI_TRANSACTION_POOL_SIZE; i++) {
        spi_transaction_ext_t *trans = heap_caps_malloc(sizeof(spi_transaction_ext_t), MALLOC_CAP_DMA);
        if (trans == NULL) {
            ESP_LOGE(TAG, "事务池内存分配失败（第%d个）", i);
            continue;                           // 继续分配剩余部分
        }
        memset(trans, 0, sizeof(spi_transaction_ext_t));
        xQueueSend(TransactionPool, &trans, portMAX_DELAY);
    }
    ESP_LOGI(TAG, "SPI初始化完成");
}

/**
 * @brief 等待所有挂起的事务完成
 */
void disp_wait_for_pending_transactions(void)
{
    spi_transaction_t *presult;
    while (uxQueueMessagesWaiting(TransactionPool) < SPI_TRANSACTION_POOL_SIZE) {
        if (spi_device_get_trans_result(spi, &presult, 1) == ESP_OK) {
            xQueueSend(TransactionPool, &presult, portMAX_DELAY);
        }
    }
}

/**
 * @brief 执行SPI数据传输
 * @param data 发送数据缓冲区
 * @param length 数据长度（字节）
 * @param flags 传输标志
 * @param out 接收数据缓冲区（可选）
 * @param addr 地址值（可选）
 * @param dummy_bits 哑位数（可选）
 */
static void disp_spi_transaction(const uint8_t *data, size_t length, disp_spi_send_flag_t flags,
                                 uint8_t *out, uint64_t addr, uint8_t dummy_bits)
{
    if (length == 0) return;                     // 空数据直接返回

    // 初始化事务结构体
    spi_transaction_ext_t t = {
        .base = {
            .length = length * 8,               // 长度以位为单位
            .tx_buffer = (length <= 4 && data) ? NULL : data, // 小数据使用tx_data
            .flags = (length <= 4 && data) ? SPI_TRANS_USE_TXDATA : 0, // 小数据标志
            .user = (void *)flags,              // 用户数据保存标志
        },
    };

    if (t.base.flags & SPI_TRANS_USE_TXDATA) {
        memcpy(t.base.tx_data, data, length);   // 复制小数据到tx_data
    }

    if (flags & DISP_SPI_RECEIVE) {
        t.base.rx_buffer = out;                 // 设置接收缓冲区
        t.base.rxlength = t.base.length;        // 接收长度
    }

    if (flags & (DISP_SPI_ADDRESS_8 | DISP_SPI_ADDRESS_16 | DISP_SPI_ADDRESS_24 | DISP_SPI_ADDRESS_32)) {
        t.address_bits = (flags & DISP_SPI_ADDRESS_8) ? 8 :
                         (flags & DISP_SPI_ADDRESS_16) ? 16 :
                         (flags & DISP_SPI_ADDRESS_24) ? 24 : 32; // 设置地址位数
        t.base.addr = addr;                     // 设置地址
        t.base.flags |= SPI_TRANS_VARIABLE_ADDR; // 可变地址标志
    }

    if (flags & DISP_SPI_VARIABLE_DUMMY) {
        t.dummy_bits = dummy_bits;              // 设置哑位
        t.base.flags |= SPI_TRANS_VARIABLE_DUMMY; // 可变哑位标志
    }

    // 根据标志选择传输模式
    if (flags & DISP_SPI_SEND_POLLING) {
        disp_wait_for_pending_transactions();   // 等待挂起事务
        spi_device_polling_transmit(spi, (spi_transaction_t *)&t);
    } else if (flags & DISP_SPI_SEND_SYNCHRONOUS) {
        disp_wait_for_pending_transactions();   // 等待挂起事务
        spi_device_transmit(spi, (spi_transaction_t *)&t);
    } else {
        // 队列模式传输
        if (uxQueueMessagesWaiting(TransactionPool) == 0) {
            spi_transaction_t *presult;
            while (uxQueueMessagesWaiting(TransactionPool) < SPI_TRANSACTION_POOL_RESERVE) {
                if (spi_device_get_trans_result(spi, &presult, 1) == ESP_OK) {
                    xQueueSend(TransactionPool, &presult, portMAX_DELAY);
                }
            }
        }
        spi_transaction_ext_t *pTransaction;
        if (xQueueReceive(TransactionPool, &pTransaction, portMAX_DELAY) != pdTRUE) {
            ESP_LOGE(TAG, "从事务池获取事务失败");
            return;
        }
        *pTransaction = t;
        if (spi_device_queue_trans(spi, (spi_transaction_t *)pTransaction, portMAX_DELAY) != ESP_OK) {
            xQueueSend(TransactionPool, &pTransaction, portMAX_DELAY); // 失败时归还事务
            ESP_LOGE(TAG, "SPI事务队列传输失败");
        }
    }
}

/**
 * @brief 使用轮询模式发送数据
 * @param data 数据缓冲区
 * @param length 数据长度（字节）
 */
void disp_spi_send_data(uint8_t *data, size_t length)
{
    disp_spi_transaction(data, length, DISP_SPI_SEND_POLLING, NULL, 0, 0);
}

/**
 * @brief 使用队列模式发送颜色数据
 * @param data 颜色数据缓冲区
 * @param length 数据长度（字节）
 */
void disp_spi_send_colors(uint8_t *data, size_t length)
{
    disp_spi_transaction(data, length, DISP_SPI_SEND_QUEUED | DISP_SPI_SIGNAL_FLUSH, NULL, 0, 0);
}