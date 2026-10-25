# RF Hound Feature Framework

RF Hound is an open-source, low-cost RF testing handheld built on the ESP32 and ESP32-S3. Its firmware is a small core plus independent features, such as Wi-Fi scanning and BLE inventory, that show up as apps in the on-device menu. This guide explains how features are structured, how to add one, and what we check in review.

**Current hardware:** ESP32 or ESP32-S3, a 1.8-inch TFT, navigation buttons, microSD, a LiPo power system, and an optional nRF24L01 transceiver (2.4 GHz, not sub-GHz). The first release covers Wi-Fi and BLE. Sub-GHz, NFC/RFID, IR, USB, GPIO, and counter-surveillance tools are planned for later versions.

**Responsible use:** RF Hound is for testing devices and networks you own or have explicit written permission to test. Contributors are responsible for keeping their features within the law and radio regulations of the places they are used.

## 1. How the pieces fit

The firmware has four layers. A feature only talks to the layer directly below it, which keeps features portable and keeps hardware details out of feature code.

```text
+----------------------------------------------------------+
|  features/    wifi_scan   ble_inventory   ...            |  <- what you add
+----------------------------------------------------------+
|  services     display  input  storage  power             |  <- stable headers
|               radio_wifi  radio_ble  radio_nrf24         |
+----------------------------------------------------------+
|  core         feature manager, menu, lifecycle            |
+----------------------------------------------------------+
|  ESP-IDF, hardware drivers, boards/<name>/pins.h          |
+----------------------------------------------------------+
```

### Design principles

1. **One feature runs at a time.** Features are foreground apps chosen from the TFT menu. A running feature owns the screen and the radio until it exits. This removes radio arbitration and most concurrency bugs.
2. **Features use services, never drivers.** No raw GPIO, SPI, SD, or display-controller calls in feature code.
3. **Prefer build-time checks over runtime negotiation.** Hardware presence and feature selection are Kconfig options. Runtime code still handles failures such as a missing SD card.
4. **Use what ESP-IDF already provides.** Components, Kconfig, `idf.py set-target`, and `idf.py size-components` cover what a custom plugin system would otherwise reinvent.
5. **Add abstraction when a second implementation exists.** We don't build interfaces for hardware that isn't supported yet.

### Terms

| Term | Meaning |
| --- | --- |
| Feature | A self-contained capability (for example Wi-Fi scan) that appears in the device menu. Lives in `features/<name>/` |
| Service | A component with a small public header that gives features access to the screen, buttons, storage, power, or a radio |
| Component | The ESP-IDF unit of build. Every feature and service is one |
| Kconfig | ESP-IDF's configuration system. It switches features on and off and records which hardware is present |
| Board profile | The pin map and default configuration for a physical board, in `boards/<name>/` |
| Passive / TX | Passive features only receive. TX features transmit, impersonate, or inject, and have extra rules (section 6) |

## 2. Quick start: add a feature

This walks through a minimal Wi-Fi scanner. A feature is one directory plus three small registration steps.

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

`features/wifi_scan/CMakeLists.txt`

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

### Step 4: register the feature

There are two small edits outside your directory.

Add the component name to `REQUIRES` in `main/CMakeLists.txt`:

```cmake
idf_component_register(
    SRCS "app_main.c" "features_table.c"
    INCLUDE_DIRS "."
    REQUIRES core wifi_scan ble_inventory
)
```

Add the feature to `main/features_table.c`, which is the single place the feature list exists:

```c
#include "feature.h"

#ifdef CONFIG_FEATURE_WIFI_SCAN
extern const feature_t wifi_scan_feature;
#endif
#ifdef CONFIG_FEATURE_BLE_INVENTORY
extern const feature_t ble_inventory_feature;
#endif

const feature_t *const g_features[] = {
#ifdef CONFIG_FEATURE_WIFI_SCAN
    &wifi_scan_feature,
#endif
#ifdef CONFIG_FEATURE_BLE_INVENTORY
    &ble_inventory_feature,
#endif
};
const size_t g_feature_count = sizeof(g_features) / sizeof(g_features[0]);
```

The explicit table is deliberate. Nothing is compiled or listed unless someone switched it on, so an unfinished directory can't end up in a release by accident.

### Step 5: build and run

```bash
idf.py set-target esp32s3      # or esp32
idf.py menuconfig              # RF Hound features -> enable your feature
idf.py build flash monitor
```

Build for both ESP32 and ESP32-S3 before opening a pull request.

## 3. Repository layout

```text
rf-hound/
  CMakeLists.txt             # sets EXTRA_COMPONENT_DIRS to include features/
  main/
    app_main.c               # boot, starts the core
    features_table.c         # the list of enabled features
  components/
    core/                    # feature manager, menu, lifecycle
    svc_display/             # TFT service
    svc_input/               # debounced button events
    svc_storage/             # microSD / filesystem service
    svc_power/               # battery telemetry, where hardware supports it
    radio_wifi/              # Wi-Fi scan and related APIs
    radio_ble/               # BLE scan and related APIs
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
    feature-framework.md     # this guide
```

