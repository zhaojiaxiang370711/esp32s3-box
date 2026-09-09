#include <esp_system.h>
#include <algorithm>
#include <atomic>
#include "application.h"
#include "box_menu_display.h"
#include "box_music.h"
#include "box_ota.h"
#include "button.h"
#include "codecs/es8311_audio_codec.h"
#include "codecs/no_audio_codec.h"
#include "config.h"
#include "display/lcd_display.h"
#include "i2c_device.h"
#include "led/single_led.h"
#include "settings.h"
#include "wifi_board.h"
#include "wifi_manager.h"

#include <esp_log.h>
#include <driver/i2c_master.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/timers.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_panel_ops.h>

#define TAG "atk_dnesp32s3_box"

class ATK_NoAudioCodecDuplex : public NoAudioCodec {
public:
    ATK_NoAudioCodecDuplex(int input_sample_rate, int output_sample_rate, gpio_num_t bclk, gpio_num_t ws, gpio_num_t dout, gpio_num_t din) {
        duplex_ = true;
        input_sample_rate_ = input_sample_rate;
        output_sample_rate_ = output_sample_rate;
    
        i2s_chan_config_t chan_cfg = {
            .id = I2S_NUM_0,
            .role = I2S_ROLE_MASTER,
            .dma_desc_num = AUDIO_CODEC_DMA_DESC_NUM,
            .dma_frame_num = AUDIO_CODEC_DMA_FRAME_NUM,
            .auto_clear_after_cb = true,
            .auto_clear_before_cb = false,
            .intr_priority = 0,
        };
        ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_handle_, &rx_handle_));
    
        i2s_std_config_t std_cfg = {
            .clk_cfg = {
                .sample_rate_hz = (uint32_t)output_sample_rate_,
                .clk_src = I2S_CLK_SRC_DEFAULT,
                .mclk_multiple = I2S_MCLK_MULTIPLE_256,
                #ifdef   I2S_HW_VERSION_2
                    .ext_clk_freq_hz = 0,
                #endif
            },
            .slot_cfg = {
                .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,
                .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,
                .slot_mode = I2S_SLOT_MODE_STEREO,
                .slot_mask = I2S_STD_SLOT_BOTH,
                .ws_width = I2S_DATA_BIT_WIDTH_16BIT,
                .ws_pol = false,
                .bit_shift = true,
                #ifdef   I2S_HW_VERSION_2
                    .left_align = true,
                    .big_endian = false,
                    .bit_order_lsb = false
                #endif
            },
            .gpio_cfg = {
                .mclk = I2S_GPIO_UNUSED,
                .bclk = bclk,
                .ws = ws,
                .dout = dout,
                .din = din,
                .invert_flags = {
                    .mclk_inv = false,
                    .bclk_inv = false,
                    .ws_inv = false
                }
            }
        };
        ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle_, &std_cfg));
        ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle_, &std_cfg));
        ESP_LOGI(TAG, "Duplex channels created");
    }
};

// Preserve an explicitly saved mute setting; the shared codec defaults zero to 10.
template <typename Base>
class BoxSettingsCodec : public Base {
public:
    using Base::Base;
    void Start() override {
        Base::Start();
        Settings settings("audio");
        this->output_volume_ =
            std::clamp<int32_t>(settings.GetInt("output_volume", this->output_volume_), 0, 100);
        ESP_LOGI(TAG, "Restored output volume=%d", this->output_volume_);
    }
};

class XL9555_IN : public I2cDevice {
public:
    XL9555_IN(i2c_master_bus_handle_t i2c_bus, uint8_t addr) : I2cDevice(i2c_bus, addr) {
        WriteReg(0x06, 0x3B);
        WriteReg(0x07, 0xFE);
    }

    void xl9555_cfg(void) {
        WriteReg(0x06, 0x1B);
        WriteReg(0x07, 0xFE);
    }

