#ifndef __TOUCH_SPI_H
#define __TOUCH_SPI_H

#include <stdint.h>
#include "driver/spi_master.h"  // ESP-IDF SPI驱动头文件

// 触摸屏SPI配置宏
#define TP_SPI_HOST             SPI3_HOST       // SPI主机设备（通常为SPI3）
#define SPI_TOUCH_CLOCK_SPEED_HZ (2 * 1000 * 1000) // SPI时钟频率（2MHz）
#define SPI_TOUCH_SPI_MODE      0               // SPI模式（0或1）
#define TP_SPI_CLK              11              // 时钟引脚
#define TP_SPI_CS               12              // 片选引脚
#define TP_SPI_MOSI             13              // 主输出从输入引脚
#define TP_SPI_MISO             14              // 主输入从输出引脚
#define TP_SPI_IRQ              21              // 中断引脚

/**
 * @brief 初始化触摸屏SPI总线和设备
 */
void tp_spi_init(void);

/**
 * @brief 配置并添加SPI设备
 * @param host SPI主机设备
 * @param devcfg SPI设备接口配置结构体指针
 */
void tp_spi_add_device_config(spi_host_device_t host, spi_device_interface_config_t *devcfg);

/**
 * @brief 添加触摸屏SPI设备
 * @param host SPI主机设备
 */
void tp_spi_add_device(spi_host_device_t host);

/**
 * @brief SPI数据交换
 * @param data_send 发送数据缓冲区
 * @param data_recv 接收数据缓冲区
 * @param byte_count 数据字节数
 */
void tp_spi_xchg(uint8_t *data_send, uint8_t *data_recv, uint8_t byte_count);

/**
 * @brief 写触摸屏寄存器
 * @param data 数据缓冲区
 * @param byte_count 数据字节数
 */
void tp_spi_write_reg(uint8_t *data, uint8_t byte_count);

/**
 * @brief 读触摸屏寄存器
 * @param reg 寄存器命令
 * @param data 接收数据缓冲区
 * @param byte_count 数据字节数
 */
void tp_spi_read_reg(uint8_t reg, uint8_t *data, uint8_t byte_count);

#endif /* __TOUCH_SPI_H */