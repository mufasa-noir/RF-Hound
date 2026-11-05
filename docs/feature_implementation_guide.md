# RF Hound Feature Framework

RF Hound is an open-source, low-cost RF testing handheld built on the ESP32 and ESP32-S3. Its firmware is a small core plus independent features, grouped into categories such as Wi-Fi and Bluetooth, that the user picks from the main menu. This guide explains how features and categories are structured, how to add one, and what we check in review.

**Current hardware:** ESP32 or ESP32-S3, a 1.8-inch TFT, navigation buttons, microSD, a LiPo power system, and an optional nRF24L01 transceiver (2.4 GHz, not sub-GHz). The first release covers Wi-Fi and BLE. Sub-GHz, NFC/RFID, IR, USB, GPIO, and counter-surveillance tools are planned for later versions.

**Responsible use:** RF Hound is for testing devices and networks you own or have explicit written permission to test. Contributors are responsible for keeping their features within the law and radio regulations of the places they are used.

## 1. How the pieces fit

The firmware has five layers. A feature talks only to the services below it. A category lists the features it offers. The app layer is the only code that knows every category.

```text
+--------------------------------------------------------------+
|  app/          main menu, web dashboard, g_categories[]      |  <- knows every category
+--------------------------------------------------------------+
|  categories    components/<cat>/features_table.c             |  <- each category owns its features
+--------------------------------------------------------------+
|  features/     wifi_scan   ble_inventory   ...               |  <- what you add
+--------------------------------------------------------------+
|  services      display  input  storage  power                |
|                radio_wifi  radio_ble  radio_nrf24            |
+--------------------------------------------------------------+
|  core          feature manager, lifecycle, event dispatch    |
+--------------------------------------------------------------+
|  ESP-IDF, hardware drivers, boards/<name>/pins.h             |
+--------------------------------------------------------------+
```

### Design principles

1. **One feature runs at a time.** Features are foreground apps. A running feature owns the screen and the radio until it exits. This removes radio arbitration and most concurrency bugs.
2. **Features use services, never drivers.** No raw GPIO, SPI, SD, or display-controller calls in feature code.
3. **Prefer build-time checks over runtime negotiation.** Hardware presence and feature selection are Kconfig options. Runtime code still handles failures such as a missing SD card.
4. **Use what ESP-IDF already provides.** Components, Kconfig, `idf.py set-target`, and `idf.py size-components` cover what a custom plugin system would otherwise reinvent.
5. **Add abstraction when a second implementation exists.** We don't build interfaces for hardware that isn't supported yet.
6. **A category owns its feature list.** Each category defines the array of feature descriptors it offers. Menu code walks descriptors; it never hard-codes a feature name. The only global list is `g_categories[]`, defined in the app layer.
7. **Descriptors are `const` and live in flash.** Nothing in the menu path allocates memory. See section 6.

### Terms

| Term | Meaning |
| --- | --- |
| Feature | A self-contained capability (for example Wi-Fi scan) that appears in a category's menu. Lives in `features/<name>/` |
| Category | A group of related features (Wi-Fi, Bluetooth) shown as one entry in the main menu. Its descriptor and feature list live in `components/<category>/features_table.c` |
| Descriptor | A `const feature_t` or `const feature_category_t` that describes a feature or category to the core |
| Service | A component with a small public header that gives features access to the screen, buttons, storage, power, or a radio |
| Component | The ESP-IDF unit of build. Every feature, service, and category table is one |
| Kconfig | ESP-IDF's configuration system. It switches features on and off and records which hardware is present |
| Board profile | The pin map and default configuration for a physical board, in `boards/<name>/` |
| Passive / TX | Passive features only receive. TX features transmit, impersonate, or inject, and have extra rules (section 8) |

## 2. Quick start: add a feature to an existing category

A feature is one directory plus two lines in its category's table.

The service calls below show the shape of the API. The headers under `components/svc_*/include/` and `components/radio_*/include/` are the source of truth for exact names and signatures.

### Step 1: write the feature

`features/wifi_scan/feature.c`