    void SetOutputState(uint8_t bit, uint8_t level) {
        uint16_t data;
        int index = bit;

        if (bit < 8) {
            data = ReadReg(0x02);
        } else {
            data = ReadReg(0x03);
            index -= 8;
        }

        data = (data & ~(1 << index)) | (level << index);

        if (bit < 8) {
            WriteReg(0x02, data);
        } else {
            WriteReg(0x03, data);
        }
    }

    esp_err_t ReadNavigationInputs(uint8_t& inputs) {
        const uint8_t reg = 0x00;
        return i2c_master_transmit_receive(i2c_device_, &reg, 1, &inputs, 1, 10);
    }

    int GetPingState(uint16_t pin) {
        uint8_t data;
        if (pin <= 0x0080) {
            data = ReadReg(0x00);
            return (data & (uint8_t)(pin & 0xFF)) ? 1 : 0;
        } else {
            data = ReadReg(0x01);
            return (data & (uint8_t)((pin >> 8) & 0xFF )) ? 1 : 0;
        }

        return 0;
    }
};

class atk_dnesp32s3_box : public WifiBoard {
private:
    i2c_master_bus_handle_t i2c_bus_;
    i2c_master_dev_handle_t xl9555_handle_;
    Button boot_button_;
    BoxMenuDisplay* display_;
    BoxOta ota_;
    BoxMusic music_;
    TaskHandle_t navigation_task_ = nullptr;
    XL9555_IN* xl9555_in_;
    bool es8311_detected_ = false;
    static constexpr uint8_t kLcdBacklightPin = 7;
    bool screen_enabled_ = true;
    std::atomic<bool> wifi_restart_requested_{false};
    std::atomic<bool> wifi_waiting_connection_{false};

    void RefreshWifiStatus() {
        auto& manager = WifiManager::GetInstance();
        display_->SetWifiStatus(manager.IsConnected(), manager.IsConfigMode(), manager.GetSsid(),
                                manager.GetIpAddress());
    }

    void OpenWifiSetup() {
        wifi_restart_requested_ = true;
        if (IsInWifiConfigMode()) {
            auto& manager = WifiManager::GetInstance();
            display_->SetWifiInfo(manager.GetApSsid(), manager.GetApWebUrl());
        } else {
            display_->SetWifiInfo("", "");
            EnterWifiConfigMode();
        }
    }

