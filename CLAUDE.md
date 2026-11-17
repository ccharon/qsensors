# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

`qsensors` is a native Qt6 Widgets application for Linux/Wayland that displays hardware sensor data directly from `libsensors` (lm-sensors), with an xsensors-inspired LCD-style UI.

## Build & Test Commands

```bash
# Configure (with tests; -DBUILD_TESTING=OFF builds without Qt Test)
cmake -S . -B build -DBUILD_TESTING=ON

# Optional, both default OFF: warnings as errors; ASan + UBSan build
cmake -S . -B build -DQSENSORS_WARNINGS_AS_ERRORS=ON
cmake -S . -B build-asan -DQSENSORS_SANITIZE=ON -DCMAKE_BUILD_TYPE=Debug

# Hardening flags (PIE, stack protector, FORTIFY_SOURCE, RELRO) default ON; to disable
cmake -S . -B build -DQSENSORS_HARDENING=OFF

# Build
cmake --build build -j

# Run tests
ctest --test-dir build --output-on-failure

# Run the app
./build/qsensors

# Sync translations (required after any user-visible string change)
cmake --build build --target update_translations
```

## Architecture

Data flow: `SensorMonitor` timer → `SensorSource` read in a worker thread (raw readings) → `ReadingPipeline` in the UI thread (display readings) → `SensorsPanel` reconciles structure → selective widget rebuild or in-place value update → `QSettings` persistence.

