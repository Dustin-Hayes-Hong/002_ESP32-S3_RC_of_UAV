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
void LVGL_picture_establish(void){//创建画面






    //**************************主页面**********************************//
        home_page = lv_obj_create(NULL);//创建主页面 lv_obj_create()
    
        //home_page = lv_scr_act();//创建主页面
        lv_obj_set_size(home_page, 320, 240);   //设置页面大小
        lv_obj_set_style_bg_color(home_page, lv_color_hex(0x808080), LV_STATE_DEFAULT); //设置页面默认颜色
        lv_scr_load(home_page); // 加载屏幕
    
        status_bar = lv_obj_create(home_page);//状态栏
         lv_obj_set_style_text_font(status_bar, &lv_font_montserrat_14, 0);//状态栏主要显示图标，所以使用系统使用系统自带库
        lv_obj_set_style_bg_color(status_bar, lv_color_hex(0xC0C0C0), LV_STATE_DEFAULT);//设置页面默认颜色
        lv_obj_set_size(status_bar, 320, 25);    //设置大小
        lv_obj_set_pos(status_bar, 0, 0);    //设置位置
    
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