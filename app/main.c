#include <stdio.h>
#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "components.h"

static const char *TAG = "rf-hound";

void app_main(void) {
     ESP_LOGI(TAG, "Starting rf-hound ...");
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_netif_init());

    esp_err_t ret = nvs_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_init();
    }
    ESP_ERROR_CHECK(ret);

    if (radio_wifi_init() == ESP_OK) {
        ESP_LOGE(TAG, "Wi-Fi initialized successfully.");
        if (server_start() == ESP_OK) {
            ESP_LOGI(TAG, "HTTP server started successfully.");
        } else {
            ESP_LOGE(TAG, "Failed to start HTTP server.");
        }
    } else {    
        ESP_LOGE(TAG, "Failed to initialize Wi-Fi.");
    }

}
