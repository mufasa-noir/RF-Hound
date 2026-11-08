#pragma once
#include "esp_http_server.h"
#include "esp_err.h"

extern bool main_opt_checker;

esp_err_t route_main_menu_register(httpd_handle_t s);
void route_main_menu_deregister(httpd_handle_t s);