#include "box_ota.h"
#include <esp_app_desc.h>
#include <esp_image_format.h>
#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_system.h>
#include <cJSON.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <psa/crypto.h>
#include <algorithm>
#include <cstring>
#include <memory>
#include "board.h"
#include "box_menu_display.h"
#include "box_ota_validation.h"
#include "wifi_manager.h"
#if __has_include("box_ota_private.h")
#include "box_ota_private.h"
#else
#define BOX_OTA_TOKEN ""
#endif

namespace {
constexpr auto kTag = "BoxOta";
constexpr auto kManifest = "https://ota.ainotex.com/box/latest.json";
std::string String(cJSON* object, const char* key) {
    auto value = cJSON_GetObjectItemCaseSensitive(object, key);
    return cJSON_IsString(value) && value->valuestring ? value->valuestring : "";
}
std::unique_ptr<Http> Open(const std::string& url) {
    auto http = Board::GetInstance().GetNetwork()->CreateHttp(0);
    http->SetTimeout(15000);
    http->SetHeader("Authorization", "Bearer " BOX_OTA_TOKEN);
    http->SetHeader("Accept-Encoding", "identity");
    if (!http->Open("GET", url) || http->GetStatusCode() != 200)
        return nullptr;
    return http;
}
struct WriteSession {
    esp_ota_handle_t handle = 0;
    psa_hash_operation_t hash = PSA_HASH_OPERATION_INIT;
    ~WriteSession() {
        if (handle)
            esp_ota_abort(handle);
        psa_hash_abort(&hash);
    }
};
bool Download(const std::string& url, const std::string& version, const std::string& expected_hash,
              size_t length, BoxMenuDisplay* display) {
    const auto partition = esp_ota_get_next_update_partition(nullptr);
    if (!partition || length > partition->size)
        return false;
    auto http = Open(url);
    if (!http || http->GetBodyLength() != length)
        return false;
    WriteSession session;
    if (psa_crypto_init() != PSA_SUCCESS ||
        psa_hash_setup(&session.hash, PSA_ALG_SHA_256) != PSA_SUCCESS)
        return false;
    auto buffer = std::make_unique<char[]>(4096);
    size_t total = 0;
    int last_progress = -1;
    while (total < length) {
        const auto wanted = std::min(size_t(4096), length - total);
        size_t count = 0;
        while (count < wanted) {
            const int received = http->Read(buffer.get() + count, wanted - count);
            if (received <= 0)
                return false;
            count += received;
        }
        if (total == 0) {
            constexpr auto offset = sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t);
            if (count < offset + sizeof(esp_app_desc_t))
                return false;
            esp_app_desc_t description{};
            memcpy(&description, buffer.get() + offset, sizeof(description));
            if (description.magic_word != ESP_APP_DESC_MAGIC_WORD ||
                strnlen(description.version, sizeof(description.version)) ==
                    sizeof(description.version) ||
                version != description.version ||
                memcmp(description.project_name, esp_app_get_description()->project_name,
                       sizeof(description.project_name)) != 0)
                return false;
            if (esp_ota_begin(partition, OTA_WITH_SEQUENTIAL_WRITES, &session.handle) != ESP_OK)
                return false;
        }
        if (psa_hash_update(&session.hash, reinterpret_cast<const uint8_t*>(buffer.get()), count) !=
                PSA_SUCCESS ||
            esp_ota_write(session.handle, buffer.get(), count) != ESP_OK)
            return false;
        total += count;
        int progress = std::min(99, int(total * 100 / length));
        if (progress != last_progress) {
            display->ShowOta(progress);
            if (progress % 10 == 0)
                ESP_LOGI(kTag, "Download %d%%", progress);
            last_progress = progress;
        }
    }
    uint8_t digest[32];
    size_t digest_length = 0;
    if (psa_hash_finish(&session.hash, digest, sizeof(digest), &digest_length) != PSA_SUCCESS ||
        digest_length != sizeof(digest))
        return false;
    char actual_hash[65];
    for (size_t i = 0; i < sizeof(digest); ++i)
        snprintf(actual_hash + 2 * i, 3, "%02x", digest[i]);
    if (expected_hash != actual_hash) {
        ESP_LOGE(kTag, "SHA-256 mismatch; retaining current firmware");
        return false;
    }
    auto result = esp_ota_end(session.handle);
    session.handle = 0;  // esp_ota_end frees the handle even when validation fails.
    if (result != ESP_OK || esp_ota_set_boot_partition(partition) != ESP_OK)
        return false;
    ESP_LOGI(kTag, "Verified SHA-256; boot partition=%s version=%s", partition->label,
             version.c_str());
    return true;
}
}  // namespace

