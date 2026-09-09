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
    Settings preferences("box_ui");
    Settings audio("audio");
    menu_.Load(preferences.GetInt("brightness", 100), audio.GetInt("output_volume", 70));
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
    lv_obj_set_style_text_color(counter_, lv_color_hex(0x86868B), 0);
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
    lv_obj_set_style_text_color(help_, lv_color_hex(0x86868B), 0);
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
    lv_obj_set_style_shadow_opa(card, 12, 0);
    lv_obj_set_style_shadow_width(card, 10, 0);
    lv_obj_set_style_shadow_offset_y(card, 3, 0);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    if (menu_.IsSettings()) {
        for (int i = 0; i < 2; ++i) {
            rows_[i] = lv_obj_create(card);
            lv_obj_remove_style_all(rows_[i]);
            lv_obj_set_pos(rows_[i], 8, 6 + i * 56);
            lv_obj_set_size(rows_[i], 272, 54);
            lv_obj_set_style_radius(rows_[i], 12, 0);
            lv_obj_set_style_bg_opa(rows_[i], LV_OPA_COVER, 0);
            lv_obj_set_style_border_width(rows_[i], 1, 0);
            Label(rows_[i], 10, 4, 160, i == 0 ? "屏幕亮度" : "声音大小");
            values_[i] = Label(rows_[i], 192, 8, 68, "");
            lv_obj_set_style_text_font(values_[i], &box_menu_font_14, 0);
            lv_obj_set_style_text_align(values_[i], LV_TEXT_ALIGN_RIGHT, 0);
            bars_[i] = lv_bar_create(rows_[i]);
            lv_obj_set_pos(bars_[i], 12, 37);
            lv_obj_set_size(bars_[i], 248, 6);
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
    std::snprintf(text, sizeof(text), "%02d", menu_.selected + 1);
    auto number = Label(icon, 0, 10, 44, text);
    lv_obj_set_style_text_align(number, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(number, lv_color_white(), 0);
    if (menu_.selected == menu_.kPageCount - 1)
        std::snprintf(text, sizeof(text), "设置");
    else
        std::snprintf(text, sizeof(text), "测试功能 %d", menu_.selected + 1);
    Label(card, 76, 18, 194, text);
    auto subtitle = Label(card, 76, 47, 194,
                          menu_.selected == menu_.kPageCount - 1
                              ? "屏幕亮度与声音大小"
                              : (menu_.entered ? "已进入测试页面" : "探索你的新功能"));
    lv_obj_set_style_text_font(subtitle, &box_menu_font_14, 0);
    lv_obj_set_style_text_color(subtitle, lv_color_hex(0x86868B), 0);
    auto detail =
        Label(card, 18, 88, 252, menu_.entered ? "功能待开发，长按 K2 返回" : "长按 K1 打开");
    lv_obj_set_style_text_font(detail, &box_menu_font_14, 0);
    lv_obj_set_style_text_color(detail, lv_color_hex(menu_.entered ? 0x86868B : 0x007AFF), 0);
    if (!menu_.entered) {
        auto chevron = Label(card, 252, 86, 18, ">");
        lv_obj_set_style_text_color(chevron, lv_color_hex(0x007AFF), 0);
    }
    return card;
}
void BoxMenuDisplay::Render() {
    char text[24];
    lv_label_set_text(header_,
                      menu_.IsSettings() ? "设置" : (menu_.entered ? "功能页面" : "主功能"));
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
    for (int i = 0; i < menu_.kPageCount; ++i)
        lv_obj_set_style_bg_color(dots_[i], lv_color_hex(i == menu_.selected ? 0x007AFF : 0xD1D1D6),
                                  0);
}
void BoxMenuDisplay::SetX(void* object, int32_t value) {
    lv_obj_set_x(static_cast<lv_obj_t*>(object), value);
}
void BoxMenuDisplay::SetOpacity(void* object, int32_t value) {
    lv_obj_set_style_opa(static_cast<lv_obj_t*>(object), value, 0);
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
    lv_obj_set_style_opa(card_, LV_OPA_COVER, 0);
    outgoing_ = card_;
    card_ = CreateCard();
    Animate(outgoing_, SetOpacity, LV_OPA_COVER, LV_OPA_TRANSP);
    Animate(outgoing_, SetX, kCardX, kCardX - direction * 52, this, FinishTransition);
    Animate(card_, SetX, kCardX + direction * 80, kCardX);
    Animate(card_, SetOpacity, LV_OPA_TRANSP, LV_OPA_COVER);
}
void BoxMenuDisplay::ApplyBrightness() {
    // XL9555 LCD_BL is a binary output. Dim pixels without toggling the I2C backlight.
    if (dimmer_)
        lv_obj_set_style_bg_opa(dimmer_, (100 - menu_.brightness) * 255 / 100, 0);
}
void BoxMenuDisplay::UpdateSettings() {
    for (int i = 0; i < 2; ++i) {
        const bool selected = menu_.setting == i;
        lv_obj_set_style_bg_color(rows_[i], lv_color_hex(selected ? 0xEDF5FF : 0xFFFFFF), 0);
        lv_obj_set_style_border_color(
            rows_[i],
            lv_color_hex(selected && menu_.editing ? 0x007AFF : (selected ? 0xBCD9FF : 0xFFFFFF)),
            0);
        char text[16];
        const int value = i == 0 ? menu_.brightness : menu_.volume;
        std::snprintf(text, sizeof(text), "%d%%", value);
        lv_label_set_text(values_[i], text);
        lv_bar_set_value(bars_[i], value, LV_ANIM_ON);
    }
}
void BoxMenuDisplay::Navigate(box_menu::Action action) {
    const auto previous = menu_;
    {
        DisplayLockGuard lock(this);
        menu_.Apply(action);
        if (root_) {
            Render();
            if (previous.selected != menu_.selected || previous.entered != menu_.entered) {
                Transition(
                    action == box_menu::Action::Left || action == box_menu::Action::Back ? -1 : 1);
            } else if (menu_.IsSettings()) {
                UpdateSettings();
            }
            ApplyBrightness();
        }
    }
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
