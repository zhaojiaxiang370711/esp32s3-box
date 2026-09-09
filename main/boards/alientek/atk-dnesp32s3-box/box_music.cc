#include "box_music.h"
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <algorithm>
#include "application.h"
#include "assets.h"
#include "box_menu_display.h"
#include "box_music_data.h"

uint32_t BoxMusic::Position() const {
    return position_ + (playing_ ? uint32_t((esp_timer_get_time() - anchor_) / 1000) : 0);
}
void BoxMusic::Start(BoxMenuDisplay* display) {
    display_ = display;
    auto result = xTaskCreate([](void* arg) { static_cast<BoxMusic*>(arg)->Run(); }, "box_music",
                              4096, this, 2, nullptr);
    ESP_ERROR_CHECK(result == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
}
void BoxMusic::Action(box_menu::Action action) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (action == box_menu::Action::Left || action == box_menu::Action::Right) {
        track_ = (track_ + (action == box_menu::Action::Left ? 2 : 1)) % 3;
        position_ = 0;
    } else if (action == box_menu::Action::Enter) {
        position_ = Position();
        playing_ = !playing_;
    } else {
        playing_ = false;
        position_ = 0;
    }
    anchor_ = esp_timer_get_time();
    ++generation_;
    ESP_LOGI("BoxMusic", "Track=%d playing=%d position=%lu", track_ + 1, playing_, position_);
}
void BoxMusic::Run() {
    box_music::Track tracks[3];
    // The board is constructed before the application finishes loading its assets.
    vTaskDelay(pdMS_TO_TICKS(2000));
    for (int i = 0; i < 3; ++i) {
        char name[24];
        snprintf(name, sizeof(name), "box_music_%d.bxm", i);
        void* data = nullptr;
        size_t size = 0;
        if (Assets::GetInstance().GetAssetData(name, data, size) && tracks[i].Load(data, size))
            ESP_LOGI("BoxMusic", "Loaded track=%d duration=%lu ms", i + 1, tracks[i].Count() * 60);
        else
            ESP_LOGW("BoxMusic", "Missing or invalid track=%d", i + 1);
    }
    uint32_t generation = 0, next = 0;
    int64_t last_ui = 0;
    auto& audio = Application::GetInstance().GetAudioService();
    for (;;) {
        int track;
        bool playing;
        uint32_t position, command;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            track = track_;
            playing = playing_;
            position = Position();
            command = generation_;
        }
        const auto duration = tracks[track].Count() * 60;
        if (generation != command) {
            // At most 180 ms is prequeued; resume from the elapsed media position.
            if (generation)
                audio.ResetDecoder();
            generation = command;
            next = position / 60;
            last_ui = 0;
        }
        if (playing && duration && position >= duration) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (generation_ == command) {
                track_ = (track_ + 1) % 3;
                position_ = 0;
                anchor_ = esp_timer_get_time();
                ++generation_;
            }
            continue;
        }
        if (playing && duration && next < tracks[track].Count() && next * 60 <= position + 120) {
            size_t size;
            auto data = tracks[track].Packet(next, size);
            auto packet = std::make_unique<AudioStreamPacket>();
            packet->sample_rate = 24000;
            packet->frame_duration = 60;
            packet->payload.assign(data, data + size);
            if (audio.PushPacketToDecodeQueue(std::move(packet), false))
                ++next;
        }
        if (esp_timer_get_time() - last_ui >= 250000) {
            display_->SetMusicStatus(track, playing && duration, std::min(position, duration),
                                     duration);
            last_ui = esp_timer_get_time();
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
