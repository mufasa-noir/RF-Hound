#pragma once
#include <stddef.h>
#include "esp_err.h"
#include "svc_input.h"       

#ifdef __cplusplus
extern "C" {
#endif

typedef struct feature {
    const char *name;
    const feature_category_t *category;
    esp_err_t (*start)(void *ctx);
    void      (*stop)(void *ctx);
    void      (*on_button)(button_event_t ev, void *ctx);  
    void       *ctx;
} feature_t;

typedef struct feature_category {
    const char *name;
    esp_err_t (*enter)(void);  
    void      (*exit)(void);    
} feature_category_t;

extern const feature_category_t *const g_categories[];
extern const size_t g_category_count;

extern const feature_t *const g_features[];
extern const size_t g_feature_count;
extern const feature_t *feature_lookup(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

#ifdef __cplusplus
}
#endif