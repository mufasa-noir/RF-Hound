# Designing the Feature API, ABI, SDK, and Manifest

## 1. Purpose and design target

This document describes a contributor-facing extension system for an embedded wireless testing tool. The aim is to let a contributor add a feature in its own directory, implement a stable contract, declare metadata and dependencies in a manifest, and have the build system validate and compile it into the firmware.

The initial design is a **compile-time plugin system**. Feature code is linked into one firmware image; it is not downloaded as executable code and dynamically loaded on the device. This keeps the first version practical for MCUs with constrained memory and no general-purpose dynamic linker.

All radio functionality should be used only on devices and networks the operator owns or is explicitly authorized to test. The framework itself should enforce clear capability boundaries rather than giving every feature unrestricted access to every driver.

## 2. Keep four concepts distinct

### API — source-level contract

The API is what a contributor sees when writing C code: types, function declarations, enums, callbacks, and documented semantics.

Example:

```c
int feature_log_info(const char *tag, const char *message);
int radio_get_channel(int *channel_out);
```

Changing a function signature is an API change because source code using it may no longer compile.

### ABI — binary-level contract

The ABI describes how compiled code and the core agree on the layout and calling convention of shared objects: struct layout, field types/order, symbol names, alignment, enum representation, and function-pointer signatures.

For a statically linked MCU firmware, the ABI still matters as an architectural contract, even though core and features are usually rebuilt together. It becomes especially important if prebuilt feature objects, separately versioned SDK packages, or runtime-loaded modules are introduced.

A C struct used across an ABI should:
- have fixed, documented field types and order;
- include an ABI version and struct size when practical;
- avoid compiler-specific types and bitfields in the public contract;
- avoid passing ownership-ambiguous heap objects across the boundary;
- avoid exposing private driver structs;
- define who owns buffers and how long pointers remain valid.

### SDK — developer package

The SDK is the supported set of public headers, helper functions, macros, documentation, and test utilities contributors use. The SDK should not expose private implementation headers.

Example layout:

```text
sdk/
  include/
    feature_api.h
    feature_types.h
    logging.h
    radio_api.h
    storage_api.h
    event_api.h
```

### Manifest — machine-readable metadata

A manifest declares what a feature is, which SDK/ABI version it expects, what capabilities it requires/provides, build inputs, resource estimates, supported targets, permissions, stability, and license. It does not replace the C interface: the manifest describes a feature; the C interface implements it.

## 3. Recommended repository layout

```text
project/
  app/
    main.c
  core/
    feature_manager.c
    feature_registry.c
    private/
  sdk/
    include/                 # public, stable headers only
  drivers/
    radio/
    storage/
  features/
    wifi_scan/
      feature.c
      CMakeLists.txt
      feature.yml
    ble_inventory/
      feature.c
      CMakeLists.txt
      feature.yml
  tools/
    validate_manifests.py
    create_feature.py
  schemas/
    feature-manifest.schema.json
  docs/
    feature-api-abi-sdk.md
  CMakeLists.txt
```

The key dependency rule is: **features depend on the SDK, not on core private headers or another feature's implementation files**. If multiple features need the same functionality, promote that functionality into a core capability/service.

## 4. The public feature interface

A practical initial interface is:

```c
#ifndef FEATURE_API_H
#define FEATURE_API_H

#include <stddef.h>
#include <stdint.h>

#define FEATURE_ABI_VERSION 1u
#define FEATURE_API_VERSION 1u

typedef enum {
    FEATURE_OK = 0,
    FEATURE_E_INVALID_ARGUMENT = -1,
    FEATURE_E_UNSUPPORTED = -2,
    FEATURE_E_BUSY = -3,
    FEATURE_E_NO_MEMORY = -4,
    FEATURE_E_IO = -5,
    FEATURE_E_PERMISSION = -6,
    FEATURE_E_INTERNAL = -7
} feature_status_t;

typedef struct feature_context feature_context_t;

typedef struct {
    uint32_t struct_size;
    uint32_t abi_version;
    const char *id;
    const char *name;
    const char *version;

    /* Called once after registration and core services are ready. */
    feature_status_t (*init)(feature_context_t *ctx);

    /* Starts work; should return promptly rather than block forever. */
    feature_status_t (*start)(feature_context_t *ctx);

    /* Requests work to stop; must be safe to call more than once. */
    void (*stop)(feature_context_t *ctx);

    /* Releases feature-owned resources after stop. */
    void (*deinit)(feature_context_t *ctx);
} feature_descriptor_t;

#endif
```

