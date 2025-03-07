#ifndef __DISP_SPI_H
#define __DISP_SPI_H

#include <stdint.h>
#include "driver/spi_master.h"
#include "driver/gpio.h"

// SPI事务池配置
#define SPI_TRANSACTION_POOL_SIZE 50              // 事务池最大容量
#define SPI_TRANSACTION_POOL_RESERVE_PERCENTAGE 10 // 保留容量百分比
#define SPI_TRANSACTION_POOL_RESERVE (SPI_TRANSACTION_POOL_SIZE / SPI_TRANSACTION_POOL_RESERVE_PERCENTAGE)

// 显示分辨率和缓冲区配置
#define LV_HOR_RES_MAX (320)                      // 水平最大分辨率
#define LV_VER_RES_MAX (240)                      // 垂直最大分辨率
#define DISP_BUF_SIZE  (LV_HOR_RES_MAX * 40)      // 显示缓冲区大小
#define SPI_BUS_MAX_TRANSFER_SZ (DISP_BUF_SIZE * 2) // SPI最大传输大小

// SPI主机和引脚配置
#define LCD_SPI_HOST       SPI2_HOST      // SPI主机设备，通常为SPI2或SPI3
#define LCD_SPI_MOSI       42             // SPI主输出从输入引脚
#define LCD_SPI_CS         41             // SPI片选引脚
#define LCD_SPI_DC         40             // 数据/命令控制引脚
#define LCD_SPI_SCLK       39             // SPI时钟引脚
#define LCD_SPI_RST        38             // 显示屏复位引脚
#define LCD_SPI_MODE       0              // SPI模式0

// SPI传输标志枚举，定义不同的传输模式和选项
typedef enum {
    DISP_SPI_SEND_QUEUED        = 0x00000000,  // 队列模式异步传输（默认）
    DISP_SPI_SEND_POLLING       = 0x00000001,  // 轮询模式同步传输
    DISP_SPI_SEND_SYNCHRONOUS   = 0x00000002,  // 同步传输，等待完成
    DISP_SPI_SIGNAL_FLUSH       = 0x00000004,  // 标记需要刷新显示
    DISP_SPI_RECEIVE            = 0x00000008,  // 接收模式
    DISP_SPI_ADDRESS_8          = 0x00000040,  // 8位地址
    DISP_SPI_ADDRESS_16         = 0x00000080,  // 16位地址
    DISP_SPI_ADDRESS_24         = 0x00000100,  // 24位地址
    DISP_SPI_ADDRESS_32         = 0x00000200,  // 32位地址
    DISP_SPI_MODE_DIO           = 0x00000400,  // 双线模式
    DISP_SPI_MODE_QIO           = 0x00000800,  // 四线模式
    DISP_SPI_MODE_DIOQIO_ADDR   = 0x00001000,  // DIO/QIO模式下的地址
    DISP_SPI_VARIABLE_DUMMY     = 0x00002000,  // 可变哑位
} disp_spi_send_flag_t;

// 函数声明
/**
 * @brief 初始化SPI总线和设备
 */
void disp_spi_init(void);

/**
 * @brief 使用轮询模式发送数据
 * @param data 数据缓冲区指针
 * @param length 数据长度（字节）
 */
void disp_spi_send_data(uint8_t *data, size_t length);

/**
 * @brief 使用队列模式发送颜色数据
 * @param data 颜色数据缓冲区指针
 * @param length 数据长度（字节）
 */
void disp_spi_send_colors(uint8_t *data, size_t length);

/**
 * @brief 等待所有挂起的事务完成
 */
void disp_wait_for_pending_transactions(void);

#endif /* __DISP_SPI_H */