# 介绍 

开发工具VSCode  ESP-IDF，编译环境为esp-idf v5.4

LVGL移植[教程](https://blog.csdn.net/2401_84036568/article/details/144636538)

~~LVGL选择[v9.2.2](https://github.com/lvgl/lvgl/releases/tag/v9.2.2)，[视频移植过程](https://www.bilibili.com/video/BV1cD4y1P71y/)，[文字移植过程](https://github.com/lvgl/lv_port_esp32)，[esp32的LVGL驱动](https://github.com/lvgl/lvgl_esp32_drivers)~~

也可参考[官方移植示例](https://github.com/espressif/esp-idf/tree/v5.4/examples/peripherals/lcd/i80_controller)和[官方触摸示例](https://github.com/espressif/esp-idf/tree/v5.4/examples/peripherals/lcd/spi_lcd_touch)。

## 如何使用

在VSCODE安装ESP-IDF，并安装ESP-IDF v5.4.0

以下是项目文件夹中其余文件的简短说明：

```
├── CMakeLists.txt
├── main
│   ├── CMakeLists.txt
│   └── main.c
└── README.md                  这是您当前正在阅读的文件
```

## 项目思路

### SPI和触摸屏

先初始化SPI2，添加SPI设备，再编写初始化ili9341函数。
如果SPI总线上有多个设备，可以利用事务池调度和管理任务。
事务池传输会根据数据长度选择发送方式（小数据用 tx_data，大数据用 tx_buffer），根据flag来配置地址位数和哑位。哑位是在 SPI 通信中用于时序调整的空闲位，某些设备在地址或命令后需要额外的等待时间。
数据的传输分为轮询、同步、队列。轮训和同步都是阻塞传输，数据传输完成才返回；队列是非阻塞，事务放入队列后立即返回。
项目中向屏幕发送普通数据用轮询方式，需要把DC置高表数据，发送颜色数据用队列。
使用队列模式传输，会先填充新事务，再回收已完成的事务，并从事务池获取对象，接着把事务加入队列一直反复。

触摸屏的坐标读取则使用SPI的同步模式，兼容性更好。

### LVGL

先将LVGL克隆到组件库，或者通过ESP-IDF直接下载。将将 lvgl/lv_conf_template.h 复制为 lv_conf.h 并放在 lvgl 文件夹旁边，将其第一个的 #if 0 更改为 1 以启用该文件的内容并修改设置 LV_COLOR_DEPTH 定义。
[文档](https://lvgl.100ask.net/9.2/get-started/quick-overview.html#add-lvgl-into-your-project-lvgl)已经写得很好了，绘制缓冲区，实现并注册一个flush函数、一个读取触摸的函数。

## 相关资料

- [ESP32运行LVGLv9.2](https://blog.csdn.net/2401_84036568/article/details/144636538)