```c
#include "feature.h"
#include "svc_display.h"
#include "svc_input.h"
#include "radio_wifi_rx.h"   // passive receive-only API

static void on_result(const wifi_ap_info_t *ap, void *user)
{
    svc_display_list_add(ap->ssid, ap->rssi);
}

static esp_err_t start(void)
{
    svc_display_clear();
    svc_display_title("Wi-Fi scan");
    return radio_wifi_scan_start(on_result, NULL);
}

static void stop(void)
{
    radio_wifi_scan_stop();   // safe to call when no scan is running
}

static void on_button(button_event_t ev)
{
    if (ev.id == BTN_UP)   svc_display_list_scroll(-1);
    if (ev.id == BTN_DOWN) svc_display_list_scroll(+1);
}

const feature_t wifi_scan_feature = {
    .id        = "wifi_scan",
    .name      = "Wi-Fi Scan",
    .start     = start,
    .stop      = stop,
    .on_button = on_button,
};
```

A feature does not name its category. Membership is decided by the category's table, in one place, at build time.

The Back button is handled by the core: it always calls your `stop()` and returns to the menu, so features don't implement their own exit.

### Step 2: declare the Kconfig option

`features/wifi_scan/Kconfig.projbuild`

```kconfig
config FEATURE_WIFI_SCAN
    bool "Wi-Fi scanner"
    default y
```

If your feature needs optional hardware, say so here. The build then refuses to enable it on boards without that hardware:

```kconfig
config FEATURE_NRF24_SNIFFER
    bool "nRF24 sniffer"
    depends on HAS_NRF24
    default n
```

### Step 3: write the component's CMakeLists.txt

```cmake
if(CONFIG_FEATURE_WIFI_SCAN)
    idf_component_register(
        SRCS "feature.c"
        INCLUDE_DIRS "."
        REQUIRES core svc_display svc_input radio_wifi
    )
else()
    idf_component_register()   # disabled: registers as an empty component
endif()
```

### Step 4: link the feature into its category

`components/radio_wifi/features_table.c` owns the Wi-Fi category and the list of features it offers:

```c
#include "feature.h"

extern const feature_t wifi_scan_feature;
extern const feature_t wifi_deauth_feature;

static const feature_t *const wifi_features[] = {
    &wifi_scan_feature,
    &wifi_deauth_feature,
};

const feature_category_t wifi_category = {
    .id               = "wifi",
    .name             = "Wi-Fi",
    .features         = wifi_features,
    .feature_count    = sizeof(wifi_features) / sizeof(wifi_features[0]),
    .category_handler = NULL,   // NULL: the core draws a default list
};
```

Adding a feature to an existing category is two lines: the `extern` and one pointer in the array. Wrap both in the feature's `#ifdef CONFIG_...` so a disabled feature is never referenced.

The category's component needs the feature components in its `REQUIRES`:

```cmake
idf_component_register(
    SRCS "radio_wifi.c" "features_table.c"
    INCLUDE_DIRS "include"
    REQUIRES core radio_wifi_rx wifi_scan
)
```

### Step 5: build and run

```bash
idf.py set-target esp32s3      # or esp32
idf.py menuconfig              # RF Hound features -> enable your feature
idf.py build flash monitor
```

Build for both ESP32 and ESP32-S3 before opening a pull request.

## 3. Adding a new category

A new category is a descriptor in a component plus one entry in the app's global list.

1. Create `components/<category>/features_table.c` with the category descriptor and its feature pointer array, as in step 4 above. Set `category_handler` if the category needs a custom screen; leave it `NULL` to get the core's default list.
2. Add `&<category>_category` to `g_categories[]` in `app/features_table.c`, wrapped in the category's Kconfig option. This is the only place the menu learns that the category exists.
3. Give the category a Kconfig option and put its component in the app's `REQUIRES`.
4. If the category's submenu needs behaviour the default list cannot express (grouping, an on-screen keyboard, a settings page), implement `category_handler()`. It must return promptly; it runs on the caller's task.

