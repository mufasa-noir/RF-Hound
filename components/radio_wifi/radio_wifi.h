#pragma once
#include "esp_http_server.h"
#include "esp_err.h"

esp_err_t route_wifi_register(httpd_handle_t s);