**`src/sensors/`**: sensor data, rules and the libsensors integration; no Qt widgets.
- `sensor_reading.h`: the normalized model used by all layers: `SensorReading`, `SensorCategory`, `SensorUnit` (the quantity, named by its base unit: °C/°F, V, RPM, A, W, % PWM duty, %RH, J, Hz; no prefixes), `SiPrefix`, `SensorScale` (prefix and decimals of the display) and `AlertState` (None/Low/High). Values and limits are always in the base unit; `SensorReading::scale` is set by the pipeline (nullopt lets the formatter pick the natural scale for the value). `SensorReading::value` is `std::optional<double>`: nullopt when libsensors lists the sensor but cannot read it (or it reads non-finite); the reading and its card stay, so this is a value update, not a structural one.
- `sensor_source.h`: interface for hardware access; a source reports raw values only (°C, V, RPM, A, W and firmware limits). `recover(retryDue)` retries a source that is not initialized (default: never recovers); without `retryDue` it must stay cheap.
- `libsensors_source.{h,cpp}`: the libsensors `SensorSource`: init/cleanup lifecycle, chip enumeration, reading native limits (`temp*_crit` stands in for a missing or implausible `temp*_max`). Inputs: `temp`, `in`, `fan`, `curr`, `power*_input` (else `power*_average`), `energy`, `humidity`, `pwm*` (API 0x501+; libsensors 3.6.2 divides it by 2, the source multiplies back and reports percent) and `freq` (API 0x510+). PWM goes to the Fans category, energy to Power, humidity and frequency to Other. An unreadable input is reported without a value (logged once); inputs without read permission are skipped. libsensors enumerates chips only in `sensors_init`, so `readAll()` re-initializes (`sensors_cleanup` + `sensors_init`) when `HwmonWatch` reports a change; a failed (re-)init leaves the source uninitialized with `lastError()` set and the backend released (`g_backendActive` is held exactly while the instance is initialized). `recover()` runs `sensors_init` again when the retry is due or `HwmonWatch` reports a change since the failed attempt. Labels and limits are cached per chip and feature in a `RefreshingCache` (cleared on re-init and every `kFeatureCacheLifetime`, 1 min; a set with a failed limit read is not cached, so it is retried); values are read every poll.
- `refreshing_cache.h`: cache with a lifetime whose incomplete loads are not stored.
- `hwmon_watch.{h,cpp}`: per-poll snapshot of `/sys/class/hwmon` (entry names and symlink targets, so a reused `hwmonN` of another device counts); the re-init trigger, testable on a temporary directory.
- `reading_pipeline.{h,cpp}`: turns raw readings into display readings (drops placeholder firmware limits, default ranges, temperature unit, then the display scale with its per-sensor latch, kept with the unit so a °C/°F switch starts over). A setting change re-processes the last raw readings without reading the hardware.
- `sensor_monitor.{h,cpp}`: polls the source in a worker thread (source created, read and destroyed there only); the first read at startup is blocking so the window is sized to its content before it is shown, but bounded by `kDefaultStartupTimeout` (the minimum stall timeout, 10 s): past it `start()` returns without readings in the stalled state, and the read's result arrives later through the normal path (or `sourceFailed()` if the init failed); skips a poll while the previous one is still running; a source that turns uninitialized (failed init at startup, or failed re-init after a read) is kept in the worker and retried: `sourceFailed()` is emitted (not for a failed synchronous start, which `start()` returns), followed by empty readings; while failed, each polling tick calls `recover(false)` (cheap, hwmon change) and a precise single-shot timer makes a due `recover(true)` after one polling interval, doubling up to `kDefaultMaxRetryDelay` (1 min; `setRetryDelays()` for tests); the same error is logged once, a different one is logged and re-emitted via `sourceFailed()`; a successful retry emits `sourceRecovered()` and the readings, and polling goes on (`main_window` shows the error, then clears it with a notice); reports a read outstanding for longer than `stallTimeoutFor()` (3 intervals, at least 10 s) via `stalledChanged()`, which `main_window` shows in the status; the destructor waits at most 3 s for the worker and otherwise leaks the thread, so source and poller still die in the worker if the read ever returns; it then sets `workerAbandoned()`, and `main.cpp` ends with `std::_Exit` instead of tearing down `QApplication` and the statics under the still running worker (settings are synced when written).
- `sensors_policy.h`: rules applied to readings: placeholder firmware limits (0/0, max not above min, 0 K and 65535 K temperatures) dropped as if never reported, the display scale (`chooseScale`: a nonzero firmware limit picks the scale range and latches it, kept while the limit is missing; otherwise a latched range moves only when the value leaves it by a factor of 10; without a value the latch is kept), default bar graph ranges per quantity when firmware has no limits (none for energy and frequency), PWM duty to percent, alert state (`alertState`: which limit was crossed, Low or High; firmware limits only, plus 100 °C for temperatures without a firmware maximum; fans only Low, temperatures only High, PWM and energy never; never without a value) and range fraction (none without a value). `SensorReading` keeps the firmware limits (`firmwareMin/Max`, alert and tooltip) apart from the bar graph range (`minValue/maxValue`).
- `sensor_format.{h,cpp}`: the unit table (per quantity: symbol, allowed SI prefixes, most decimals) and the scale ranges it generates (four significant digits: `880 mV`, `1.052 V`, `12.10 V`, `230.1 V`, whole mA/mW, `1.234 kW`); formatting shared by the LCD and tooltips. Every value fits `kValueCells` (5) digit cells including a minus sign: the formatter decides by the formatted result, dropping decimals, then moving to the next prefix, `OL` when nothing fits; no `-0`. A missing value shows a dash in every digit cell in the layout of its scale (`----.-`), unit kept.
- `sensor_identity.h`: widget keys, the chip fingerprint and `uniqueChipNames()`: chips sharing a printed name (virtual/ACPI chips, address 0) get ` #2`, ` #3`, ... ordered by device path; unique names stay unchanged because chip names are persisted (chip order, expand state). `LibsensorsSource` applies it on every read.

**`src/config/`**: runtime configuration. `runtime_config.{h,cpp}` defines `TemperatureUnit`, polling interval bounds (1-10 s, default 2 s) and fan RPM fallback bounds (500-9999, default 5000). `app_config_store.{h,cpp}` validates and persists them via QSettings. `settings_keys.h` holds all QSettings keys. `settings_schema.{h,cpp}` handles versioned migration (current: v2); it runs once, at the start of `MainWindow::loadSettings()`, and logs every migration. The stores only read and write keys; they never migrate.