```c
// app/features_table.c — the only global list in the firmware
#include "feature.h"

extern const feature_category_t wifi_category;
extern const feature_category_t ble_category;

const feature_category_t *const g_categories[] = {
#ifdef CONFIG_CATEGORY_WIFI
    &wifi_category,
#endif
#ifdef CONFIG_CATEGORY_BLE
    &ble_category,
#endif
};
const size_t g_category_count = sizeof(g_categories) / sizeof(g_categories[0]);
```

The explicit table is deliberate. Nothing is compiled or listed unless someone switched it on, so an unfinished directory can't end up in a release by accident.

## 4. Repository layout

```text
rf-hound/
  CMakeLists.txt             # sets EXTRA_COMPONENT_DIRS to include features/
  app/
    main.c                   # boot, starts the core
    main-menu.c              # web dashboard, AP setup, menu event wiring
    features_table.c         # g_categories[] — the only global registry
  components/
    core/                    # feature manager, dispatch, lifecycle
      include/feature.h
      feature_manager.c
    http_server/             # HTTP plumbing shared by the menu
    radio_wifi/
      features_table.c       # wifi_category + its feature list
      radio_wifi.c
    radio_ble/
      features_table.c       # ble_category + its feature list
    svc_display/
    svc_input/
    svc_storage/
    svc_power/
    radio_nrf24/             # optional, behind CONFIG_HAS_NRF24
  features/
    wifi_scan/
      feature.c
      CMakeLists.txt
      Kconfig.projbuild
    ble_inventory/
      ...
  boards/
    handheld/
      pins.h                 # every GPIO and SPI assignment
      sdkconfig.defaults     # defaults, including HAS_NRF24 where fitted
  docs/
    feature_implementation_guide.md
```

What goes where:

- **`app/`** is the only layer that knows every category. It assembles `g_categories[]`, starts the menu, and owns boot.
- **`components/<category>/features_table.c`** is where a category and its feature list live. A category never references another category's features.
- **`features/`** holds user-visible capabilities. Anything a user would pick from a category menu belongs here.
- **`components/`** also holds services shared by several features. If two features need the same logic, move it into a service rather than copying it or reaching into another feature.
- **`boards/`** holds pin assignments and board defaults. Pin numbers appear nowhere else.
- ESP32 vs ESP32-S3 is chosen with `idf.py set-target`. There is no per-chip directory.

The dependency rule is simple: **features depend on services; categories depend on the features they list; app depends on categories. Never the reverse.** Each service exposes one public header in its `include/` directory. Anything outside `include/` is private.

A feature does not include a category header, and a category does not include another category's header. If two categories need the same helper, it belongs in a service.

## 5. Feature interface and lifecycle

There are two descriptors: a leaf `feature_t` and a group `feature_category_t`. Both are `const` and live in flash.

```c
// components/core/include/feature.h
#pragma once
#include <stddef.h>
#include "esp_err.h"
#include "esp_event.h"
#include "svc_input.h"   // button_event_t

#ifdef __cplusplus
extern "C" {
#endif

typedef struct feature {
    const char *id;          // stable machine id, e.g. "wifi_scan"
    const char *name;        // label shown in the menu
    esp_err_t (*start)(void);                  // begin work; return promptly
    void      (*stop)(void);                   // must be idempotent
    void      (*on_button)(button_event_t ev); // optional, may be NULL
} feature_t;

typedef struct feature_category {
    const char *id;                            // "wifi"
    const char *name;                          // "Wi-Fi"
    const feature_t *const *features;          // array of pointers, in flash
    size_t feature_count;
    void (*category_handler)(void);            // optional; NULL = default list
} feature_category_t;

extern const feature_category_t *const g_categories[];
extern const size_t g_category_count;

esp_err_t feature_run(const feature_t *f);
void      feature_stop_current(void);
void      feature_dispatch_category(const char *category_id);

#ifdef __cplusplus
}
#endif
```

### How selection flows

```text
 user picks "wifi"            event posts "wifi"
 (web POST /option, or  -->   MAIN_MENU_OPTION_SELECTED  -->  feature_dispatch_category("wifi")
  on-device button)                                                    |
                                                                       v
                                                    walk g_categories[] to find id
                                                                       |
                                        category_handler ? call it : default list
                                                                       |
                                                     user picks a feature_t*
                                                                       |
                                                            feature_run(f)
```

