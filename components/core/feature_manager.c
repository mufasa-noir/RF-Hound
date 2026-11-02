#include <strings.h>
#include "esp_err.h"
#include "feature.h"

static const feature_t *current = NULL;

void feature_lookup(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    const char *id = (const char *)event_data;
    for (size_t i = 0; i < g_category_count; i++)
        if (strcmp(g_category[i]->id, id) == 0) {
            g_categories[i]->category_handler();
        }
}

esp_err_t feature_run(const char *id) {
         
}

void feature_stop_current(void) {
    if (current) { current->stop(); current = NULL; }
}