#include "disp_spi.h"
#include <stdint.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include <string.h>
#include "freertos/task.h"

#define TAG "DISP_SPI"

#define SPI_TRANSACTION_POOL_SIZE 50
#define SPI_TRANSACTION_POOL_RESERVE_PERCENTAGE 10
#define SPI_TRANSACTION_POOL_RESERVE (SPI_TRANSACTION_POOL_SIZE / SPI_TRANSACTION_POOL_RESERVE_PERCENTAGE)

spi_device_handle_t disp_spi_handle;
static QueueHandle_t TransactionPool = NULL;

// 函数声明
static void disp_spi_pre_transfer_callback(spi_transaction_t *t);
static void disp_spi_transaction(const uint8_t *data, size_t length, disp_spi_send_flag_t flags,
                                 uint8_t *out, uint64_t addr, uint8_t dummy_bits);

/**
 * @brief SPI传输前回调
 */
static void disp_spi_pre_transfer_callback(spi_transaction_t *t)
{
    int dc = (int)t->user;
    gpio_set_level(LCD_SPI_DC, dc);
}

/**
 * @brief 初始化SPI总线和设备
 */
void disp_spi_init_config(void)
{
    esp_err_t ret;

    static const spi_bus_config_t bus_cfg = {
        .mosi_io_num = LCD_SPI_MOSI,
        .miso_io_num = -1,
        .sclk_io_num = LCD_SPI_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = PARALLEL_LINES * 320 * 2 + 8,
    };

    static const spi_device_interface_config_t dev_cfg = {
        .clock_speed_hz = 10 * 1000 * 1000,
        .mode = 0,
        .spics_io_num = LCD_SPI_CS,
        .queue_size = 7,
        .pre_cb = disp_spi_pre_transfer_callback,
    };

    ret = spi_bus_initialize(LCD_SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI总线初始化失败: %s", esp_err_to_name(ret));
        return;
    }

    ret = spi_bus_add_device(LCD_SPI_HOST, &dev_cfg, &disp_spi_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI设备添加失败: %s", esp_err_to_name(ret));
        return;
    }

    TransactionPool = xQueueCreate(SPI_TRANSACTION_POOL_SIZE, sizeof(spi_transaction_ext_t *));
    if (TransactionPool == NULL) {
        ESP_LOGE(TAG, "事务池创建失败");
        return;
    }

    for (int i = 0; i < SPI_TRANSACTION_POOL_SIZE; i++) {
        spi_transaction_ext_t *trans = heap_caps_malloc(sizeof(spi_transaction_ext_t), MALLOC_CAP_DMA);
        if (trans) {
            xQueueSend(TransactionPool, &trans, portMAX_DELAY);
        } else {
            ESP_LOGE(TAG, "事务池内存分配失败");
        }
    }
    ESP_LOGI(TAG, "SPI初始化完成");
}

/**
 * @brief 等待所有挂起事务完成
 */
void disp_wait_for_pending_transactions(void)
{
    spi_transaction_t *presult;
    while (uxQueueMessagesWaiting(TransactionPool) < SPI_TRANSACTION_POOL_SIZE) {
        if (spi_device_get_trans_result(disp_spi_handle, &presult, 1) == ESP_OK) {
            xQueueSend(TransactionPool, &presult, portMAX_DELAY);
        }
    }
}

/**
 * @brief 执行SPI数据传输
 */
static void disp_spi_transaction(const uint8_t *data, size_t length, disp_spi_send_flag_t flags,
                                 uint8_t *out, uint64_t addr, uint8_t dummy_bits)
{
    if (length == 0) return;

    spi_transaction_ext_t t = {
        .base = {
            .length = length * 8,
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
            ESP_LOGE(TAG, "SPI事务队列传输失败");
        }
    }
}

/**
 * @brief 发送数据（轮询模式）
 */
void disp_spi_send_data(uint8_t *data, size_t length)
{
    disp_spi_transaction(data, length, DISP_SPI_SEND_POLLING, NULL, 0, 0);
}

/**
 * @brief 发送颜色数据（队列模式）
 */
void disp_spi_send_colors(uint8_t *data, size_t length)
{
    disp_spi_transaction(data, length, DISP_SPI_SEND_QUEUED | DISP_SPI_SIGNAL_FLUSH, NULL, 0, 0);
}