`feature_dispatch_category` is the renamed, split version of the old `feature_find`. It is a thin event adapter: it does one lookup by category id and hands off. It does not return a feature; features are selected from the category's own list.

```c
void feature_dispatch_category(const char *category_id)
{
    for (size_t i = 0; i < g_category_count; i++) {
        const feature_category_t *c = g_categories[i];
        if (strcmp(c->id, category_id) != 0) continue;
        if (c->category_handler) c->category_handler();
        else                     feature_show_default_list(c);
        return;                                  // stop at the first match
    }
    ESP_LOGW(TAG, "No category '%s'", category_id);
}
```

`feature_run` takes the descriptor, not a string. The submenu already has the pointer, so no string lookup happens on the run path:

```c
esp_err_t feature_run(const feature_t *f)
{
    if (!f) return ESP_ERR_INVALID_ARG;
    feature_stop_current();
    current = f;
    esp_err_t err = f->start();
    if (err != ESP_OK) current = NULL;   // stop() stays safe after a failed start
    return err;
}

void feature_stop_current(void)
{
    if (current) { current->stop(); current = NULL; }
}
```

### Lifecycle

```text
                 start() ok
    IDLE ---------------------> RUNNING
     ^  \                          |
     |   \ start() fails           | Back button, or the feature
     |    +--> stop() called,      | asks to exit
     |         error shown         v
     +---------------------- stop() called
```

1. The user picks a category, then a feature in it. The core calls `start()`.
2. If `start()` returns an error, the core calls `stop()` (safe, because it is idempotent), shows the error, and returns to the category menu.
3. When the user presses Back, the core calls `stop()` and releases any radio the feature held.
4. Only one feature is `RUNNING` at a time. The core refuses a second `start()`.

There is no `init`/`deinit` pair. One-time setup that must outlive a single run belongs in a service.

### Rules for features

- **`start()` returns promptly.** Long-running work goes in a task you create in `start()`. Blocking in `start()` freezes the menu and can trip the watchdog.
- **`stop()` is idempotent and complete.** It may be called twice, or after a failed `start()`. When it returns, every task you created is gone and every buffer you allocated is freed.
- **`on_button()` returns quickly.** It runs on the core's input task. Hand slow work to your own task.
- **Descriptors and strings are static.** `id` and `name` must be string literals or otherwise live for the whole program.
- **`id` is stable.** Saved settings and logs may refer to it, so don't rename it casually. Use lowercase letters, digits, and underscores.
- **Log with a tag.** Use `ESP_LOGI(TAG, ...)` with your feature id as the tag.
- **Use services only.** Include public service headers and nothing from another component's private directories.
- **Handle service errors.** Hardware can be absent or fail at runtime. Check return codes and show a message rather than assuming success.

### Rules for categories

- **A category lists features; it does not implement them.** The list is a `const` pointer array in `features_table.c`.
- **`category_handler()` is optional and must return promptly.** Leave it `NULL` to get the core's default list. Implement it only when the default list cannot express the screen you need.
- **A category never includes another category's header.** Shared helpers go in a service.
- **Membership is build-time.** Wrap each pointer and each `extern` in the feature's `#ifdef CONFIG_...`.

## 6. Memory rules

The device has little RAM and a lot of it is already spent on the Wi-Fi AP and the HTTP server before any feature runs. The menu and dispatch path must not add to that.

- **Descriptors are `const`.** `feature_t` and `feature_category_t` instances, the per-category feature pointer arrays, and `g_categories[]` are all `const`. They belong in flash, not DRAM. After adding entries, check `idf.py size` and the map file to confirm the tables landed in the flash-mapped read-only segment; add a section attribute if they did not.
- **No allocation in dispatch.** `feature_dispatch_category`, the default list, and `feature_run` use no heap. Do not copy the category string; compare in place.
- **No per-feature context pointers in the descriptors.** A menu handler that needs state uses a `static` variable. A `void *ctx` field costs four bytes per descriptor and invites heap-backed contexts, which is exactly what we are avoiding.
- **Static buffers over heap.** A leak in a feature survives across runs, because only `stop()` runs between them. Prefer fixed buffers sized at build time.
- **Keep the HTTP server cheap.** Trim `stack_size`, `max_open_sockets`, and the socket buffers in the `httpd_config_t` you pass to `httpd_start`. Serve the dashboard from a pre-compressed `static const` array in flash; never gzip at runtime into a heap buffer.
- **Measure, don't guess.** Record `idf.py size-components` before and after, and at runtime log `heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL)` at idle and with the AP up. The minimum is the number that tells you whether you are about to run out under load.

