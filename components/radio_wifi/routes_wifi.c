#include "radio_wifi.h"
#include "routes.h"

static esp_err_t option_handler(httpd_req_t *req) {
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
    bool main_opt_checker = true;

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, "{\"ok\":true}", HTTPD_RESP_USE_STRLEN);
}
static const httpd_uri_t wifi_feature = {
    .uri = "/feature_select",
    .method = HTTP_POST,
    .handler = option_handler,
    .user_ctx = NULL,
};
static esp_err_t wifi_handler(httpd_req_t *req) {
    httpd_resp_set_status(req, "200 OK");
    httpd_resp_set_type(req, "gzip/encoded");
    return httpd_resp_send(req,  );
}
static const httpd_uri_t wifi_uri = {
    .uri = "/wifi",
    .method = HTTP_GET,
    .handler = wifi_handler,
    .user_ctx = NULL,
};
esp_err_t radio_wifi_register(httpd_handle_t s) {
    if (main_opt_checker){
        route_main_menu_deregister(httpd_handle_t s);
    }
    ESP_ERROR_CHECK(httpd_register_uri_handler(s, &wifi_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(s, &wifi_feature));
    ESP_ERROR_CHECK(esp_event_handler_register(MAIN_MENU_EVENT, MAIN_MENU_OPTION_SELECTED, (esp_event_handler_t)feature_lookup, NULL));
    return ESP_OK;
}