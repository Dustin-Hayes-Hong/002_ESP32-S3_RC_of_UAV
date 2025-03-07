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
        .clock_speed_hz = 40 * 1000 * 1000,     // 时钟频率40MHz
        .mode = LCD_SPI_MODE,                               // SPI模式0
        .spics_io_num = LCD_SPI_CS,              // 片选引脚
        .queue_size = SPI_TRANSACTION_POOL_SIZE, // 队列大小
        .pre_cb = NULL,
        .post_cb= NULL,
        .flags = SPI_DEVICE_NO_DUMMY | SPI_DEVICE_HALFDUPLEX,
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

#define DISP_SPI_HALF_DUPLEX
/**
 * @brief 执行SPI数据传输
 * @param data 发送数据缓冲区
 * @param length 数据长度（字节）
 * @param flags 传输标志
 * @param out 接收数据缓冲区（可选）
 * @param addr 地址值（可选）
 * @param dummy_bits 哑位数（可选）
 */
void disp_spi_transaction(const uint8_t *data, size_t length,
    disp_spi_send_flag_t flags, uint8_t *out,
    uint64_t addr, uint8_t dummy_bits)
{
    if (0 == length) {
        return;
    }

    spi_transaction_ext_t t = {0};

    /* transaction length is in bits */
    t.base.length = length * 8;

    if (length <= 4 && data != NULL) {
        t.base.flags = SPI_TRANS_USE_TXDATA;
        memcpy(t.base.tx_data, data, length);
    } else {
        t.base.tx_buffer = data;
    }

    if (flags & DISP_SPI_RECEIVE) {
        assert(out != NULL && (flags & (DISP_SPI_SEND_POLLING | DISP_SPI_SEND_SYNCHRONOUS)));
        t.base.rx_buffer = out;

#if defined(DISP_SPI_HALF_DUPLEX)
		t.base.rxlength = t.base.length;
		t.base.length = 0;	/* no MOSI phase in half-duplex reads */
#else
		t.base.rxlength = 0; /* in full-duplex mode, zero means same as tx length */
#endif
    }

    if (flags & DISP_SPI_ADDRESS_8) {
        t.address_bits = 8;
    } else if (flags & DISP_SPI_ADDRESS_16) {
        t.address_bits = 16;
    } else if (flags & DISP_SPI_ADDRESS_24) {
        t.address_bits = 24;
    } else if (flags & DISP_SPI_ADDRESS_32) {
        t.address_bits = 32;
    }
    if (t.address_bits) {
        t.base.addr = addr;
        t.base.flags |= SPI_TRANS_VARIABLE_ADDR;
    }

#if defined(DISP_SPI_HALF_DUPLEX)
	if (flags & DISP_SPI_MODE_DIO) {
		t.base.flags |= SPI_TRANS_MODE_DIO;
	} else if (flags & DISP_SPI_MODE_QIO) {
		t.base.flags |= SPI_TRANS_MODE_QIO;
	}

	if (flags & DISP_SPI_MODE_DIOQIO_ADDR) {
		t.base.flags |= SPI_TRANS_MODE_DIOQIO_ADDR;
	}

	if ((flags & DISP_SPI_VARIABLE_DUMMY) && dummy_bits) {
		t.dummy_bits = dummy_bits;
		t.base.flags |= SPI_TRANS_VARIABLE_DUMMY;
	}
#endif

    /* Save flags for pre/post transaction processing */
    t.base.user = (void *) flags;

    /* Poll/Complete/Queue transaction */
    if (flags & DISP_SPI_SEND_POLLING) {
		disp_wait_for_pending_transactions();	/* before polling, all previous pending transactions need to be serviced */
        spi_device_polling_transmit(spi, (spi_transaction_t *) &t);
    } else if (flags & DISP_SPI_SEND_SYNCHRONOUS) {
		disp_wait_for_pending_transactions();	/* before synchronous queueing, all previous pending transactions need to be serviced */
        spi_device_transmit(spi, (spi_transaction_t *) &t);
    } else {
		
		/* if necessary, ensure we can queue new transactions by servicing some previous transactions */
		if(uxQueueMessagesWaiting(TransactionPool) == 0) {
			spi_transaction_t *presult;
			while(uxQueueMessagesWaiting(TransactionPool) < SPI_TRANSACTION_POOL_RESERVE) {
				if (spi_device_get_trans_result(spi, &presult, 1) == ESP_OK) {
					xQueueSend(TransactionPool, &presult, portMAX_DELAY);	/* back to the pool to be reused */
				}
			}
		}

		spi_transaction_ext_t *pTransaction = NULL;
		xQueueReceive(TransactionPool, &pTransaction, portMAX_DELAY);
        memcpy(pTransaction, &t, sizeof(t));
        if (spi_device_queue_trans(spi, (spi_transaction_t *) pTransaction, portMAX_DELAY) != ESP_OK) {
			xQueueSend(TransactionPool, &pTransaction, portMAX_DELAY);	/* send failed transaction back to the pool to be reused */
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