## 7. Services

Services wrap the hardware so features never touch it directly. Each service documents its behavior when the hardware is missing or fails.

| Service | What it provides | Behavior to know about |
| --- | --- | --- |
| `svc_display` | Text, lists, and status elements on the 1.8-inch TFT | Features never talk to the display controller |
| `svc_input` | Debounced button events: press, release, long-press | Pin map and active-high/low live in `boards/<name>/pins.h`. A resistor ladder or I/O expander would only change this component |
| `svc_storage` | Open, read, write, and list files; mount state | Returns errors when the card is absent, fails to mount, is full, or is removed mid-write. Features cannot mount or unmount |
| `svc_power` | Battery voltage and percentage | Present only on boards with a voltage divider or fuel gauge. No percentage is reported without a calibrated measurement path |
| `radio_wifi` | Wi-Fi scan and related operations | Receive-only API in `radio_wifi_rx.h` |
| `radio_ble` | BLE scanning and inventory | Receive-only API in `radio_ble_rx.h`. ESP32-S3 supports BLE but not Bluetooth Classic |
| `radio_nrf24` | nRF24L01 receive and transmit | Optional hardware. Separate RX and TX headers (section 8). It is a 2.4 GHz device and is never exposed as `subghz.*` |

`http_server` is not a feature-facing service. It holds HTTP plumbing used by the app's menu and the web dashboard. Features do not include it; if a feature needs to serve pages, it asks the app layer for a registration hook rather than starting a server of its own.

### When hardware is missing or the radio isn't held

Services return standard ESP-IDF errors rather than crashing:

- `ESP_ERR_NOT_FOUND`: the hardware isn't present or failed to initialize (no SD card, no nRF24 fitted).
- `ESP_ERR_INVALID_STATE`: the call isn't valid right now, for example a radio call made when the core isn't holding that radio for your feature, or outside the window between `start()` and `stop()`.

Because the build already hides features whose hardware isn't configured, these errors cover runtime surprises, such as a loose module or a card pulled out.

### Shared SPI bus

The display, microSD, and nRF24L01 may share one SPI host. The services own their SPI device handles and use ESP-IDF's bus locking, so features never see the bus and never need to arbitrate it. If you are changing a service that touches SPI, test the combined case: display refreshing while the SD card writes while the nRF24 receives.

### Rules for adding or changing a service

- One public header per service, in its `include/` directory.
- Document ownership: who allocates a buffer, who frees it, and how long a returned pointer stays valid.
- Don't expose driver structs, register addresses, or RTOS handles in public headers.
- Add a service only when at least two features need the same functionality. Otherwise keep it inside the feature.

## 8. Transmit-capable features and responsible use

A passive feature listens. A TX feature transmits, impersonates a device or network, or injects frames. Both are welcome, but TX features have to clear a higher bar, because they can affect other people's equipment.

### How the project enforces the split

- **Separate headers.** Each radio service has a receive-only header and, where it applies, a transmit header: for example `radio_nrf24_rx.h` and `radio_nrf24_tx.h`. A passive feature includes only the `_rx` header.
- **A build gate.** Any feature that includes a `_tx` header is compiled only when `CONFIG_ALLOW_TX_FEATURES` is enabled. It defaults to off, so a default build contains no transmit features.
- **Explicit review.** A pull request that adds a `_tx` include is flagged and reviewed for authorization, scope, and safety. Say in the PR description that your feature transmits.
- **Reviewed release builds.** Release firmware enables only features that have passed review.

### What a TX feature should do

