#include "disp_spi.h"
#include "esp_log.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// SPI 事务池配置
#define SPI_TRANSACTION_POOL_SIZE 50
#define SPI_TRANSACTION_POOL_RESERVE_PERCENTAGE 10
#define SPI_TRANSACTION_POOL_RESERVE (SPI_TRANSACTION_POOL_SIZE / SPI_TRANSACTION_POOL_RESERVE_PERCENTAGE)

// 全局 SPI 设备句柄
spi_device_handle_t disp_spi_handle;
static QueueHandle_t TransactionPool = NULL;

static void disp_spi_pre_transfer_callback(spi_transaction_t *t);

/**
 * @brief 初始化 SPI 总线和设备
 */
void disp_spi_init_config(void) {
    esp_err_t ret;

    // SPI 总线配置
    static const spi_bus_config_t bus_cfg = {
        .mosi_io_num = LCD_SPI_MOSI,
        .sclk_io_num = LCD_SPI_SCLK,
        .quadhd_io_num = -1,
        .quadwp_io_num = -1,
        .max_transfer_sz = PARALLEL_LINES * 320 * 2 + 8,
    };

    // SPI 设备配置
    static const spi_device_interface_config_t dev_cfg = {
        .clock_speed_hz = 10 * 1000 * 1000,  // 10 MHz
        .mode = 0,                           // SPI 模式 0
        .spics_io_num = LCD_SPI_CS,
        .queue_size = 7,
        .pre_cb = disp_spi_pre_transfer_callback,
    };

    // 初始化 SPI 总线和设备
    ret = spi_bus_initialize(LCD_SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    ESP_ERROR_CHECK(ret);
    ret = spi_bus_add_device(LCD_SPI_HOST, &dev_cfg, &disp_spi_handle);
    ESP_ERROR_CHECK(ret);

    // 创建事务池
    TransactionPool = xQueueCreate(SPI_TRANSACTION_POOL_SIZE, sizeof(spi_transaction_ext_t *));
    if (TransactionPool == NULL) {
        ESP_LOGE("DISP_SPI", "Failed to create transaction pool");
        return;
    }
    for (int i = 0; i < SPI_TRANSACTION_POOL_SIZE; i++) {
        spi_transaction_ext_t *trans = heap_caps_malloc(sizeof(spi_transaction_ext_t), MALLOC_CAP_DMA);
        if (trans) {
            xQueueSend(TransactionPool, &trans, portMAX_DELAY);
        }
    }
}

/**
 * @brief SPI 传输前回调，设置 DC 引脚电平
 * @param t SPI 事务指针
 */
static void disp_spi_pre_transfer_callback(spi_transaction_t *t) {
    int dc = (int)t->user;
    gpio_set_level(LCD_SPI_DC, dc);
}

/**
 * @brief 等待所有挂起的事务完成
 */
void disp_wait_for_pending_transactions(void) {
    spi_transaction_t *presult;
    while (uxQueueMessagesWaiting(TransactionPool) < SPI_TRANSACTION_POOL_SIZE) {
        if (spi_device_get_trans_result(disp_spi_handle, &presult, 1) == ESP_OK) {
            xQueueSend(TransactionPool, &presult, portMAX_DELAY);
        }
    }
}

/**
 * @brief 执行 SPI 数据传输
 * @param data 数据指针
 * @param length 数据长度（字节）
 * @param flags 传输标志
 * @param out 接收数据缓冲区
 * @param addr 地址（可选）
 * @param dummy_bits 哑位数（可选）
 */
static void disp_spi_transaction(const uint8_t *data, size_t length, disp_spi_send_flag_t flags,
                                uint8_t *out, uint64_t addr, uint8_t dummy_bits) {
    if (length == 0) return;

    spi_transaction_ext_t t = {
        .base = {
            .length = length * 8,  // 转换为位长度
            .tx_buffer = (length <= 4 && data) ? NULL : data,
            .flags = (length <= 4 && data) ? SPI_TRANS_USE_TXDATA : 0,
            .user = (void *)flags,
        },
    };

    if (t.base.flags & SPI_TRANS_USE_TXDATA) {
        memcpy(t.base.tx_data, data, length);
    }

    if (flags & DISP_SPI_RECEIVE) {
        t.base.rx_buffer = out;
        t.base.rxlength = t.base.length;
    }

    if (flags & (DISP_SPI_ADDRESS_8 | DISP_SPI_ADDRESS_16 | DISP_SPI_ADDRESS_24 | DISP_SPI_ADDRESS_32)) {
        t.address_bits = (flags & DISP_SPI_ADDRESS_8) ? 8 : (flags & DISP_SPI_ADDRESS_16) ? 16 :
                         (flags & DISP_SPI_ADDRESS_24) ? 24 : 32;
        t.base.addr = addr;
        t.base.flags |= SPI_TRANS_VARIABLE_ADDR;
    }

    if (flags & DISP_SPI_VARIABLE_DUMMY) {
        t.dummy_bits = dummy_bits;
        t.base.flags |= SPI_TRANS_VARIABLE_DUMMY;
    }

    if (flags & DISP_SPI_SEND_POLLING) {
        disp_wait_for_pending_transactions();
        spi_device_polling_transmit(disp_spi_handle, (spi_transaction_t *)&t);
    } else if (flags & DISP_SPI_SEND_SYNCHRONOUS) {
        disp_wait_for_pending_transactions();
        spi_device_transmit(disp_spi_handle, (spi_transaction_t *)&t);
    } else {
        if (uxQueueMessagesWaiting(TransactionPool) == 0) {
            spi_transaction_t *presult;
            while (uxQueueMessagesWaiting(TransactionPool) < SPI_TRANSACTION_POOL_RESERVE) {
                if (spi_device_get_trans_result(disp_spi_handle, &presult, 1) == ESP_OK) {
                    xQueueSend(TransactionPool, &presult, portMAX_DELAY);
                }
            }
        }
        spi_transaction_ext_t *pTransaction;
        xQueueReceive(TransactionPool, &pTransaction, portMAX_DELAY);
        *pTransaction = t;
        if (spi_device_queue_trans(disp_spi_handle, (spi_transaction_t *)pTransaction, portMAX_DELAY) != ESP_OK) {
            xQueueSend(TransactionPool, &pTransaction, portMAX_DELAY);
        }
    }
}

/**
 * @brief 发送数据（轮询模式）
 * @param data 数据指针
 * @param length 数据长度
 */
void disp_spi_send_data(uint8_t *data, size_t length) {
    disp_spi_transaction(data, length, DISP_SPI_SEND_POLLING, NULL, 0, 0);
}

/**
 * @brief 发送颜色数据（队列模式）
 * @param data 数据指针
 * @param length 数据长度
 */
void disp_spi_send_colors(uint8_t *data, size_t length) {
    disp_spi_transaction(data, length, DISP_SPI_SEND_QUEUED | DISP_SPI_SIGNAL_FLUSH, NULL, 0, 0);
}