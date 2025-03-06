#include "ili9341.h"
#include "disp_spi.h"       // SPI显示驱动头文件
#include <stdint.h>
#include <string.h>
#include "esp_log.h"        // ESP-IDF日志库
#include "driver/gpio.h"    // GPIO驱动头文件
#include "esp_rom_gpio.h"   // ROM GPIO函数
#include "freertos/task.h"  // FreeRTOS任务支持

// 日志标签
static const char *TAG = "ILI9341";

// LCD初始化命令结构体
typedef struct {
    uint8_t cmd;           // 命令字节
    uint8_t data[16];      // 数据字节，最多16个
    uint8_t databytes;     // 数据字节数，低5位表示长度，高位0x80表示需要延时
} lcd_init_cmd_t;

// ILI9341初始化命令序列
static lcd_init_cmd_t ili_init_cmds[] = {
    {0xCF, {0x00, 0x83, 0x30}, 3},                    // 电源控制B
    {0xED, {0x64, 0x03, 0x12, 0x81}, 4},             // 电源控制序列
    {0xE8, {0x85, 0x01, 0x79}, 3},                    // 显示时序控制
    {0xCB, {0x39, 0x2C, 0x00, 0x34, 0x02}, 5},       // 电源控制A
    {0xF7, {0x20}, 1},                                // 泵比率控制
    {0xEA, {0x00, 0x00}, 2},                          // 显示时序控制
    {0xC0, {0x26}, 1},                                // 电源控制1
    {0xC1, {0x11}, 1},                                // 电源控制2
    {0xC5, {0x35, 0x3E}, 2},                          // VCOM控制1
    {0xC7, {0xBE}, 1},                                // VCOM控制2
    {0x36, {0x28}, 1},                                // 内存访问控制（初始方向）
    {0x3A, {0x55}, 1},                                // 像素格式（16位RGB565）
    {0xB1, {0x00, 0x1B}, 2},                          // 帧率控制
    {0xF2, {0x08}, 1},                                // 启用3伽马功能
    {0x26, {0x01}, 1},                                // 伽马设置
    {0xE0, {0x1F, 0x1A, 0x18, 0x0A, 0x0F, 0x06, 0x45, 0x87, 0x32, 0x0A, 0x07, 0x02, 0x07, 0x05, 0x00}, 15}, // 正伽马校正
    {0xE1, {0x00, 0x25, 0x27, 0x05, 0x10, 0x09, 0x3A, 0x78, 0x4D, 0x05, 0x18, 0x0D, 0x38, 0x3A, 0x1F}, 15}, // 负伽马校正
    {0x2A, {0x00, 0x00, 0x00, 0xEF}, 4},              // 列地址设置（0-239）
    {0x2B, {0x00, 0x00, 0x01, 0x3F}, 4},              // 页面地址设置（0-319）
    {0x2C, {0}, 0},                                    // 内存写入
    {0xB7, {0x07}, 1},                                // 进入模式设置
    {0xB6, {0x0A, 0x82, 0x27, 0x00}, 4},              // 显示功能控制
    {0x11, {0}, 0x80},                                // 退出睡眠模式（需延时）
    {0x29, {0}, 0x80},                                // 显示开启（需延时）
    {0, {0}, 0xFF},                                   // 序列结束标志
};

// 函数声明
static void ili9341_send_cmd(uint8_t cmd);
static void ili9341_send_data(void *data, uint16_t length);
static void ili9341_send_color(void *data, uint16_t length);
static void ili9341_set_orientation(uint8_t orientation);

/**
 * @brief 初始化ILI9341显示屏
 */
void ili9341_init(void)
{
    // 配置GPIO引脚
    esp_rom_gpio_pad_select_gpio(LCD_SPI_DC);
    esp_rom_gpio_pad_select_gpio(LCD_SPI_RST);
    if (gpio_set_direction(LCD_SPI_DC, GPIO_MODE_OUTPUT) != ESP_OK ||
        gpio_set_direction(LCD_SPI_RST, GPIO_MODE_OUTPUT) != ESP_OK) {
        ESP_LOGE(TAG, "GPIO方向设置失败");
        return;
    }

    // 执行硬件复位
    gpio_set_level(LCD_SPI_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(100));  // 低电平100ms
    gpio_set_level(LCD_SPI_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(100));  // 高电平100ms

    ESP_LOGI(TAG, "开始初始化ILI9341显示屏");

    // 发送初始化命令序列
    uint16_t cmd = 0;
    while (ili_init_cmds[cmd].databytes != 0xFF) {
        ili9341_send_cmd(ili_init_cmds[cmd].cmd);
        ili9341_send_data(ili_init_cmds[cmd].data, ili_init_cmds[cmd].databytes & 0x1F);
        if (ili_init_cmds[cmd].databytes & 0x80) {
            vTaskDelay(pdMS_TO_TICKS(100));  // 需要延时的命令
        }
        cmd++;
    }

    // 设置默认方向
    ili9341_set_orientation(2);  // 横向

    // 设置颜色反转（根据配置宏）
#if defined(ILI9341_INVERT_COLORS) && ILI9341_INVERT_COLORS == 1
    ili9341_send_cmd(0x21);  // 反转显示
#else
    ili9341_send_cmd(0x20);  // 正常显示
#endif

    ESP_LOGI(TAG, "ILI9341显示屏初始化完成");
}