**`src/ui/`**: presentation only; business rules live in `src/sensors/`.
- `main_window`: gets the sensor source factory from `main.cpp` (libsensors) and runs it in a `SensorMonitor`; size limits (rule in `window_sizing.h`, re-evaluated on `LayoutRequest`): at most as tall as the content and at most as wide as the full layout (all columns, all cards at maximum width, scrollbar room included), never forced smaller, lifted when maximized; the first start without saved geometry fits both. Minimum widths need no code here: they follow from the layouts.
- `panels/sensors_panel`: chip-grouped layout; owns the chip expand state, the drag-and-drop chip order (absent chips keep their place before the chip they preceded, at most `kMaxAbsentChipsInOrder` of them) and the `CardGridPlan`; rebuilds a chip's cards only when its sensors change; it suspends repaints (`setUpdatesEnabled`) only around such structural changes, because re-enabling repaints every child and would defeat the LCD repaint caching on the value path. While a chip is dragged (`QDrag::exec()` runs a nested event loop with the dragged header's mouse event on the stack), structural updates are deferred and the last deferred readings are applied queued after the drag; value updates go on (`setDragExecutor()` replaces `exec()` in tests). Drops are accepted only during the panel's own drag, never from another instance. It emits `chipOrderChanged()` / `chipExpandedChanged()` for user changes only (not for restored state), and `main_window` saves them right away through `MainWindowStateStore::saveLayout()`; geometry is written on close only. A restored expand state loses only the entries of chips missing from the first readings that have any chip (`expandStateDropped()` names the collapsed ones for a notice); the chip fingerprint is still saved but no longer read. `structureChanged()` is not connected in the app; it documents and tests the structural-versus-value-update split. Its minimum width ignores chip headers and collapsed state (one column per category for the chip with most categories).
- `panels/card_grid_plan`: column planning without widgets: columns per category (most rows first), the one card width shared by all chips (so all cards always have the same size), and the minimum and maximum useful widths.
- `panels/category_row_layout`: `QLayout` per chip placing category titles and cards by the plan; `heightForWidth`, so a resize only moves cards.
- `panels/settings_panel`: polling interval, fan RPM fallback, temperature unit controls.
- `widgets/vertical_scroll_area`: vertical-only scroll area whose minimum width is the content's plus the scrollbar, and whose size hint height is the content height at the current width.
- `widgets/collapsible_section`: framed card with toggle header (optionally draggable; `dragRequested(pos)` carries the pointer position in the card, used as drag hotspot because Wayland clients cannot query the global cursor), used by both panels.
- `widgets/status_line`: status bar text with timed notices on top of the permanent status; notices arriving together are queued and shown one after another.
- `widgets/sensor_value_widget`: per-sensor card (title label above the LCD; tooltip with chip, a line when the sensor cannot be read, and limits; set only when its text changes; the fixed height follows the layout, so a font change updates it).
- `widgets/lcd_display_widget` + `lcd_segment_font` + `lcd_symbol_font`: multimeter-style LCD. `layoutFor()` (independent of the reading): the value right-aligned in `kValueCells` 7-segment cells of fixed pitch (decimal points in the gaps between cells), spare width adds ghost cells on the left, a fixed unit field at the right edge as wide as the widest symbol of the unit table, the HI/LO annunciator under it on the bar graph row, and the bar graph ending before it. The smallest card shows full-height digits, so the card widths need no change. Units and HI/LO are `lcd_symbol_font` paths (a bold narrow face built from stroked lines in code, no installed font needed) drawn with the segments' slant, colour, shadow and glow. Repaints only when the digits, unit, lit bar segments or alert state (including HI vs LO) differ from the last paint.
- `theme/app_theme.h`: sizing, spacing, LCD colors and style sheets. Change the look here, not in widget code.

**Build targets**: the link graph enforces the layering. `qsensors_model` (static, `src/config` and `src/sensors` without the libsensors source, links only `Qt::Core`) is used by `qsensors_ui` (static, `src/ui`, adds `Qt::Widgets`), which is used by the app (`main.cpp` and the libsensors source). Tests link the lowest library they need. New sources go into `QSENSORS_MODEL_SOURCES`, `QSENSORS_UI_SOURCES` or `QSENSORS_APP_SOURCES` in `CMakeLists.txt`; all lists are also scanned for translations. Includes are relative to `src/` (`#include "sensors/sensor_reading.h"`).