What goes where:

- **`features/`** holds user-visible capabilities. Anything a user would pick from the menu belongs here.
- **`components/`** holds code shared by several features. If two features need the same logic, move it into a service rather than copying it or reaching into another feature.
- **`boards/`** holds pin assignments and board defaults. Pin numbers appear nowhere else.
- ESP32 vs ESP32-S3 is chosen with `idf.py set-target`. There is no per-chip directory.

The dependency rule is simple: **features depend on services, never on service internals and never on another feature.** Each service exposes one public header in its `include/` directory. Anything outside `include/` is private.

## 4. Feature interface and lifecycle

Every feature exports one `feature_t` descriptor. It uses ESP-IDF's `esp_err_t` for errors, so there is no separate status enum to learn.

```c
// components/core/include/feature.h
#pragma once
#include "esp_err.h"
#include "svc_input.h"   // button_event_t

typedef struct {
    const char *id;          // stable machine id, e.g. "wifi_scan"
    const char *name;        // label shown in the menu
    esp_err_t (*start)(void);                  // begin work; return promptly
    void      (*stop)(void);                   // must be idempotent
    void      (*on_button)(button_event_t ev); // optional, may be NULL
} feature_t;
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

1. The user picks a feature in the menu. The core calls `start()`.
2. If `start()` returns an error, the core calls `stop()` (safe, because it is idempotent), shows the error, and returns to the menu.
3. When the user presses Back, the core calls `stop()` and releases any radio the feature held.
4. Only one feature is `RUNNING` at a time. The core refuses a second `start()`.

There is no `init`/`deinit` pair. One-time setup that must outlive a single run belongs in a service.

### Rules for features

- **`start()` returns promptly.** Long-running work goes in a task you create in `start()`. Blocking in `start()` freezes the menu and can trip the watchdog.
- **`stop()` is idempotent and complete.** It may be called twice, or after a failed `start()`. When it returns, every task you created is gone and every buffer you allocated is freed.
- **`on_button()` returns quickly.** It runs on the core's input task. Hand slow work to your own task.
- **Descriptors and strings are static.** `id` and `name` must be string literals or otherwise live for the whole program.
- **`id` is stable.** Saved settings and logs may refer to it, so don't rename it casually. Use lowercase letters, digits, and underscores.
- **Prefer static buffers.** Keep task stacks modest and avoid unbounded heap use. The device has little RAM, and a leak survives across runs.
- **Log with a tag.** Use `ESP_LOGI(TAG, ...)` with your feature id as the tag.
- **Use services only.** Include public service headers and nothing from another component's private directories.
- **Handle service errors.** Hardware can be absent or fail at runtime. Check return codes and show a message rather than assuming success.

## 5. Services

Services wrap the hardware so features never touch it directly. Each service documents its behavior when the hardware is missing or fails.

| Service | What it provides | Behavior to know about |
| --- | --- | --- |
| `svc_display` | Text, lists, and status elements on the 1.8-inch TFT | Features never talk to the display controller |
| `svc_input` | Debounced button events: press, release, long-press | Pin map and active-high/low live in `boards/<name>/pins.h`. A resistor ladder or I/O expander would only change this component |
| `svc_storage` | Open, read, write, and list files; mount state | Returns errors when the card is absent, fails to mount, is full, or is removed mid-write. Features cannot mount or unmount |
| `svc_power` | Battery voltage and percentage | Present only on boards with a voltage divider or fuel gauge. No percentage is reported without a calibrated measurement path |
| `radio_wifi` | Wi-Fi scan and related operations | Receive-only API in `radio_wifi_rx.h` |
| `radio_ble` | BLE scanning and inventory | Receive-only API in `radio_ble_rx.h`. ESP32-S3 supports BLE but not Bluetooth Classic |
| `radio_nrf24` | nRF24L01 receive and transmit | Optional hardware. Separate RX and TX headers (section 6). It is a 2.4 GHz device and is never exposed as `subghz.*` |

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

## 6. Transmit-capable features and responsible use

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

## 7. Build, size budgets, and tests

### Everyday commands

```bash
idf.py set-target esp32       # or esp32s3
idf.py menuconfig             # enable or disable features and hardware options
idf.py build
idf.py size-components        # flash and RAM per component
idf.py flash monitor
```

CI builds both ESP32 and ESP32-S3. A change that breaks either target doesn't merge.

### Size budgets

Flash and RAM are tight, so CI runs `idf.py size-components` for each target and fails if a limit is exceeded. Report the measured cost of your feature in the PR description, taken from that output. Estimates are not a substitute for measurements.

### Tests

- **Host tests.** Services are plain C interfaces, so features can be built against mocks on a development machine without any radio hardware. Mock `radio_nrf24` and `svc_storage` first, since they are the hardest to reproduce on a bench.
- **Lifecycle tests.** Every feature should survive: `start` then `stop`; `stop` called twice; `start` failing and then `stop`; and `start` called while another feature is running (the core must refuse it).
- **Hardware checks.** Before merging changes that touch shared hardware, test the combined SPI case (display, SD, and nRF24 active together) and SD removal during a write.

Tests that need real radios should be clearly marked and kept out of the default CI run.

## 8. Pull request checklist

Reviewers go through this list. Working through it yourself first makes review much faster.

- [ ] The feature lives in its own `features/<id>/` directory with `feature.c`, `CMakeLists.txt`, and `Kconfig.projbuild`.
- [ ] It includes only public service headers, with nothing from another component's private files and nothing from another feature.
- [ ] It is registered in `main/CMakeLists.txt` and `main/features_table.c`, with the table entries wrapped in the matching `#ifdef CONFIG_...`.
- [ ] Hardware it needs is expressed with `depends on` in Kconfig.
- [ ] `start()` returns promptly, and long work runs in a task created there.
- [ ] `stop()` is idempotent and leaves no tasks, buffers, or radio holds behind.
- [ ] Service errors are handled, and the user sees a clear message when hardware is missing.
- [ ] It builds for both ESP32 and ESP32-S3.
- [ ] Measured flash and RAM cost from `idf.py size-components` is in the PR description.
- [ ] Lifecycle tests pass, and host tests use mocks where no hardware is needed.
- [ ] If it includes a `_tx` header, the PR says so, explains what it transmits, and states what authorization an operator needs.
- [ ] It is off by default unless it is part of the reviewed release set.