Important semantics:
- `struct_size` lets the manager reject descriptors that are too small and can support carefully designed extension in later ABI versions.
- `abi_version` identifies the binary contract expected by the feature.
- `id` is a stable machine identifier. Do not rename it casually; other configuration may refer to it.
- `version` is the feature's own semantic version, separate from SDK and ABI versions.
- callbacks should return status where failure is meaningful.
- `stop()` should be idempotent, because shutdown paths can be repeated.
- `deinit()` should release resources allocated by `init()`, even if `start()` failed.
- the context is owned by the core. Features must not free it.
- the descriptor and its string fields must have static lifetime in a static-firmware design.

For a production project, consider making `feature_context_t` opaque and providing accessors for logging, event subscriptions, configuration, storage, and radio capabilities. Do not put internal RTOS objects or hardware register addresses in the public descriptor.

## 5. Lifecycle state machine

The core should document a lifecycle such as:

```text
DISCOVERED -> INITIALIZED -> RUNNING -> STOPPED -> DEINITIALIZED
                   |           |
                   v           v
                 ERROR       ERROR
```

Suggested rules:
1. Discovery occurs at boot or build-generated registry initialization.
2. `init()` may validate configuration and acquire long-lived resources.
3. `start()` begins a bounded task or schedules work; it should not monopolize the caller.
4. `stop()` signals cancellation and waits only within a documented timeout policy.
5. `deinit()` releases resources.
6. If `init()` fails, the manager should not call `start()`.
7. If `start()` fails, the manager should call `stop()` if needed, then allow `deinit()`.
8. Avoid silently running two features that require exclusive control of the same radio. Add resource arbitration in the core.

The core should own lifecycle state and prevent invalid transitions, such as starting a feature before initialization or deinitializing while its worker is still active.

## 6. Capability-oriented SDK

A contributor should request services through stable APIs, for example:

```c
feature_status_t sdk_log_info(const char *tag, const char *message);
feature_status_t sdk_wifi_scan_start(const wifi_scan_config_t *config);
feature_status_t sdk_wifi_scan_cancel(void);
feature_status_t sdk_config_get_string(const char *key,
                                       char *buffer,
                                       size_t buffer_size);
```

Avoid an API that exposes raw driver internals. A high-level capability interface lets the implementation change from one chipset or driver to another without rewriting all features.

For wireless testing tools, model permissions explicitly. For example, a passive scan capability should not implicitly grant frame injection or transmission capabilities. The build/manifest validator can reject features requesting capabilities not enabled for that firmware profile. The runtime manager should also check permissions; manifest declarations alone are not a security boundary.

## 7. Registration and linking

There are two reasonable registration strategies.

### Strategy A: explicit generated registry — recommended for portability

A script reads manifests and generates a C source file containing references to enabled feature descriptors. CMake compiles that file and the feature sources. This is explicit, debuggable, and portable across embedded toolchains.

Conceptually generated source:

```c
extern const feature_descriptor_t wifi_scan_feature;
extern const feature_descriptor_t ble_inventory_feature;

const feature_descriptor_t *const enabled_features[] = {
    &wifi_scan_feature,
    &ble_inventory_feature,
};
const size_t enabled_feature_count =
    sizeof(enabled_features) / sizeof(enabled_features[0]);
```

Advantages:
- no linker-section magic;
- predictable behavior with static libraries and dead-code elimination;
- easy to inspect the enabled feature list;
- straightforward support for multiple toolchains.

### Strategy B: linker-section registration

A macro places pointers into a named linker section and the core iterates between linker-provided start/end symbols. This can be elegant but depends on compiler/linker support and linker-script behavior. Static-library members can be discarded unless forced into the link or referenced. Validate this technique on the exact ESP-IDF/Zephyr/GCC/LLVM toolchain and linker script before adopting it.

For an extensible public project, I'd start with **manifest-driven generated registry**, and consider linker sections only if they provide a concrete benefit.

## 8. Manifest design

A manifest is metadata, not executable code. Use YAML for contributor readability, then validate it against a versioned schema before CMake configures the build.

Recommended fields:
- `schema_version`: version of the manifest format.
- `id`: unique, stable identifier.
- `name`, `description`, `author`, `license`.
- `version`: feature version using semantic versioning.
- `sdk.api_version`, `sdk.abi_version`: expected public contract versions.
- `requires`: required core capabilities and minimum versions.
- `provides`: capabilities exported by the feature.
- `build`: source directory, entry source, and default enablement.
- `resources`: estimates for flash, RAM, and task stack.
- `permissions`: explicit hardware/security capabilities requested.
- `targets`: supported board/MCU families.
- `stability`: experimental, beta, or stable.

Example:

