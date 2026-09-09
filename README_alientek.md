# ESP32-S3 BOX

基于 [78/xiaozhi-esp32](https://github.com/78/xiaozhi-esp32) 的正点原子 ESP32-S3 开发项目。上游基线：`c7241272f2d5fd140c77542f3cf12d09e717fc2f`。保留上游 MIT 许可证与版权声明。

## 当前功能

已在第一代正点原子 BOX 上验证：

- 苹果风格浅色圆角卡片，240ms 翻页、进入和返回动画。
- 四个测试功能页面，以及一个设置页面。
- K0 切换屏幕背光，保持当前页面。
- 设置画面亮度（10%–100%）和音量（0%–100%），支持保存和重启恢复。

| 场景 | 短按 K2 / K1 | 长按 K1（约 0.8 秒） | 长按 K2 |
| --- | --- | --- | --- |
| 主菜单 | 向左 / 向右循环选择 | 进入当前功能 | 保持主菜单 |
| 设置列表 | 切换设置项 | 开始调节 | 返回主菜单 |
| 调节中 | 减小 / 增大 10 个百分点 | 保存并退出调节 | 保留修改，返回设置列表 |

第一代 BOX 的背光由 XL9555 P07 开关控制。目前亮度通过画面透明黑色层调节，**不是背光 PWM 调光，不降低背光功耗**。音量调用实际音频输出接口；0% 静音，非静音状态保存音量时播放短提示音。亮度退出调节时保存，音量即调即存。

## 支持的板型配置

| 目录（main/boards/alientek/ 下） | 上游发布变体 |
| --- | --- |
| atk-dnesp32s3 | atk-dnesp32s3 |
| atk-dnesp32s3-box | atk-dnesp32s3-box |
| atk-dnesp32s3-box0 | atk-dnesp32s3-box0 |
| atk-dnesp32s3-box2-4g | atk-dnesp32s3-box2-4g |
| atk-dnesp32s3-box2-wifi | atk-dnesp32s3-box2-wifi |
| atk-dnesp32s3-box3 | atk-dnesp32s3-box3 |
| atk-dnesp32s3m-4g | 上游 builds 为空，保留原始板型实现与菜单 |
| atk-dnesp32s3m-wifi | 上游 builds 为空，保留原始板型实现与菜单 |

只保留正点原子 ESP32-S3 配置与共享组件；自定义菜单与设置目前仅适用于 `atk-dnesp32s3-box`。其他配置保留上游实现，未验证这些新功能。

**第一代 BOX 与 BOX0 引脚不同，不可互刷。请按实物型号选择固件。**

## 构建与烧录

当前验证环境：ESP-IDF v6.1，ESP32-S3，16MB Flash、8MB PSRAM。安装 ESP-IDF 并激活对应环境后运行：

```bash
python scripts/build.py --list-boards
python scripts/build.py alientek/atk-dnesp32s3-box --name atk-dnesp32s3-box
idf.py -p YOUR_SERIAL_PORT flash
```

首次刷入前建议自行备份原固件。构建产物、设备完整备份、SDK、密钥和本地配置均不入库。

## 开发与验证

- 板级入口：`main/boards/alientek/atk-dnesp32s3-box/atk_dnesp32s3_box.cc`
- 界面：`box_menu_display.cc` / `box_menu_display.h`
- 按键与导航状态：`box_menu_state.h`
- 主机测试：`python3 -m unittest discover -s scripts/tests -v`

56 项主机测试和 BOX 构建通过；实机验证了 K0、菜单导航、过渡动画、亮度、音量，以及保存值的重启恢复。0% 静音的重启恢复分支已实现，但尚未单独进行实机验证。

## 仓库

- GitHub：https://github.com/zhaojiaxiang370711/esp32s3-box
- NAS Gitea：私有备份仓库，仅授权用户可访问。

公开仓库采用当前精简代码快照作为初始提交，完整上游历史见原项目。