- Make transmission obvious on screen while it is active.
- Stop transmitting immediately in `stop()`, including on a failed `start()`.
- Keep power, duration, and channel inside what the hardware and local regulations allow.
- Document, in the feature's README or header comment, what it transmits and what authorization the operator needs.

### What this does not do

A feature is native code linked into the firmware, so it can reach anything the firmware can. The header split and the build gate catch accidents and make intent visible in review. They are not a sandbox or a security boundary, and a feature declaration is not proof of what the code does. That is why review matters.

Use RF Hound only on devices and networks you own or have explicit written permission to test. Features and contributions that target third parties without authorization will not be accepted.

## 9. Build, size budgets, and tests

### Everyday commands

```bash
idf.py set-target esp32       # or esp32s3
idf.py menuconfig             # enable or disable categories, features, and hardware options
idf.py build
idf.py size-components        # flash and RAM per component
idf.py size                   # segment map: confirm const tables are in flash
idf.py flash monitor
```

CI builds both ESP32 and ESP32-S3. A change that breaks either target doesn't merge.

### Size budgets

Flash and RAM are tight, so CI runs `idf.py size-components` for each target and fails if a limit is exceeded. Report the measured cost of your feature in the PR description, taken from that output. Estimates are not a substitute for measurements. If a new `const` table shows up in a DRAM segment in `idf.py size`, fix the placement before merging.

### Tests

- **Host tests.** Services are plain C interfaces, so features can be built against mocks on a development machine without any radio hardware. Mock `radio_nrf24` and `svc_storage` first, since they are the hardest to reproduce on a bench.
- **Dispatch tests.** Feed a category id to `feature_dispatch_category` and check it resolves to the right category and stops at the first match. Feed an unknown id and check the warning path.
- **Lifecycle tests.** Every feature should survive: `start` then `stop`; `stop` called twice; `start` failing and then `stop`; and `start` called while another feature is running (the core must refuse it).
- **Hardware checks.** Before merging changes that touch shared hardware, test the combined SPI case (display, SD, and nRF24 active together) and SD removal during a write.

Tests that need real radios should be clearly marked and kept out of the default CI run.

## 10. Pull request checklist

Reviewers go through this list. Working through it yourself first makes review much faster.

- [ ] The feature lives in its own `features/<id>/` directory with `feature.c`, `CMakeLists.txt`, and `Kconfig.projbuild`.
- [ ] It is linked into its category in `components/<category>/features_table.c` with an `extern` and a pointer array entry, both wrapped in the matching `#ifdef CONFIG_...`.
- [ ] A new category is added to `g_categories[]` in `app/features_table.c` and the app's `REQUIRES`.
- [ ] It includes only public service headers, with nothing from another component's private files and nothing from another feature or category.
- [ ] Hardware it needs is expressed with `depends on` in Kconfig.
- [ ] `start()` returns promptly, and long work runs in a task created there.
- [ ] `stop()` is idempotent and leaves no tasks, buffers, or radio holds behind.
- [ ] Service errors are handled, and the user sees a clear message when hardware is missing.
- [ ] The menu and dispatch path add no heap allocation; new descriptors are `const` and land in flash per `idf.py size`.
- [ ] It builds for both ESP32 and ESP32-S3.
- [ ] Measured flash and RAM cost from `idf.py size-components` is in the PR description.
- [ ] Lifecycle and dispatch tests pass, and host tests use mocks where no hardware is needed.
- [ ] If it includes a `_tx` header, the PR says so, explains what it transmits, and states what authorization an operator needs.
- [ ] It is off by default unless it is part of the reviewed release set.

## 11. Troubleshooting