```yaml
schema_version: 1
id: wifi_scan
name: "Wi-Fi Scanner"
version: "1.0.0"
description: "Scans nearby Wi-Fi networks and reports discovery metadata."
author:
  name: "Example Contributor"
  url: "https://example.invalid/contributor"
license: "MIT"

sdk:
  api_version: 1
  abi_version: 1

requires:
  - capability: wifi.scan
    version: ">=1.0.0"

provides:
  - capability: wifi.scan.results
    version: "1.0.0"

build:
  default_enabled: true
  source_dir: "."
  entry_source: "feature.c"

resources:
  flash_bytes_estimate: 18000
  ram_bytes_estimate: 2400
  stack_bytes_estimate: 2048

permissions:
  - radio.wifi.scan

targets:
  - esp32
  - esp32s3

stability: stable
```

Resource values are estimates, not guarantees. The CI build should compare the final map file and runtime measurements against board budgets.

## 9. Dependency resolution

Use capabilities instead of depending on another feature's private source code.

Good:

```yaml
requires:
  - capability: wifi.scan
    version: ">=1.0.0"
```

Fragile:

```yaml
requires:
  - feature: alice_private_scanner_helper
```

The first says what service is needed, not which implementation must provide it. The build system can map `wifi.scan` to a driver/core service or a provider feature.

The resolver should:
1. parse all manifests;
2. reject duplicate IDs;
3. validate IDs and semantic versions;
4. verify the declared SDK/API/ABI version;
5. verify required capabilities exist;
6. detect dependency cycles;
7. verify target compatibility;
8. enforce allowlisted permissions;
9. produce the enabled feature list and generated registry;
10. emit a readable error with the feature ID and missing requirement.

For the first version, keep dependencies simple and reject cycles rather than trying to resolve arbitrary dependency graphs.

## 10. Build integration

A contributor should not need to edit core files. A useful workflow is:

```text
add features/my_feature/feature.c
add features/my_feature/feature.yml
add features/my_feature/CMakeLists.txt
run manifest validator
configure/build
run tests
```

CMake can include a generated `features_registry.c`. The manifest validator should run before compilation and produce a clear failure if a manifest is malformed.

Avoid silently compiling every directory found under `features/`: an incomplete experimental directory could accidentally enter a release. Prefer an explicit enabled-feature configuration, with manifests controlling defaults and a generated lock/list recording exactly what was included.

## 11. API and ABI versioning policy

Keep three independent versions:
- **feature version**: version of one feature, e.g. `1.2.0`;
- **API version**: source-level public SDK contract;
- **ABI version**: binary descriptor/calling contract.

Suggested policy:
- patch: bug fix without API/ABI break;
- minor: backward-compatible capability addition;
- major: incompatible API or ABI change.

For an MCU project that rebuilds all features from source, API compatibility is the day-to-day concern. ABI versioning still documents the boundary and prepares the project for prebuilt modules later.

Never assume `sizeof(struct)` or enum layout is stable across unrelated compilers/ABIs. If independently compiled binary modules become a goal, pin the compiler/target ABI or use a serialized message boundary instead of sharing complex C structs.

## 12. Safety, trust, and contributor review

A contributor feature is native code. Once compiled into firmware, it can potentially access everything available to that firmware. A manifest cannot sandbox it.

For a security tool:
- review every new capability request;
- keep transmit/injection capabilities separate from passive discovery;
- use test hardware and authorized targets;
- require CI builds and tests before merging;
- inspect dependency and license declarations;
- do not automatically download and execute contributor binaries on the device;
- treat resource estimates as advisory until measured;
- have release builds explicitly select reviewed features.

If runtime-loaded modules are ever introduced, treat that as a separate security project involving signature verification, compatibility checks, memory ownership, fault handling, and a clear threat model.

## 13. Suggested implementation sequence

1. Stabilize `feature_descriptor_t` and lifecycle semantics.
2. Create public SDK headers and prohibit includes from `core/private`.
3. Add manifests to the example features.
4. Write a manifest validator and schema.
5. Generate a registry source from enabled manifests.
6. Add CMake integration and a reproducible feature list.
7. Add unit tests for lifecycle and error paths.
8. Add target-specific radio implementations behind SDK abstractions.
9. Add CI builds for each supported board.
10. Document API/ABI compatibility policy before accepting third-party features.

## 14. Definition of done

A feature framework is ready for outside contributors when:
- a feature can be added without editing core source;
- invalid or duplicate manifests fail before compilation;
- API/ABI versions are checked;
- unsupported targets and missing capabilities fail clearly;
- lifecycle errors are handled predictably;
- the build can report which features and versions are in the firmware;
- flash/RAM budgets are checked for each supported target;
- SDK headers are documented and kept separate from private implementation;
- tests can run without physical radio hardware where possible.