    void InitializeI2c() {
        // Initialize I2C peripheral
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = (i2c_port_t)0,
            .sda_io_num = GPIO_NUM_48,
            .scl_io_num = GPIO_NUM_45,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));

        // Initialize XL9555
        xl9555_in_ = new XL9555_IN(i2c_bus_, 0x20);

        if (xl9555_in_->GetPingState(0x0020) == 1) {
            es8311_detected_ = true;    /* 音频设备标志位，SPK_CTRL_IO为高电平时，该标志位置1，且判定为ES8311 */
        } else {
            es8311_detected_ = false;    /* 音频设备标志位，SPK_CTRL_IO为低电平时，该标志位置0，且判定为NS4168 */
        }

        xl9555_in_->xl9555_cfg();
    }

    void InitializeATK_ST7789_80_Display() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;
        /* 配置RD引脚 */
        gpio_config_t gpio_init_struct;
        gpio_init_struct.intr_type = GPIO_INTR_DISABLE;
        gpio_init_struct.mode = GPIO_MODE_INPUT_OUTPUT;
        gpio_init_struct.pin_bit_mask = 1ull << LCD_NUM_RD;
        gpio_init_struct.pull_down_en = GPIO_PULLDOWN_DISABLE;
        gpio_init_struct.pull_up_en = GPIO_PULLUP_ENABLE;
        gpio_config(&gpio_init_struct);
        gpio_set_level(LCD_NUM_RD, 1);

        esp_lcd_i80_bus_handle_t i80_bus = NULL;
        esp_lcd_i80_bus_config_t bus_config = {
            .dc_gpio_num = LCD_NUM_DC,
            .wr_gpio_num = LCD_NUM_WR,
            .clk_src = LCD_CLK_SRC_DEFAULT,
            .data_gpio_nums = {
                GPIO_LCD_D0,
                GPIO_LCD_D1,
                GPIO_LCD_D2,
                GPIO_LCD_D3,
                GPIO_LCD_D4,
                GPIO_LCD_D5,
                GPIO_LCD_D6,
                GPIO_LCD_D7,
            },
            .bus_width = 8,
            .max_transfer_bytes = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t),
            .dma_burst_size = 64,
        };
        ESP_ERROR_CHECK(esp_lcd_new_i80_bus(&bus_config, &i80_bus));

        esp_lcd_panel_io_i80_config_t io_config = {
            .cs_gpio_num = LCD_NUM_CS,
            .pclk_hz = (10 * 1000 * 1000),
            .trans_queue_depth = 10,
            .on_color_trans_done = nullptr,
            .user_ctx = nullptr,
            .lcd_cmd_bits = 8,
            .lcd_param_bits = 8,
            .dc_levels = {
                .dc_idle_level = 0,
                .dc_cmd_level = 0,
                .dc_dummy_level = 0,
                .dc_data_level = 1,
            },
            .flags = {
                .swap_color_bytes = 0,
            },
        };
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_i80(i80_bus, &io_config, &panel_io));

        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
        panel_config.bits_per_pixel = 16;
        panel_config.reset_gpio_num = LCD_NUM_RST;
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel));

        esp_lcd_panel_reset(panel);
        esp_lcd_panel_init(panel);
        esp_lcd_panel_invert_color(panel, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        esp_lcd_panel_set_gap(panel, 0, 0);
        uint8_t data0[] = {0x00};
        uint8_t data1[] = {0x65};
        esp_lcd_panel_io_tx_param(panel_io, 0x36, data0, 1);
        esp_lcd_panel_io_tx_param(panel_io, 0x3A, data1, 1);
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));

        display_ = new BoxMenuDisplay(panel_io, panel, DISPLAY_WIDTH, DISPLAY_HEIGHT,
                                      DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X,
                                      DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY, 32, true);
    }

    void PollNavigationButtons() {
        box_menu::Key left;
        box_menu::Key right;
        bool failed = false;
        while (true) {
            uint8_t inputs = 0xFF;
            const auto err = xl9555_in_->ReadNavigationInputs(inputs);
            if (err == ESP_OK) {
                failed = false;
                const uint32_t now = esp_timer_get_time() / 1000;
                const auto dispatch = [this](box_menu::Press press, bool is_left) {
                    if (press == box_menu::Press::None)
                        return;
                    const auto action =
                        is_left ? (press == box_menu::Press::Long ? box_menu::Action::Back
                                                                  : box_menu::Action::Left)
                                : (press == box_menu::Press::Long ? box_menu::Action::Enter
                                                                  : box_menu::Action::Right);
                    Application::GetInstance().Schedule(
                        [this, action]() { display_->Navigate(action); });
                };
                // BOX K2=P03, K1=P04; both are active low XL9555 inputs.
                dispatch(left.Update((inputs & (1U << 3)) == 0, now), true);
                dispatch(right.Update((inputs & (1U << 4)) == 0, now), false);
            } else {
                // Drop incomplete gestures instead of synthesizing clicks on a bus error.
                left = box_menu::Key{};
                right = box_menu::Key{};
                if (!failed)
                    ESP_LOGW(TAG, "Navigation key read failed: %s", esp_err_to_name(err));
                failed = true;
            }
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            Application::GetInstance().Schedule([this]() {
                const bool enabled = !screen_enabled_;
                // Only change LCD_BL (XL9555 P07), preserving the audio outputs.
                xl9555_in_->SetOutputState(kLcdBacklightPin, enabled ? 1 : 0);
                screen_enabled_ = enabled;
                ESP_LOGI(TAG, "K0: screen %s", enabled ? "on" : "off");
            });
        });
    }

