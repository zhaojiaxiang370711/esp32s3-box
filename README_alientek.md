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

## 界面字体

BOX 菜单使用独立的 Noto Sans CJK SC 字体子集（14px、20px），覆盖全部界面文案和可打印 ASCII。原项目 basic 字库不包含部分自定义菜单用字，不可直接用于新增文案。字体按 SIL OFL 1.1 授权，详见板级目录 `BOX-FONT-LICENSE.txt`。

修改中文文案后，用 FontTools、NotoSansCJK-Regular.ttc 与 `lv_font_conv@1.5.3` 重新生成：

```bash
python scripts/generate_box_fonts.py --font /path/to/NotoSansCJK-Regular.ttc --converter /path/to/lv_font_conv
python3 -m unittest discover -s scripts/tests -v
```

生成的 `box_menu_font_14.c`、`box_menu_font_20.c` 需要一同提交。58 项主机测试包含字库覆盖与设置页行高检查，避免出现缺字和底部文字越界。

## Wi-Fi 热点配网

设置页第三项为“Wi-Fi 配网”。短按 K1/K2 选择后，长按 K1 打开配网说明页：屏幕显示设备热点名称和网页地址。手机连接该热点后，用浏览器打开屏幕地址（默认 `http://192.168.4.1`），选择外部 2.4GHz Wi-Fi 并输入密码。

复用上游热点网页的连接测试和密码保存逻辑：成功页自动退出热点配网；BOX 随后确认连接成功并重启，重启后自动连接已保存的网络。失败时不触发重启，可继续在网页重试。长按 K2 仅退出屏幕说明页，配网热点继续开启。

该 BOX 作为独立菜单设备，联网后进入本地菜单待机，不触发原小智云激活或自动 OTA，避免覆盖自定义固件。此行为仅作用于第一代 BOX，其余板型保持原实现。亮度、音量和 K0 背光开关功能保留。

实机已完成手机网页配网：保存后自动重启，随后成功重连并进入本地菜单，用户确认流程正常。亮度和音量在重启后保留。独立菜单向音频服务提供空语音模型列表，避免待机时尝试读取未配置的唤醒词模型分区。

### Wi-Fi 状态与切换确认

设置列表直接显示 Wi-Fi 的“已连接 / 未连接 / 配网中”状态。进入后先显示当前 SSID 与 IP，查看状态不会开启热点或断开连接；长网络名横向滚动显示。

短按 K2/K1 选择“保持连接 / 切换 Wi-Fi”，长按 K1 确认。“切换 Wi-Fi”会进入二次确认页，默认选中“取消”，只有选中“确认切换”并长按 K1 才开始热点配网。长按 K2 返回上一层。热点已经开启时，右侧按钮为“查看配网”，直接显示热点和网页地址。

网络事件实时更新页面状态。由菜单发起的配网成功后，重启会自动打开 Wi-Fi 状态页；这个结果页标记只消费一次，之后普通重启回到主菜单。

本版 58 项主机测试通过；开源字体版与本机苹果字体版均构建成功。实机自动重连正常，用户确认网络名称、状态显示及选择/取消操作正常。
