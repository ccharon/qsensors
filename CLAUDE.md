# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

`qsensors` is a native Qt6 Widgets application for Linux/Wayland that displays hardware sensor data directly from `libsensors` (lm-sensors), with an xsensors-inspired LCD-style UI. See `AGENTS.md` for the full project policy document.

## Build & Test Commands

```bash
# Configure (with tests)
cmake -S . -B build -DBUILD_TESTING=ON

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

Data flow: polling timer → `SensorsBackend` → normalized `SensorReading` list → `SensorsPanel` reconciles structure → selective widget rebuild or in-place value update → `QSettings` persistence.

**`src/sensors/`**: sensor data, rules and the libsensors integration; no Qt widgets.
- `sensor_reading.h`: the normalized model (`SensorReading`, `SensorUnit`, `SensorCategory`, unit symbols) used by all layers.
- `sensors_backend.{h,cpp}`: libsensors init/cleanup lifecycle, chip enumeration, reading limits; produces `SensorReading` lists. Temperature unit conversion happens here.
- `sensors_policy.h`: rules applied to readings: mA/mW scaling, default ranges when firmware has no limits, alert state and range fraction.
- `sensor_format.{h,cpp}`: value formatting shared by the LCD and tooltips.
- `sensor_identity.h`: widget keys and the chip fingerprint.

**`src/config/`**: runtime configuration. `runtime_config.{h,cpp}` defines `TemperatureUnit`, polling interval bounds (1-10 s, default 2 s) and fan RPM fallback bounds (500-9999, default 5000). `app_config_store.{h,cpp}` validates and persists them via QSettings. `settings_keys.h` holds all QSettings keys. `settings_schema.{h,cpp}` handles versioned migration (current: v2).

**`src/ui/`**: presentation only; business rules live in `src/sensors/`.
- `main_window`: polling, window sizing, status messages, settings load/save.
- `panels/sensors_panel`: chip-grouped layout; owns the chip expand state and the drag-and-drop chip order; separates structural rebuilds from value-only updates to avoid layout thrash.
- `panels/settings_panel`: polling interval, fan RPM fallback, temperature unit controls.
- `widgets/collapsible_section`: framed card with toggle header (optionally draggable), used by both panels.
- `widgets/status_line`: status bar text with timed notices on top of the permanent status.
- `widgets/sensor_value_widget`: per-sensor card (title label above the LCD; tooltip with chip and limits).
- `widgets/lcd_display_widget` + `lcd_segment_font`: vector segment LCD rendering (value, unit, range bar graph).
- `theme/app_theme.h`: sizing, spacing, LCD colors and style sheets. Change the look here, not in widget code.

**Build targets**: `qsensors_core` (static library with everything except `main.cpp`, `main_window` and the libsensors backend) is linked by the app and by every test. New sources go into `QSENSORS_CORE_SOURCES` or `QSENSORS_APP_SOURCES` in `CMakeLists.txt`; both lists are also scanned for translations.

**`tests/`**: 9 unit test files covering range policy and rules, LCD logic, segment glyph model, sensor contracts and formatting, settings persistence/migration, sensor identity, the sensors panel, the status line and runtime theme refresh. Treat failing tests as blockers.

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
- A release is cut by tagging a commit on `main` with `vX.Y.Z` (e.g. `v0.80.10`). `.github/workflows/release.yml` then verifies the tag is on `main`, matches `project(qsensors VERSION …)` in `CMakeLists.txt` and has a `## [X.Y.Z]` section in `CHANGELOG.md`, builds both AppImages via the same `appimage.yml` and creates a **draft** GitHub release.
- AppImages: `packaging/appimage/build.sh` does the whole build (system packages, Qt 6.8.3 via aqtinstall, build, tests, linuxdeploy) inside a container whose glibc is the minimum glibc: `manylinux_2_28` for x86_64 (2.28), `ubuntu:24.04` for arm64 (2.39, official Qt arm64 binaries need 2.38). Both run natively on GitHub runners (`ubuntu-24.04`, `ubuntu-24.04-arm`).
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
- **Structural vs value updates**: `SensorsPanel` intentionally separates layout rebuilds (structure changed) from in-place value patches (same sensors, new readings). Preserve this distinction when modifying the panel.
- **No silent settings migrations**: schema version bumps must be explicit and visible.
- **libsensors lifecycle**: `sensors_init` / `sensors_cleanup` must be paired; no leaks.