public:
    atk_dnesp32s3_box() : boot_button_(BOOT_BUTTON_GPIO) {
        InitializeI2c();
        InitializeATK_ST7789_80_Display();
        xl9555_in_->SetOutputState(5, 1);
        xl9555_in_->SetOutputState(kLcdBacklightPin, 1);
        InitializeButtons();
        display_->OnWifiSetup([this]() { OpenWifiSetup(); });
        display_->OnMusicAction([this](box_menu::Action action) { music_.Action(action); });
        music_.Start(display_);
        const auto result = xTaskCreate(
            [](void* arg) { static_cast<atk_dnesp32s3_box*>(arg)->PollNavigationButtons(); },
            "box_keys", 3072, this, 3, &navigation_task_);
        ESP_ERROR_CHECK(result == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
    }

    ~atk_dnesp32s3_box() override {
        if (navigation_task_)
            vTaskDelete(navigation_task_);
    }

    void SetNetworkEventCallback(NetworkEventCallback callback) override {
        WifiBoard::SetNetworkEventCallback(
            [this, callback = std::move(callback)](NetworkEvent event, const std::string& data) {
                Application::GetInstance().Schedule([this]() { RefreshWifiStatus(); });
                if (event == NetworkEvent::WifiConfigModeEnter) {
                    wifi_waiting_connection_ = false;
                    Application::GetInstance().Schedule([this]() {
                        auto& manager = WifiManager::GetInstance();
                        display_->SetWifiInfo(manager.GetApSsid(), manager.GetApWebUrl());
                    });
                } else if (event == NetworkEvent::WifiConfigModeExit && wifi_restart_requested_) {
                    wifi_waiting_connection_ = true;
                } else if (event == NetworkEvent::Connected &&
                           wifi_waiting_connection_.exchange(false) &&
                           wifi_restart_requested_.exchange(false)) {
                    Application::GetInstance().Schedule([]() {
                        {
                            Settings preferences("box_ui", true);
                            preferences.SetBool("wifi_result", true);
                        }
                        ESP_LOGI(TAG, "Wi-Fi configured and connected; restarting BOX");
                        esp_restart();
                    });
                    return;
                }
                if (event == NetworkEvent::Connected) {
                    // This BOX is a standalone menu device: do not launch cloud activation/OTA.
                    Application::GetInstance().Schedule([this]() {
                        auto& app = Application::GetInstance();
                        // No speech models are needed by this standalone menu.
                        static srmodel_list_t no_speech_models{};
                        app.GetAudioService().SetModelsList(&no_speech_models);
                        const auto state = app.GetDeviceState();
                        if (state == kDeviceStateStarting || state == kDeviceStateWifiConfiguring) {
                            app.SetDeviceState(kDeviceStateActivating);
                            app.SetDeviceState(kDeviceStateIdle);
                        }
                        ESP_LOGI(TAG, "BOX Wi-Fi connected; local menu ready");
                        ota_.Start(display_);
                    });
                    return;
                }
                if (callback)
                    callback(event, data);
            });
    }

    virtual AudioCodec* GetAudioCodec() override {
        /* 根据探测结果初始化编解码器 */
        if (es8311_detected_) {
            /* 使用ES8311 驱动 */
            static BoxSettingsCodec<Es8311AudioCodec> audio_codec(
                i2c_bus_, I2C_NUM_0, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE, GPIO_NUM_NC,
                AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN,
                GPIO_NUM_NC, AUDIO_CODEC_ES8311_ADDR, false);
            return &audio_codec;
        } else {
            static BoxSettingsCodec<ATK_NoAudioCodecDuplex> audio_codec(
                AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE, AUDIO_I2S_GPIO_BCLK,
                AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN);
            return &audio_codec;
        }
        return NULL;
    }
    
    virtual Display* GetDisplay() override {
        return display_;
    }
};

DECLARE_BOARD(atk_dnesp32s3_box);
