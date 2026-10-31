#include <strings.h>
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "feature.h"

#define WIFI_AP_SSID CONFIG_ESP_WIFI_SSID
#define WIFI_AP_PASS CONFIG_ESP_WIFI_PASSWORD
#define WIFI_AP_CHANNEL CONFIG_ESP_WIFI_CHANNEL
#define WIFI_STA_MAX_CONN CONFIG_ESP_MAX_STA_CONN

static const char *TAG = "main-menu";

static esp_err_t option_handler(httpd_req_t *req){
    char buf[64];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) return ESP_FAIL;
    buf[len] = '\0';

    const char *mode = strstr(buf, "\"wifi\"") ? "wifi"
                     : strstr(buf, "\"ble\"")  ? "ble"
                     : NULL;
    if (!mode) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "unknown option");
        return ESP_OK;
    }
    ESP_LOGI(TAG, "Selected mode: %s", mode);
    ESP_ERROR_CHECK(esp_event_post(MAIN_MENU_EVENT, MAIN_MENU_OPTION_SELECTED, (void *)mode, strlen(mode) + 1, portMAX_DELAY));

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, "{\"ok\":true}", HTTPD_RESP_USE_STRLEN);
}

static httpd_uri_t option = {
    .uri = "/option",
    .method = HTTP_POST,
    .handler = option_handler,
    .user_ctx = NULL,
};

static esp_err_t dashboard_handler(httpd_req_t *req){
    httpd_resp_set_status(req, "200 OK");
    httpd_resp_set_type(req, "gzip/encoded");
    httpd_resp_send(req, );
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

    ESP_ERROR_CHECK(esp_event_handler_instance_register(MAIN_MENU_EVENT, ESP_EVENT_ANY_ID, &feature_find, NULL, NULL));
}
void menu_server(void){
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;
    
    if (httpd_start(&server, &config) == ESP_OK){
        ESP_ERROR_CHECK(httpd_register_uri_handler(server, &root));
        ESP_ERROR_CHECK(httpd_register_uri_handler(server, &option_handler));
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
}