**Build options**: the INTERFACE library `qsensors_build_options` carries the warning flags (`-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wold-style-cast`; `-Wno-missing-field-initializers` because designated initializers rely on default member initializers) the sanitizer flags and the hardening flags. It is linked only into `qsensors_model`, `qsensors_ui`, `qsensors` and the tests, so nothing leaks to consumers. `QSENSORS_WARNINGS_AS_ERRORS` adds `-Werror`; `QSENSORS_SANITIZE` adds ASan + UBSan (`test_sensor_monitor` deliberately leaks one QThread in its shutdown-timeout case, which LeakSanitizer reports). `QSENSORS_HARDENING` (default ON) adds PIE (`CMAKE_POSITION_INDEPENDENT_CODE` after `check_pie_supported`), `-fstack-protector-strong`, `-fstack-clash-protection`, `-fcf-protection` (each probed, so unsupported ones such as `-fcf-protection` on arm64 are skipped), `-U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3` (Release, RelWithDebInfo and MinSizeRel only, and not together with `QSENSORS_SANITIZE`) and `-Wl,-z,relro,-z,now`; the `-U` keeps toolchains that define it themselves warning-free. Qt Test is required only when `BUILD_TESTING` is on. Keep the code warning-free.

**`tests/`**: 15 unit test files covering the main window sizing and status (fake source), range policy and rules, the reading pipeline, the sensor monitor (worker thread, stall reporting, retry and recovery of a failed source, and bounded shutdown, with a fake source), LCD logic (layout, repaint behaviour), segment glyph model and symbol font, unit table, scale ranges and formatting, settings persistence/migration, sensor identity, the hwmon change detection, the label/limit cache, the sensors panel (including chip layout signals, the drag position and LCD repaints on value updates), the status line, window sizing and runtime theme refresh. Treat failing tests as blockers.

## Non-Goals

- CLI parsing as primary sensor source (`sensors` command output)
- QML
- mandatory X11-specific UI path
- hardware-in-the-loop test requirements in CI
- pixel/screenshot-based UI regression tests
- major architecture rewrites without prior alignment
- packaging work beyond currently requested targets
- silent settings-schema/key migrations

## Branching & Releases

- `develop` is the development branch: all work and pull requests target it. CI (`.github/workflows/ci.yml`) runs the release build (`appimage.yml`: both AppImages, all tests, translation check) on every push to `develop` and on PRs; the AppImages are kept as workflow artifacts for 14 days.
- `main` holds released states only; `develop` is merged into `main` for a release.
- A release is cut by tagging a commit on `main` with `vX.Y.Z` (e.g. `v0.80.11`). `.github/workflows/release.yml` then verifies the tag is on `main`, matches `project(qsensors VERSION …)` in `CMakeLists.txt` and has a `## [X.Y.Z]` section in `CHANGELOG.md`, builds both AppImages via the same `appimage.yml` and creates a **draft** GitHub release.
- AppImages: `packaging/appimage/build.sh` does the whole build (system packages, Qt 6.8.3 via aqtinstall, libsensors 3.6.2 from source because the containers ship older ones, build, tests, linuxdeploy) inside a container whose glibc is the minimum glibc: `manylinux_2_28` for x86_64 (2.28), `ubuntu:24.04` for arm64 (2.39, official Qt arm64 binaries need 2.38). Both run natively on GitHub runners (`ubuntu-24.04`, `ubuntu-24.04-arm`).
- Release prep on `develop`: bump the version in `CMakeLists.txt`, turn `[Unreleased]` into `[X.Y.Z] - date` in `CHANGELOG.md`, then merge to `main` and tag.
- Dependabot (`.github/dependabot.yml`) keeps GitHub Actions up to date via PRs against `develop`.

## Working Guidelines

- prefer small, isolated commits per change package
- align briefly before larger refactors or architecture changes
- run build + tests after functional changes; failing tests are blockers
- any behavior change should add or update automated tests when feasible
- explicit, visible error reporting; no silent failure
- robust behavior if sensors configuration is missing or invalid
- avoid overwriting unrelated in-progress worktree changes

## Key Policies

- **Translation**: any new or changed user-visible string must be reflected in all supported locales (`en` English, `de` German, `fr` French, `es` Spanish, `pt` Portuguese) before the change is considered complete. CI enforces this via git diff on translation source files. Run `update_translations` target after string changes.
- **CHANGELOG.md**: update in the same commit for any user-visible, behavior-relevant, or release-noteworthy change (Keep a Changelog format).
- **Structural vs value updates**: `SensorsPanel` intentionally separates rebuilds (sensors changed) from in-place value patches (same sensors, new readings); width changes are handled by the layouts alone and never rebuild. Preserve this distinction when modifying the panel.
- **No silent settings migrations**: schema version bumps must be explicit and visible.
- **libsensors lifecycle**: `sensors_init` / `sensors_cleanup` must be paired; no leaks. All libsensors calls happen in the `SensorMonitor` worker thread.
