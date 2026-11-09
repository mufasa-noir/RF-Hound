#include feature.h

const feature_category_t g_category_wifi = {
    .name = "wifi",
    .enter = wifi_start,
    .exit = wifi_stop,
};