| Symptom | Likely cause | Fix |
| --- | --- | --- |
| Category doesn't appear in the main menu | Its Kconfig option is off, or it isn't in `g_categories[]` | Enable it in `idf.py menuconfig` and check the `#ifdef` in `app/features_table.c` |
| Category opens but is empty | No feature pointers in its `features_table.c`, or every entry is behind a disabled `#ifdef` | Check the pointer array and the feature Kconfig options |
| `undefined reference to <id>_feature` | The option is on but the symbol name in `feature.c` differs from the `extern` in the category table, or the feature component isn't in the category's `REQUIRES` | Make the names match exactly and add the component to `REQUIRES` |
| `undefined reference to wifi_category` / `ble_category` | The category's `features_table.c` isn't compiled, or the category component isn't in the app's `REQUIRES` | Check the category's `CMakeLists.txt` `SRCS` and the app's `REQUIRES` |
| Two categories both list the same feature, or a feature appears twice | The feature pointer was added to more than one table, or duplicated in one array | Decide which category owns it; remove the other entry |
| Component not found at configure time | The feature directory isn't visible to the build, or the name in `REQUIRES` doesn't match the directory name | Check `EXTRA_COMPONENT_DIRS` in the top-level `CMakeLists.txt` and the spelling of the component name |
| Option is greyed out in menuconfig | It has a `depends on` that isn't satisfied, such as `HAS_NRF24` on a board without the module | Select a board profile that has the hardware, or enable the dependency |
| Builds on ESP32 but fails on ESP32-S3 | The code uses Bluetooth Classic APIs. ESP32-S3 supports BLE only | Use BLE APIs through `radio_ble`, or guard the code by target |
| `ESP_ERR_INVALID_STATE` from a radio call | The radio isn't held for your feature, or the call came outside `start()` to `stop()` | Make radio calls only while the feature is running, from `start()`, your task, or `on_button()` |
| Watchdog reset when a feature launches | `start()` is blocking | Move the long work into a task and return from `start()` |
| Display glitches or SD errors while a radio is busy | A feature is touching the shared SPI bus directly | Go through the services, which handle bus locking |
| Selecting a feature shows an error | `start()` failed, often because hardware is missing | Check the returned `esp_err_t` and the serial log for the feature's tag |
| CI fails on size, or DRAM looks unexpectedly full | A `const` table was placed in a DRAM segment, or a feature added a large static buffer | Run `idf.py size` to check placement and `size-components` to find the large symbols; fix the placement or trim buffers |

## 12. Roadmap

The framework is intentionally small. Pieces that add complexity are deferred until a real need appears, and this section says what that need is. If you think one of them is due, open an issue describing the problem it would solve.

### Planned features

| Area | Status |
| --- | --- |
| Wi-Fi and BLE features | First release |
| Sub-GHz | Planned. Needs a real radio driver and hardware first; it will arrive as a new `radio_subghz` service, separate from the 2.4 GHz nRF24 |
| NFC/RFID, IR, USB, GPIO tools | Planned for later versions |
| Counter-surveillance detection | Planned for later versions |

### Framework pieces deliberately left out of v1

| Not in v1 | Why | Revisit when |
| --- | --- | --- |
| Feature manifest files (YAML plus schema and validator) | Kconfig and ESP-IDF components already cover enabling, dependencies, and hardware checks | Outside contributors submit features regularly, or the feature list outgrows a hand-edited table |
| Generated registry (script or linker section) | Hand-edited tables are simple, easy to debug, and cost no RAM; a constructor-based registry would put pointers in DRAM | Editing `features_table.c` by hand becomes error-prone, or a section-based registry can be shown to save measurable RAM |
| Versioned capability names (`wifi.scan >=1.0`) | One board and one set of providers means flags are enough | Two independent providers can satisfy the same capability |
| Capability manager and radio arbitration | Only one feature runs at a time | More than one feature genuinely needs to run at once |
| ABI versioning and `struct_size` | Everything is built together from source | Prebuilt feature objects or runtime-loaded modules are proposed |
| Separate API, ABI, and feature version numbers | One SDK version is enough while everything is rebuilt together | Same as above |
| `platforms/` directory | `idf.py set-target` covers ESP32 and ESP32-S3 | A second SoC family is actually being ported |
| Additional `boards/` entries | One board exists | A second physical board exists |
| Runtime permission manager | Review and the TX build gate cover current needs | Third-party code would run without review |

Runtime-loaded or downloadable feature modules are out of scope. If they are ever proposed, they would need their own design covering signature verification, compatibility checks, memory ownership, fault handling, and a threat model.