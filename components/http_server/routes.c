#include "routes.h"

typedef struct route_entry {
    esp_err_t (*register_func)(httpd_handle_t s);
    void (*deregister_func)(httpd_handle_t s);
}route_entry_t;

const route_entry_t routes[] = {
    {route_main_menu_register, route_main_menu_deregister},
    {route_wifi_register, route_wifi_deregister},
    {route_ble_register, route_ble_deregister},
};

void register_handler(void){
    for (size_t i = 0; i < sizeof(routes)/sizeof(routes[0]); i++){

        if (!main_opt_checker){
            if (routes[i].register_func == route_main_menu_register) {
                continue; // Skip registering the main menu route if main_opt_checker is false
            }
    }
        ESP_ERROR_CHECK(routes[i].register_func(NULL));
    }
}
void deregister_handler(void){
    for (size_t i = 0; i < sizeof(routes)/sizeof(routes[0]); i++){
        routes[i].deregister_func(NULL);
    }
}