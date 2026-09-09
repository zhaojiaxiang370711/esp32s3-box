#include "box_menu_display.h"
#include <cstdio>
#include "application.h"
#include "assets/lang_config.h"
#include "audio_codec.h"
#include "board.h"
#include "settings.h"
LV_FONT_DECLARE(box_menu_font_20);
LV_FONT_DECLARE(box_menu_font_14);
namespace {
constexpr int kCardX = 16;
constexpr uint32_t kAccent[] = {0x007AFF, 0x34A56F, 0xAF52DE, 0xE58A22, 0x269EA8};
// Small native primitives retain crisp edges without enlarging raster artwork.
void IconShape(lv_obj_t* parent, int x, int y, int width, int height, int radius = 2,
               bool outline = false) {
    auto shape = lv_obj_create(parent);
    lv_obj_remove_style_all(shape);
    lv_obj_set_pos(shape, x, y);
    lv_obj_set_size(shape, width, height);
    lv_obj_set_style_radius(shape, radius, 0);
    lv_obj_remove_flag(shape, LV_OBJ_FLAG_SCROLLABLE);
    if (outline) {
        lv_obj_set_style_border_width(shape, 2, 0);
        lv_obj_set_style_border_color(shape, lv_color_white(), 0);
    } else {
        lv_obj_set_style_bg_color(shape, lv_color_white(), 0);
        lv_obj_set_style_bg_opa(shape, LV_OPA_COVER, 0);
    }
}
void DrawIcon(lv_obj_t* parent, int page) {
    if (page == 0) {
        for (int y : {11, 24})
            for (int x : {11, 24})
                IconShape(parent, x, y, 9, 9, 3);
    } else if (page == 1) {
        IconShape(parent, 11, 22, 4, 11);
        IconShape(parent, 20, 11, 4, 22);
        IconShape(parent, 29, 16, 4, 17);
    } else if (page == 2) {
        IconShape(parent, 10, 10, 24, 24, 12, true);
        IconShape(parent, 20, 15, 2, 9, 0);
        IconShape(parent, 20, 22, 8, 2, 0);
    } else if (page == 3) {
        IconShape(parent, 12, 9, 20, 26, 3, true);
        IconShape(parent, 17, 16, 10, 2, 1);
        IconShape(parent, 17, 22, 10, 2, 1);
        IconShape(parent, 17, 28, 6, 2, 1);
    } else {
        for (int i = 0; i < 3; ++i) {
            IconShape(parent, 10, 12 + i * 9, 24, 2, 1);
            IconShape(parent, i == 1 ? 25 : 14, 10 + i * 9, 5, 6, 2);
        }
    }
}

void Animate(lv_obj_t* object, lv_anim_exec_xcb_t callback, int from, int to, void* owner = nullptr,
             lv_anim_completed_cb_t completed = nullptr) {
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, object);
    lv_anim_set_exec_cb(&a, callback);
    lv_anim_set_values(&a, from, to);
    lv_anim_set_duration(&a, 240);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_set_user_data(&a, owner);
    lv_anim_set_completed_cb(&a, completed);
    lv_anim_start(&a);
}
}  // namespace
lv_obj_t* BoxMenuDisplay::Label(lv_obj_t* parent, int x, int y, int width, const char* text) {
    auto label = lv_label_create(parent);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_width(label, width);
    lv_label_set_text(label, text);
    return label;
}
void BoxMenuDisplay::SetupUI() {
    SpiLcdDisplay::SetupUI();
    DisplayLockGuard lock(this);
    if (root_)
        return;
    Settings preferences("box_ui", true);
    Settings audio("audio");
    menu_.Load(preferences.GetInt("brightness", 100), audio.GetInt("output_volume", 70));
    if (preferences.GetBool("wifi_result", false)) {
        menu_.selected = menu_.kPageCount - 1;
        menu_.entered = true;
        menu_.setting = 2;
        menu_.wifi_details = true;
        preferences.SetBool("wifi_result", false);
    }
    ESP_LOGI("BoxMenu", "Loaded brightness=%d volume=%d", menu_.brightness, menu_.volume);
    root_ = lv_obj_create(lv_display_get_layer_top(display_));
    lv_obj_remove_style_all(root_);
    lv_obj_set_size(root_, width_, height_);
    lv_obj_set_style_bg_color(root_, lv_color_hex(0xF2F2F7), 0);
    lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(root_, lv_color_hex(0x1D1D1F), 0);
    lv_obj_set_style_text_font(root_, &box_menu_font_20, 0);
    lv_obj_remove_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
    header_ = Label(root_, 20, 12, 220, "主功能");
    counter_ = Label(root_, 248, 18, 52, "1 / 5");
    lv_obj_set_style_text_font(counter_, &box_menu_font_14, 0);
    lv_obj_set_style_text_align(counter_, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_color(counter_, lv_color_hex(0x636366), 0);
    for (int i = 0; i < box_menu::State::kPageCount; ++i) {
        dots_[i] = lv_obj_create(root_);
        lv_obj_remove_style_all(dots_[i]);
        lv_obj_set_pos(dots_[i], 121 + i * 17, 182);
        lv_obj_set_size(dots_[i], 10, 5);
        lv_obj_set_style_radius(dots_[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(dots_[i], LV_OPA_COVER, 0);
    }
    navigation_ = Label(root_, 16, 190, 288, "");
    lv_obj_set_style_text_font(navigation_, &box_menu_font_14, 0);
    lv_obj_set_style_text_color(navigation_, lv_color_hex(0x007AFF), 0);
    lv_obj_set_style_text_align(navigation_, LV_TEXT_ALIGN_CENTER, 0);
    help_ = Label(root_, 8, 214, 304, "长按 K2 返回   ·   长按 K1 进入");
    lv_obj_set_style_text_font(help_, &box_menu_font_14, 0);
    lv_obj_set_style_text_align(help_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(help_, lv_color_hex(0x636366), 0);
    card_ = CreateCard();
    dimmer_ = lv_obj_create(lv_display_get_layer_top(display_));
    lv_obj_remove_style_all(dimmer_);
    lv_obj_set_size(dimmer_, width_, height_);
    lv_obj_set_style_bg_color(dimmer_, lv_color_black(), 0);
    lv_obj_remove_flag(dimmer_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(dimmer_, LV_OBJ_FLAG_SCROLLABLE);
    ApplyBrightness();
    Render();
}
lv_obj_t* BoxMenuDisplay::CreateCard() {
    auto card = lv_obj_create(root_);
    lv_obj_remove_style_all(card);
    lv_obj_set_pos(card, kCardX, 48);
    lv_obj_set_size(card, 288, 122);
    lv_obj_set_style_bg_color(card, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, 20, 0);
    lv_obj_set_style_shadow_color(card, lv_color_hex(0x202040), 0);
    lv_obj_set_style_shadow_opa(card, 8, 0);
    lv_obj_set_style_shadow_width(card, 4, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0xE5E5EA), 0);
    lv_obj_set_style_shadow_offset_y(card, 2, 0);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    if (menu_.wifi_setup) {
        auto hint = Label(card, 16, 8, 256, "手机连接设备热点");
        lv_obj_set_style_text_font(hint, &box_menu_font_14, 0);
        wifi_ssid_label_ =
            Label(card, 16, 31, 256, wifi_ssid_.empty() ? "正在开启热点..." : wifi_ssid_.c_str());
        wifi_url_label_ =
            Label(card, 16, 65, 256, wifi_url_.empty() ? "请稍候" : wifi_url_.c_str());
        lv_obj_set_style_text_font(wifi_url_label_, &box_menu_font_14, 0);
        lv_obj_set_style_text_color(wifi_url_label_, lv_color_hex(0x007AFF), 0);
        auto result = Label(card, 16, 94, 256, "配置成功后自动重启");
        lv_obj_set_style_text_font(result, &box_menu_font_14, 0);
        return card;
    }
    if (menu_.wifi_details) {
        wifi_state_label_ = Label(card, 16, 8, 256, "");
        lv_obj_set_style_text_font(wifi_state_label_, &box_menu_font_14, 0);
        station_label_ = Label(card, 16, 30, 256, "");
        lv_label_set_long_mode(station_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
        station_ip_label_ = Label(card, 16, 60, 256, "");
        lv_obj_set_style_text_font(station_ip_label_, &box_menu_font_14, 0);
        for (int i = 0; i < 2; ++i) {
            wifi_buttons_[i] = Label(card, 16 + i * 134, 88, 122, "");
            lv_obj_set_style_text_font(wifi_buttons_[i], &box_menu_font_14, 0);
            lv_obj_set_style_text_align(wifi_buttons_[i], LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_set_style_bg_opa(wifi_buttons_[i], LV_OPA_COVER, 0);
            lv_obj_set_style_radius(wifi_buttons_[i], 7, 0);
            lv_obj_set_style_pad_ver(wifi_buttons_[i], 3, 0);
        }
        UpdateWifiDetails();
        return card;
    }
    if (menu_.IsSettings()) {
        for (int i = 0; i < menu_.kSettingCount; ++i) {
            rows_[i] = lv_obj_create(card);
            lv_obj_remove_style_all(rows_[i]);
            lv_obj_set_pos(rows_[i], 8, 4 + i * 38);
            lv_obj_set_size(rows_[i], 272, 36);
            lv_obj_set_style_radius(rows_[i], 12, 0);
            lv_obj_set_style_bg_opa(rows_[i], LV_OPA_COVER, 0);
            lv_obj_set_style_border_width(rows_[i], 1, 0);
            auto title =
                Label(rows_[i], 10, 2, 160, i == 0 ? "屏幕亮度" : (i == 1 ? "声音大小" : "Wi-Fi"));
            lv_obj_set_style_text_font(title, &box_menu_font_14, 0);
            values_[i] = Label(rows_[i], 192, 2, 68, "");
            lv_obj_set_style_text_font(values_[i], &box_menu_font_14, 0);
            lv_obj_set_style_text_align(values_[i], LV_TEXT_ALIGN_RIGHT, 0);
            if (i == 2)
                continue;
            bars_[i] = lv_bar_create(rows_[i]);
            lv_obj_set_pos(bars_[i], 12, 26);
            lv_obj_set_size(bars_[i], 248, 4);
            lv_bar_set_range(bars_[i], 0, 100);
            lv_obj_set_style_bg_color(bars_[i], lv_color_hex(0xE5E5EA), LV_PART_MAIN);
            lv_obj_set_style_bg_color(bars_[i], lv_color_hex(0x007AFF), LV_PART_INDICATOR);
            lv_obj_set_style_anim_duration(bars_[i], 150, 0);
        }
        UpdateSettings();
        return card;
    }
    auto icon = lv_obj_create(card);
    lv_obj_remove_style_all(icon);
    lv_obj_set_pos(icon, 18, 18);
    lv_obj_set_size(icon, 44, 44);
    lv_obj_set_style_radius(icon, 13, 0);
    lv_obj_set_style_bg_color(icon, lv_color_hex(kAccent[menu_.selected]), 0);
    lv_obj_set_style_bg_opa(icon, LV_OPA_COVER, 0);
    char text[48];
    DrawIcon(icon, menu_.selected);
    if (menu_.selected == menu_.kPageCount - 1)
        std::snprintf(text, sizeof(text), "设置");
    else
        std::snprintf(text, sizeof(text), "测试功能 %d", menu_.selected + 1);
    Label(card, 76, 18, 194, text);
    auto subtitle = Label(card, 76, 47, 194,
                          menu_.selected == menu_.kPageCount - 1
                              ? "显示、声音与无线网络"
                              : (menu_.entered ? "已进入测试页面" : "探索你的新功能"));
    lv_obj_set_style_text_font(subtitle, &box_menu_font_14, 0);
    lv_obj_set_style_text_color(subtitle, lv_color_hex(0x636366), 0);
    auto detail =
        Label(card, 18, 88, 252, menu_.entered ? "功能待开发，长按 K2 返回" : "长按 K1 打开");
    lv_obj_set_style_text_font(detail, &box_menu_font_14, 0);
    lv_obj_set_style_text_color(detail, lv_color_hex(menu_.entered ? 0x636366 : 0x007AFF), 0);
    if (!menu_.entered) {
        static const lv_point_precise_t points[] = {{0, 0}, {6, 6}, {0, 12}};
        auto chevron = lv_line_create(card);
        lv_line_set_points(chevron, points, 3);
        lv_obj_set_pos(chevron, 258, 91);
        lv_obj_set_style_line_width(chevron, 2, 0);
        lv_obj_set_style_line_rounded(chevron, true, 0);
        lv_obj_set_style_line_color(chevron, lv_color_hex(0x007AFF), 0);
    }
    return card;
}
void BoxMenuDisplay::Render() {
    char text[24];
    lv_label_set_text(
        header_, menu_.wifi_setup
                     ? "Wi-Fi 配网"
                     : (menu_.IsSettings() ? "设置" : (menu_.entered ? "功能页面" : "主功能")));
    std::snprintf(text, sizeof(text), "%d / %d", menu_.selected + 1, menu_.kPageCount);
    lv_label_set_text(counter_, text);
    lv_label_set_text(navigation_, menu_.entered ? "长按 K2 回到主功能"
                                                 : "<  K2 向左                 K1 向右  >");
    lv_label_set_text(help_, "长按 K2 返回   ·   长按 K1 进入");
    if (menu_.IsSettings()) {
        lv_label_set_text(navigation_,
                          menu_.editing ? "K2 减小                 K1 增大" : "K2 / K1 选择设置项");
        lv_label_set_text(help_, menu_.editing ? "长按 K1 保存   ·   长按 K2 返回"
                                               : "长按 K1 调节   ·   长按 K2 返回");
    }
    if (menu_.wifi_setup) {
        lv_label_set_text(navigation_, "连接热点后，用浏览器打开地址");
        lv_label_set_text(help_, "长按 K2 返回   ·   热点保持开启");
    } else if (menu_.wifi_details) {
        lv_label_set_text(header_, menu_.wifi_confirm ? "切换 Wi-Fi" : "Wi-Fi 状态");
        lv_label_set_text(navigation_, "短按 K2 / K1 选择按钮");
        lv_label_set_text(help_, "长按 K1 确认   ·   长按 K2 返回");
    } else if (menu_.IsSettings() && menu_.setting == 2) {
        lv_label_set_text(help_, "长按 K1 查看   ·   长按 K2 返回");
    }
    for (int i = 0; i < menu_.kPageCount; ++i)
        lv_obj_set_style_bg_color(dots_[i], lv_color_hex(i == menu_.selected ? 0x007AFF : 0xD1D1D6),
                                  0);
}
void BoxMenuDisplay::SetX(void* object, int32_t value) {
    lv_obj_set_x(static_cast<lv_obj_t*>(object), value);
}
void BoxMenuDisplay::FinishTransition(lv_anim_t* animation) {
    auto self = static_cast<BoxMenuDisplay*>(lv_anim_get_user_data(animation));
    auto old = static_cast<lv_obj_t*>(animation->var);
    if (self->outgoing_ == old) {
        self->outgoing_ = nullptr;
        lv_obj_delete(old);
    }
}
void BoxMenuDisplay::Transition(int direction) {
    // Finish interrupted transitions before starting another: at most two cards exist.
    if (outgoing_) {
        lv_obj_delete(outgoing_);
        outgoing_ = nullptr;
    }
    lv_anim_delete(card_, nullptr);
    lv_obj_set_pos(card_, kCardX, 48);
    outgoing_ = card_;
    card_ = CreateCard();
    // Native pixel translation keeps text sharp and avoids full-card alpha layers.
    Animate(outgoing_, SetX, kCardX, kCardX - direction * width_, this, FinishTransition);
    Animate(card_, SetX, kCardX + direction * width_, kCardX);
}
void BoxMenuDisplay::ApplyBrightness() {
    // XL9555 LCD_BL is a binary output. Dim pixels without toggling the I2C backlight.
    if (dimmer_)
        lv_obj_set_style_bg_opa(dimmer_, (100 - menu_.brightness) * 255 / 100, 0);
}
void BoxMenuDisplay::UpdateSettings() {
    for (int i = 0; i < menu_.kSettingCount; ++i) {
        const bool selected = menu_.setting == i;
        lv_obj_set_style_bg_color(rows_[i], lv_color_hex(selected ? 0xEDF5FF : 0xFFFFFF), 0);
        lv_obj_set_style_border_color(
            rows_[i],
            lv_color_hex(selected && menu_.editing ? 0x007AFF : (selected ? 0xBCD9FF : 0xFFFFFF)),
            0);
        if (i == 2) {
            lv_label_set_text(
                values_[i], wifi_configuring_ ? "配网中" : (wifi_connected_ ? "已连接" : "未连接"));
            continue;
        }
        char text[16];
        const int value = i == 0 ? menu_.brightness : menu_.volume;
        std::snprintf(text, sizeof(text), "%d%%", value);
        lv_label_set_text(values_[i], text);
        lv_bar_set_value(bars_[i], value, LV_ANIM_ON);
    }
}
void BoxMenuDisplay::UpdateWifiDetails() {
    if (menu_.wifi_confirm) {
        lv_label_set_text(wifi_state_label_, "是否开启热点重新配网？");
        lv_label_set_text(station_label_, "切换 Wi-Fi");
        lv_label_set_text(station_ip_label_,
                          wifi_connected_ ? "确认后将断开当前网络" : "手机连接设备热点进行配置");
        lv_label_set_text(wifi_buttons_[0], "取消");
        lv_label_set_text(wifi_buttons_[1], "确认切换");
    } else {
        lv_label_set_text(wifi_state_label_,
                          wifi_configuring_ ? "配网热点已开启"
                                            : (wifi_connected_ ? "已连接 Wi-Fi" : "Wi-Fi 未连接"));
        lv_label_set_text(station_label_,
                          wifi_configuring_
                              ? "等待手机配置"
                              : (wifi_connected_ ? station_ssid_.c_str() : "暂无网络连接"));
        std::string ip =
            wifi_connected_ && !wifi_configuring_ ? "IP: " + station_ip_ : "可选择配网连接其他网络";
        lv_label_set_text(station_ip_label_, ip.c_str());
        lv_label_set_text(wifi_buttons_[0],
                          wifi_connected_ && !wifi_configuring_ ? "保持连接" : "返回");
        lv_label_set_text(wifi_buttons_[1], wifi_configuring_
                                                ? "查看配网"
                                                : (wifi_connected_ ? "切换 Wi-Fi" : "开始配网"));
    }
    for (int i = 0; i < 2; ++i) {
        const bool selected = menu_.wifi_choice == i;
        lv_obj_set_style_bg_color(wifi_buttons_[i], lv_color_hex(selected ? 0x007AFF : 0xEDF5FF),
                                  0);
        lv_obj_set_style_text_color(wifi_buttons_[i], lv_color_hex(selected ? 0xFFFFFF : 0x007AFF),
                                    0);
    }
}
void BoxMenuDisplay::SetWifiStatus(bool connected, bool configuring, const std::string& ssid,
                                   const std::string& ip) {
    DisplayLockGuard lock(this);
    wifi_connected_ = connected;
    wifi_configuring_ = configuring;
    menu_.wifi_ap_active = configuring;
    station_ssid_ = ssid;
    station_ip_ = ip;
    if (!root_)
        return;
    if (menu_.wifi_details)
        UpdateWifiDetails();
    else if (menu_.IsSettings() && !menu_.wifi_setup)
        UpdateSettings();
}
void BoxMenuDisplay::SetWifiInfo(const std::string& ssid, const std::string& url) {
    DisplayLockGuard lock(this);
    wifi_ssid_ = ssid;
    wifi_url_ = url;
    if (menu_.wifi_setup && root_) {
        lv_label_set_text(wifi_ssid_label_, ssid.empty() ? "正在开启热点..." : ssid.c_str());
        lv_label_set_text(wifi_url_label_, url.empty() ? "请稍候" : url.c_str());
    }
}
void BoxMenuDisplay::Navigate(box_menu::Action action) {
    const auto previous = menu_;
    {
        DisplayLockGuard lock(this);
        if (ota_panel_)
            return;
        menu_.Apply(action);
        if (root_) {
            Render();
            if (previous.selected != menu_.selected || previous.entered != menu_.entered ||
                previous.wifi_setup != menu_.wifi_setup ||
                previous.wifi_details != menu_.wifi_details) {
                Transition(
                    action == box_menu::Action::Left || action == box_menu::Action::Back ? -1 : 1);
            } else if (menu_.wifi_details) {
                UpdateWifiDetails();
            } else if (menu_.IsSettings() && !menu_.wifi_setup) {
                UpdateSettings();
            }
            ApplyBrightness();
        }
    }
    if (!previous.wifi_setup && menu_.wifi_setup && wifi_setup_callback_)
        wifi_setup_callback_();
    if (previous.volume != menu_.volume)
        Board::GetInstance().GetAudioCodec()->SetOutputVolume(menu_.volume);
    if (previous.editing && !menu_.editing) {
        Settings preferences("box_ui", true);
        preferences.SetInt("brightness", menu_.brightness);
        if (previous.setting == 1 && action == box_menu::Action::Enter && menu_.volume > 0)
            Application::GetInstance().PlaySound(Lang::Sounds::OGG_POPUP);
    }
    ESP_LOGI("BoxMenu", "action=%d selected=%d view=%s item=%d editing=%d brightness=%d volume=%d",
             static_cast<int>(action), menu_.selected + 1, menu_.entered ? "function" : "menu",
             menu_.setting, menu_.editing, menu_.brightness, menu_.volume);
}
BoxMenuDisplay::~BoxMenuDisplay() {
    DisplayLockGuard lock(this);
    if (dimmer_)
        lv_obj_delete(dimmer_);
    if (root_)
        lv_obj_delete(root_);
}

void BoxMenuDisplay::ShowOta(int progress, bool failed) {
    DisplayLockGuard lock(this);
    if (!root_)
        return;
    if (!ota_panel_) {
        ota_panel_ = lv_obj_create(root_);
        lv_obj_remove_style_all(ota_panel_);
        lv_obj_set_size(ota_panel_, width_, height_);
        lv_obj_set_style_bg_color(ota_panel_, lv_color_hex(0xF2F2F7), 0);
        lv_obj_set_style_bg_opa(ota_panel_, LV_OPA_COVER, 0);
        lv_obj_remove_flag(ota_panel_, LV_OBJ_FLAG_SCROLLABLE);
        Label(ota_panel_, 24, 36, 272, "系统更新");
        ota_progress_ = Label(ota_panel_, 24, 82, 272, "");
        lv_obj_set_style_text_font(ota_progress_, &box_menu_font_14, 0);
        ota_bar_ = lv_bar_create(ota_panel_);
        lv_obj_set_pos(ota_bar_, 24, 126);
        lv_obj_set_size(ota_bar_, 272, 8);
        lv_obj_set_style_bg_color(ota_bar_, lv_color_hex(0x007AFF), LV_PART_INDICATOR);
        auto hint = Label(ota_panel_, 24, 168, 272, "更新时请保持供电");
        lv_obj_set_style_text_font(hint, &box_menu_font_14, 0);
    }
    char text[80];
    snprintf(text, sizeof(text), "正在下载固件  %d%%", progress);
    lv_label_set_text(ota_progress_, failed            ? "更新失败，将返回菜单"
                                     : progress == 100 ? "校验完成，正在重启"
                                                       : text);
    lv_bar_set_value(ota_bar_, progress, LV_ANIM_OFF);
}
void BoxMenuDisplay::HideOta() {
    DisplayLockGuard lock(this);
    if (ota_panel_) {
        lv_obj_delete(ota_panel_);
        ota_panel_ = nullptr;
        ota_progress_ = nullptr;
        ota_bar_ = nullptr;
    }
}
