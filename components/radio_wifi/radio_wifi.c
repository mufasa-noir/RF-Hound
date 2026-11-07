#include "esp_err.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "radio_wifi.h"

#define WIFI_AP_SSID CONFIG_ESP_WIFI_SSID
#define WIFI_AP_PASS CONFIG_ESP_WIFI_PASSWORD
#define WIFI_AP_CHANNEL CONFIG_ESP_WIFI_CHANNEL
#define WIFI_STA_MAX_CONN CONFIG_ESP_MAX_STA_CONN

const char *TAG = "radio-wifi";
static bool wifi_initialized = false;

esp_err_t radio_wifi_init(void){
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&config));
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
  
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = WIFI_AP_SSID,
            .ssid_len = strlen(WIFI_AP_SSID),
            .password = WIFI_AP_PASS,
            .channel = WIFI_AP_CHANNEL,
            .max_connection = WIFI_STA_MAX_CONN,
            .authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    esp_wifi_set_config(WIFI_IF_MAX, &wifi_config);
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "Wifi AP is up for dashboard access.");
    wifi_initialized = true;
    return ESP_OK;
}

esp_err_t radio_wifi_deinit(void){
    ESP_ERROR_CHECK(esp_wifi_stop());
    ESP_ERROR_CHECK(esp_wifi_deinit());
    ESP_LOGI(TAG, "Wifi AP is down.");
    wifi_initialized = false;
    return ESP_OK;
}

esp_err_t wifi_start(void){
    if (!wifi_initialized) {
       ESP_ERROR_CHECK(radio_wifi_init());
    }
    route_wifi_register(NULL);
    return ESP_OK;
}

void wifi_stop(void){
    if (wifi_initialized) {
        radio_wifi_deinit();
    }
}