void BoxOta::Start(BoxMenuDisplay* display) {
    if (strlen(BOX_OTA_TOKEN) == 0 || started_.exchange(true))
        return;
    display_ = display;
    if (xTaskCreate([](void* arg) { static_cast<BoxOta*>(arg)->Run(); }, "box_ota", 8192, this, 2,
                    nullptr) != pdPASS) {
        started_ = false;
        ESP_LOGE(kTag, "Cannot start OTA task");
    }
}
void BoxOta::Run() {
    const auto running = esp_ota_get_running_partition();
    ESP_LOGI(kTag, "Running version=%s partition=%s", esp_app_get_description()->version,
             running->label);
    // Start() runs only after the display, audio, keys and station connection initialized.
    esp_ota_img_states_t state;
    if (esp_ota_get_state_partition(running, &state) == ESP_OK &&
        state == ESP_OTA_IMG_PENDING_VERIFY)
        esp_ota_mark_app_valid_cancel_rollback();
    vTaskDelay(pdMS_TO_TICKS(5000));
    for (;;) {
        auto& wifi = WifiManager::GetInstance();
        bool ok = true;
        if (wifi.IsConnected() && !wifi.IsConfigMode())
            ok = Check();
        vTaskDelay(pdMS_TO_TICKS(ok ? 300000 : 60000));
    }
}
bool BoxOta::Check() {
    ESP_LOGI(kTag, "Checking ota.ainotex.com");
    auto http = Open(kManifest);
    if (!http || http->GetBodyLength() == 0 || http->GetBodyLength() > 4096) {
        ESP_LOGW(kTag, "Manifest unavailable; retry in 60 seconds");
        return false;
    }
    std::string body(http->GetBodyLength(), '\0');
    size_t offset = 0;
    while (offset < body.size()) {
        int count = http->Read(body.data() + offset, body.size() - offset);
        if (count <= 0)
            return false;
        offset += count;
    }
    http.reset();
    std::unique_ptr<cJSON, decltype(&cJSON_Delete)> json(cJSON_Parse(body.c_str()), cJSON_Delete);
    if (!json || String(json.get(), "board") != "atk-dnesp32s3-box")
        return false;
    const auto version = String(json.get(), "version");
    std::array<unsigned, 3> parsed;
    if (!box_ota::ParseVersion(version, parsed))
        return false;
    if (!box_ota::IsNewer(esp_app_get_description()->version, version)) {
        ESP_LOGI(kTag, "Up to date: %s", esp_app_get_description()->version);
        return true;
    }
    const auto url = String(json.get(), "url");
    const auto hash = String(json.get(), "sha256");
    auto size = cJSON_GetObjectItemCaseSensitive(json.get(), "size");
    if (!box_ota::ValidUrl(url, version) || !box_ota::ValidHash(hash) || !cJSON_IsNumber(size) ||
        size->valuedouble < 4096 || size->valuedouble > 0x3f0000 ||
        size->valuedouble != size->valueint)
        return false;
    // Navigation is locked by the overlay before network mode can be changed.
    display_->ShowOta(0);
    if (WifiManager::GetInstance().IsConfigMode()) {
        display_->HideOta();
        return false;
    }
    ESP_LOGI(kTag, "Installing BOX version=%s size=%d", version.c_str(), size->valueint);
    if (!Download(url, version, hash, size->valueint, display_)) {
        ESP_LOGW(kTag, "Update failed; current firmware retained");
        display_->ShowOta(0, true);
        vTaskDelay(pdMS_TO_TICKS(3000));
        display_->HideOta();
        return false;
    }
    display_->ShowOta(100);
    vTaskDelay(pdMS_TO_TICKS(1500));
    esp_restart();
    return true;
}
