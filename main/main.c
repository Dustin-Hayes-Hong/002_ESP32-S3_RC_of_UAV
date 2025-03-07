#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "lvgl_port.h"

#define TAG "MAIN"

// LVGL 任务处理函数
static void lvgl_task(void *arg)
{
    while (1) {
        // 处理 LVGL 事件
        lv_task_handler();
        vTaskDelay(pdMS_TO_TICKS(10)); // 每 10ms 调用一次
    }
}
lv_obj_t* home_page = NULL;
lv_obj_t *status_bar = NULL;

void LVGL_picture_establish(void) {

    // 设置屏幕背景颜色
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x003a57), LV_PART_MAIN);
    lv_obj_invalidate(lv_screen_active()); // 标记整个屏幕为脏区域

    // 创建并配置标签
    lv_obj_t *label = lv_label_create(lv_screen_active());
    lv_label_set_text(label, "Hello world");
    lv_obj_set_style_text_color(label, lv_color_hex(0xffffff), LV_PART_MAIN); // 标签颜色应作用于 label 而非屏幕
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);

    // 强制刷新
    lv_refr_now(NULL);
    ESP_LOGI("LVGL", "Screen refreshed");
}

void app_main(void)
{
    // 初始化 LVGL 和硬件
    lv_port_init();
    ESP_LOGI(TAG, "LVGL 和硬件初始化完成");

    // 创建 LVGL UI
    LVGL_picture_establish();
    ESP_LOGI(TAG, "UI 创建完成");

    // 创建 LVGL 任务
    xTaskCreate(lvgl_task, "lvgl_task", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "LVGL 任务启动");
}