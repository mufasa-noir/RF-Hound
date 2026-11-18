#include "routes.h"
#include "esp_log.h"

const char *TAG = "HTTP-SERVER";
bool main_opt_checker = false;

esp_err_t server_start(void){
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    ESP_ERROR_CHECK(httpd_start(&server, &config));

    if (!main_opt_checker){
        ESP_ERROR_CHECK(route_main_menu_register(server));
    }

    return ESP_OK;
}

esp_err_t server_stop(void){
    return ESP_OK;
}