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

**`src/sensors/`** — libsensors integration layer. `sensors_backend.{h,cpp}` handles init/cleanup lifecycle, chip enumeration, and produces `SensorReading` structs. `sensors_policy.h` centralizes range normalization for all sensor categories (fills missing min/max with category defaults). Temperature unit conversion happens here, not in the UI.

**`src/config/`** — runtime configuration. `runtime_config.{h,cpp}` defines `TemperatureUnit`, polling interval bounds (1–10 s, default 2 s), and fan RPM fallback bounds (500–9999, default 5000). `app_config_store.{h,cpp}` reads/writes these via QSettings. `settings_schema.{h,cpp}` handles versioned migration (current: v2).

**`src/ui/`** — presentation only; no business logic.
- `main_window`: polling orchestration, window sizing, boots persistence.
- `panels/sensors_panel`: chip-grouped layout; separates structural rebuilds from value-only patches to avoid layout thrash.
- `panels/settings_panel`: polling interval, fan RPM fallback, temperature unit controls.
- `widgets/sensor_value_widget`: per-sensor card (title, LCD value, range bar).
- `widgets/lcd_display_widget` + `lcd_glyph_atlas`: atlas-based LCD glyph rendering.
- `theme/app_theme.h`: sizing/spacing/column constants — change layout here, not in widget code.

**`tests/`** — 5 unit test files covering range policy, LCD logic, glyph model, sensor contracts, and settings persistence/migration. Treat failing tests as blockers.

## Non-Goals

- CLI parsing as primary sensor source (`sensors` command output)
- QML
- mandatory X11-specific UI path
- hardware-in-the-loop test requirements in CI
- pixel/screenshot-based UI regression tests
- major architecture rewrites without prior alignment
- packaging work beyond currently requested targets
- silent settings-schema/key migrations

## Working Guidelines

- prefer small, isolated commits per change package
- align briefly before larger refactors or architecture changes
- run build + tests after functional changes; failing tests are blockers
- any behavior change should add or update automated tests when feasible
- explicit, visible error reporting — no silent failure
- robust behavior if sensors configuration is missing or invalid
- avoid overwriting unrelated in-progress worktree changes

## Key Policies

- **Translation**: any new or changed user-visible string must be reflected in all supported locales (`en`, `de`, `fr`, `es`) before the change is considered complete. CI enforces this via git diff on translation source files. Run `update_translations` target after string changes.
- **CHANGELOG.md**: update in the same commit for any user-visible, behavior-relevant, or release-noteworthy change (Keep a Changelog format).
- **Structural vs value updates**: `SensorsPanel` intentionally separates layout rebuilds (structure changed) from in-place value patches (same sensors, new readings). Preserve this distinction when modifying the panel.
- **No silent settings migrations**: schema version bumps must be explicit and visible.
- **libsensors lifecycle**: `sensors_init` / `sensors_cleanup` must be paired; no leaks.
