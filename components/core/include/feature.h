#pragma once
#include <stddef.h>
#include "esp_err.h"
#include "svc_input.h"       

#ifdef __cplusplus
extern "C" {
#endif

typedef struct feature {
    const char *id;                          
    const char *name;                     
    esp_err_t (*start)(void);
    void      (*stop)(void);
    void      (*on_button)(button_event_t ev);
} feature_t;

typedef struct feature_category {
    const char *id;          // "wifi", "ble"
    const char *name;        // "Wi-Fi", "Bluetooth"
    void (*category_handler)(void);
} feature_category_t;

extern const feature_category_t *const g_categories[];
extern const size_t g_category_count;

extern const feature_t *const g_features[];
extern const size_t g_feature_count;
extern const feature_t *feature_lookup(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

#ifdef __cplusplus
}
#endif