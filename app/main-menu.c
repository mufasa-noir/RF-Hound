#include <strings.h>
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_err.h"
#include "esp_http_server.h"

#define WIFI_AP_SSID CONFIG_ESP_WIFI_SSID
#define WIFI_AP_PASS CONFIG_ESP_WIFI_PASSWORD
#define WIFI_AP_CHANNEL CONFIG_ESP_WIFI_CHANNEL
#define WIFI_STA_MAX_CONN CONFIG_ESP_MAX_STA_CONN

static const char *TAG = "main-menu";

static esp_err_t dashboard_handler(httpd_req_t *req){

}
static httpd_uri_t root = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = dashboard_handler,
    .user_ctx = NULL,
};

void main_menu(void){
    ESP_LOGI(TAG, "Spawning main menu ...");
    ESP_LOGI(TAG, "The Web Dashboard is the default interface press the button to switch to On-Device Display!");

    menu_ap();
    menu_server();
}
void menu_server(void){
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK){
        ESP_ERROR_CHECK(httpd_register_uri_handler(server, &root));
    }
    ESP_LOGI(TAG, "Dashboard is running ...");
}
void menu_ap(void){
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&config));
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
  
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
    esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "Wifi AP is up for dashboard access.");

    esp_wifi_80211_tx()
}