/**
 * @brief 发送命令到ILI9341
 * @param cmd 命令字节
 */
static void ili9341_send_cmd(uint8_t cmd)
{
    disp_wait_for_pending_transactions();  // 等待挂起事务完成
    gpio_set_level(LCD_SPI_DC, 0);         // DC低表示命令
    disp_spi_send_data(&cmd, 1);           // 发送1字节命令
}

/**
 * @brief 发送数据到ILI9341
 * @param data 数据缓冲区指针
 * @param length 数据长度（字节）
 */
static void ili9341_send_data(void *data, uint16_t length)
{
    if (length == 0) return;
    disp_wait_for_pending_transactions();  // 等待挂起事务完成
    gpio_set_level(LCD_SPI_DC, 1);         // DC高表示数据
    disp_spi_send_data(data, length);      // 发送数据
}

/**
 * @brief 发送颜色数据到ILI9341（队列模式）
 * @param data 颜色数据缓冲区指针
 * @param length 数据长度（字节）
 */
static void ili9341_send_color(void *data, uint16_t length)
{
    if (length == 0) return;
    disp_wait_for_pending_transactions();  // 等待挂起事务完成
    gpio_set_level(LCD_SPI_DC, 1);         // DC高表示数据
    disp_spi_send_colors(data, length);    // 发送颜色数据
}

/**
 * @brief 刷新显示区域（LVGL回调）
 * @param drv LVGL显示驱动结构体指针
 * @param area 需要刷新的区域
 * @param color_map 颜色数据缓冲区
 */
void ili9341_flush(lv_display_t *drv, const lv_area_t *area, unsigned char *color_map)
{
    uint8_t data[4];

    // 设置列地址 (X坐标范围)
    ili9341_send_cmd(0x2A);
    data[0] = (area->x1 >> 8) & 0xFF;  // X起始高字节
    data[1] = area->x1 & 0xFF;         // X起始低字节
    data[2] = (area->x2 >> 8) & 0xFF;  // X结束高字节
    data[3] = area->x2 & 0xFF;         // X结束低字节
    ili9341_send_data(data, 4);

    // 设置页面地址 (Y坐标范围)
    ili9341_send_cmd(0x2B);
    data[0] = (area->y1 >> 8) & 0xFF;  // Y起始高字节
    data[1] = area->y1 & 0xFF;         // Y起始低字节
    data[2] = (area->y2 >> 8) & 0xFF;  // Y结束高字节
    data[3] = area->y2 & 0xFF;         // Y结束低字节
    ili9341_send_data(data, 4);

    // 写入颜色数据
    ili9341_send_cmd(0x2C);
    uint32_t size = lv_area_get_width(area) * lv_area_get_height(area);  // 计算像素数
    ili9341_send_color(color_map, size * sizeof(lv_color_t));           // 发送颜色数据
}

/**
 * @brief 进入睡眠模式
 */
void ili9341_sleep_in(void)
{
    uint8_t data = 0x08;  // 睡眠参数
    ili9341_send_cmd(0x10);  // 睡眠命令
    ili9341_send_data(&data, 1);
    ESP_LOGI(TAG, "ILI9341进入睡眠模式");
}

/**
 * @brief 退出睡眠模式
 */
void ili9341_sleep_out(void)
{
    uint8_t data = 0x08;  // 唤醒参数
    ili9341_send_cmd(0x11);  // 退出睡眠命令
    ili9341_send_data(&data, 1);
    vTaskDelay(pdMS_TO_TICKS(120));  // 等待显示器稳定
    ESP_LOGI(TAG, "ILI9341退出睡眠模式");
}

/**
 * @brief 设置显示方向
 * @param orientation 方向值 (0:纵向, 1:纵向反转, 2:横向, 3:横向反转)
 */
static void ili9341_set_orientation(uint8_t orientation)
{
    static const char *orientation_str[] = {"纵向", "纵向反转", "横向", "横向反转"};
    if (orientation > 3) {
        ESP_LOGE(TAG, "无效的方向值: %d", orientation);
        return;
    }
    ESP_LOGI(TAG, "设置显示方向: %s", orientation_str[orientation]);

    // 根据硬件配置选择方向参数
#if defined(CONFIG_LV_PREDEFINED_DISPLAY_M5STACK)
    static const uint8_t data[] = {0x68, 0x68, 0x08, 0x08};
#elif defined(CONFIG_LV_PREDEFINED_DISPLAY_M5CORE2)
    static const uint8_t data[] = {0x08, 0x88, 0x28, 0xE8};
#elif defined(CONFIG_LV_PREDEFINED_DISPLAY_WROVER4)
    static const uint8_t data[] = {0x6C, 0xEC, 0xCC, 0x4C};
#else
    static const uint8_t data[] = {0x48, 0x88, 0x28, 0xE8};  // 默认方向配置
#endif

    ESP_LOGI(TAG, "0x36命令值: 0x%02X", data[orientation]);
    ili9341_send_cmd(0x36);  // 内存访问控制命令
    ili9341_send_data((void *)&data[orientation], 1);
}