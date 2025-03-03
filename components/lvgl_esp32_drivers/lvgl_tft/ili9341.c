#include "ili9341.h"
#include "disp_spi.h"
#include <stdint.h>
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_rom_gpio.h"
#include "freertos/task.h"

#define TAG "ILI9341"

typedef struct {
    uint8_t cmd;
    uint8_t data[16];
    uint8_t databytes;
} lcd_init_cmd_t;

static const lcd_init_cmd_t ili_init_cmds[] = {
    {0xCF, {0x00, 0x83, 0x30}, 3},
    {0xED, {0x64, 0x03, 0x12, 0x81}, 4},
    {0xE8, {0x85, 0x01, 0x79}, 3},
    {0xCB, {0x39, 0x2C, 0x00, 0x34, 0x02}, 5},
    {0xF7, {0x20}, 1},
    {0xEA, {0x00, 0x00}, 2},
    {0xC0, {0x26}, 1},
    {0xC1, {0x11}, 1},
    {0xC5, {0x35, 0x3E}, 2},
    {0xC7, {0xBE}, 1},
    {0x36, {0x28}, 1},
    {0x3A, {0x55}, 1},
    {0xB1, {0x00, 0x1B}, 2},
    {0xF2, {0x08}, 1},
    {0x26, {0x01}, 1},
    {0xE0, {0x1F, 0x1A, 0x18, 0x0A, 0x0F, 0x06, 0x45, 0x87, 0x32, 0x0A, 0x07, 0x02, 0x07, 0x05, 0x00}, 15},
    {0xE1, {0x00, 0x25, 0x27, 0x05, 0x10, 0x09, 0x3A, 0x78, 0x4D, 0x05, 0x18, 0x0D, 0x38, 0x3A, 0x1F}, 15},
    {0x2A, {0x00, 0x00, 0x00, 0xEF}, 4},
    {0x2B, {0x00, 0x00, 0x01, 0x3F}, 4},
    {0x2C, {0}, 0},
    {0xB7, {0x07}, 1},
    {0xB6, {0x0A, 0x82, 0x27, 0x00}, 4},
    {0x11, {0}, 0x80},
    {0x29, {0}, 0x80},
    {0, {0}, 0xFF},
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
    esp_rom_gpio_pad_select_gpio(LCD_SPI_DC);
    gpio_set_direction(LCD_SPI_DC, GPIO_MODE_OUTPUT);
    esp_rom_gpio_pad_select_gpio(LCD_SPI_RST);
    gpio_set_direction(LCD_SPI_RST, GPIO_MODE_OUTPUT);

    gpio_set_level(LCD_SPI_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(LCD_SPI_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(100));

    ESP_LOGI(TAG, "开始初始化ILI9341显示屏");

    for (uint16_t i = 0; ili_init_cmds[i].databytes != 0xFF; i++) {
        ili9341_send_cmd(ili_init_cmds[i].cmd);
        ili9341_send_data(ili_init_cmds[i].data, ili_init_cmds[i].databytes & 0x1F);
        if (ili_init_cmds[i].databytes & 0x80) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

    ili9341_set_orientation(0);

#if ILI9341_INVERT_COLORS == 1
    ili9341_send_cmd(0x21);
#else
    ili9341_send_cmd(0x20);
#endif
    ESP_LOGI(TAG, "ILI9341显示屏初始化完成");
}

/**
 * @brief 获取LCD ID
 */
uint32_t lcd_get_id(spi_device_handle_t handle)
{
    spi_device_acquire_bus(handle, portMAX_DELAY);

    ili9341_send_cmd(0x04);

    spi_transaction_t t = {
        .length = 8 * 3,
        .flags = SPI_TRANS_USE_RXDATA,
        .user = (void *)1,
    };

    esp_err_t ret = spi_device_polling_transmit(handle, &t);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "获取LCD ID失败: %s", esp_err_to_name(ret));
    }

    spi_device_release_bus(handle);
    return *(uint32_t *)t.rx_data;
}

/**
 * @brief 发送命令到ILI9341
 */
static void ili9341_send_cmd(uint8_t cmd)
{
    disp_wait_for_pending_transactions();
    gpio_set_level(LCD_SPI_DC, 0);
    disp_spi_send_data(&cmd, 1);
}

/**
 * @brief 发送数据到ILI9341
 */
static void ili9341_send_data(void *data, uint16_t length)
{
    if (length == 0) return;
    disp_wait_for_pending_transactions();
    gpio_set_level(LCD_SPI_DC, 1);
    disp_spi_send_data(data, length);
}

/**
 * @brief 发送颜色数据到ILI9341
 */
static void ili9341_send_color(void *data, uint16_t length)
{
    if (length == 0) return;
    disp_wait_for_pending_transactions();
    gpio_set_level(LCD_SPI_DC, 1);
    disp_spi_send_colors(data, length);
}

/**
 * @brief 刷新显示区域（LVGL回调）
 */
void ili9341_flush(lv_display_t *drv, const lv_area_t *area, unsigned char *color_map)
{
    uint8_t data[4];

    ili9341_send_cmd(0x2A);
    data[0] = (area->x1 >> 8) & 0xFF;
    data[1] = area->x1 & 0xFF;
    data[2] = (area->x2 >> 8) & 0xFF;
    data[3] = area->x2 & 0xFF;
    ili9341_send_data(data, 4);

    ili9341_send_cmd(0x2B);
    data[0] = (area->y1 >> 8) & 0xFF;
    data[1] = area->y1 & 0xFF;
    data[2] = (area->y2 >> 8) & 0xFF;
    data[3] = area->y2 & 0xFF;
    ili9341_send_data(data, 4);

    ili9341_send_cmd(0x2C);
    uint32_t size = lv_area_get_width(area) * lv_area_get_height(area);
    ili9341_send_color((void *)color_map, size * sizeof(lv_color_t));
}

/**
 * @brief 进入睡眠模式
 */
void ili9341_sleep_in(void)
{
    uint8_t data = 0x08;
    ili9341_send_cmd(0x10);
    ili9341_send_data(&data, 1);
    ESP_LOGI(TAG, "ILI9341进入睡眠模式");
}

/**
 * @brief 退出睡眠模式
 */
void ili9341_sleep_out(void)
{
    uint8_t data = 0x08;
    ili9341_send_cmd(0x11);
    ili9341_send_data(&data, 1);
    ESP_LOGI(TAG, "ILI9341退出睡眠模式");
}

/**
 * @brief 设置显示方向
 */
static void ili9341_set_orientation(uint8_t orientation)
{
    static const char *orientation_str[] = {"纵向", "纵向反转", "横向", "横向反转"};
    ESP_LOGI(TAG, "设置显示方向: %s", orientation_str[orientation]);

#if defined(CONFIG_LV_PREDEFINED_DISPLAY_M5STACK)
    static const uint8_t data[] = {0x68, 0x68, 0x08, 0x08};
#elif defined(CONFIG_LV_PREDEFINED_DISPLAY_M5CORE2)
    static const uint8_t data[] = {0x08, 0x88, 0x28, 0xE8};
#elif defined(CONFIG_LV_PREDEFINED_DISPLAY_WROVER4)
    static const uint8_t data[] = {0x6C, 0xEC, 0xCC, 0x4C};
#else
    static const uint8_t data[] = {0x48, 0x88, 0x28, 0xE8};
#endif

    ESP_LOGI(TAG, "0x36命令值: 0x%02X", data[orientation]);
    ili9341_send_cmd(0x36);
    ili9341_send_data((void *)&data[orientation], 1);
}