## 9. Troubleshooting

| Symptom | Likely cause | Fix |
| --- | --- | --- |
| Feature doesn't appear in the menu | Its Kconfig option is off, or it isn't in `features_table.c` | Enable it in `idf.py menuconfig` and check both `#ifdef` blocks in the table |
| `undefined reference to <id>_feature` | The option is on but the symbol name in `feature.c` differs from the `extern` in the table, or the component isn't in `main`'s `REQUIRES` | Make the names match exactly and add the component to `REQUIRES` |
| Component not found at configure time | The feature directory isn't visible to the build, or the name in `REQUIRES` doesn't match the directory name | Check `EXTRA_COMPONENT_DIRS` in the top-level `CMakeLists.txt` and the spelling of the component name |
| Option is greyed out in menuconfig | It has a `depends on` that isn't satisfied, such as `HAS_NRF24` on a board without the module | Select a board profile that has the hardware, or enable the dependency |
| Builds on ESP32 but fails on ESP32-S3 | The code uses Bluetooth Classic APIs. ESP32-S3 supports BLE only | Use BLE APIs through `radio_ble`, or guard the code by target |
| `ESP_ERR_INVALID_STATE` from a radio call | The radio isn't held for your feature, or the call came outside `start()` to `stop()` | Make radio calls only while the feature is running, from `start()`, your task, or `on_button()` |
| Watchdog reset when a feature launches | `start()` is blocking | Move the long work into a task and return from `start()` |
| Display glitches or SD errors while a radio is busy | A feature is touching the shared SPI bus directly | Go through the services, which handle bus locking |
| Menu shows the feature but selecting it shows an error | `start()` failed, often because hardware is missing | Check the returned `esp_err_t` and the serial log for the feature's tag |
| CI fails on size | A component exceeded its flash or RAM budget | Run `idf.py size-components`, find the large symbols, and trim buffers or code |

## 10. Roadmap

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
| Generated registry (script or linker section) | The explicit table is simple and easy to debug | Editing `features_table.c` by hand becomes error-prone |
| Versioned capability names (`wifi.scan >=1.0`) | One board and one set of providers means flags are enough | Two independent providers can satisfy the same capability |
| Capability manager and radio arbitration | Only one feature runs at a time | More than one feature genuinely needs to run at once |
| ABI versioning and `struct_size` | Everything is built together from source | Prebuilt feature objects or runtime-loaded modules are proposed |
| Separate API, ABI, and feature version numbers | One SDK version is enough while everything is rebuilt together | Same as above |
| `platforms/` directory | `idf.py set-target` covers ESP32 and ESP32-S3 | A second SoC family is actually being ported |
| Additional `boards/` entries | One board exists | A second physical board exists |
| Runtime permission manager | Review and the TX build gate cover current needs | Third-party code would run without review |

Runtime-loaded or downloadable feature modules are out of scope. If they are ever proposed, they would need their own design covering signature verification, compatibility checks, memory ownership, fault handling, and a threat model.
