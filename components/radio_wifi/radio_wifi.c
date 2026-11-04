#include "esp_err.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"

const char *TAG = "radio-wifi";

void radio_wifi_init(void){
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
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "Wifi AP is up for dashboard access.");
}