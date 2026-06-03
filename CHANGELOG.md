# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [0.80.9] - 2026-06-04

### Fixed
- Chip names containing `/` are now percent-encoded before use as QSettings keys,
  preventing silent group-separator injection that could corrupt the persisted
  chip expand/collapse state.
- Integer overflow guard added in progress bar range scaling for extreme sensor
  values (scale factor clamped to `int` bounds before cast).

### Changed
- `SensorReading` min/max fields replaced with `std::optional<double>`; the
  separate `hasRange`/`hasMin`/`hasMax` bool flags are removed. `hasRange()`
  is now a const member function.
- Chip identity functions (`chipFingerprint`, `sensorKey`) extracted from
  `MainWindow` and `SensorsPanel` into a standalone `sensor_identity.h` header
  with no UI or libsensors dependencies, making them independently testable.
- Theme border-colour decision for sensor group boxes moved from widget code
  into `AppTheme::sensorGroupStyle(QPalette)`; widgets no longer compute
  palette brightness inline.
- `SettingsPanel` constructor split into three focused helper methods;
  polling interval bounds now reference `RuntimeConfigLimits` constants
  instead of being hardcoded.
- libsensors label buffer managed via RAII (`std::unique_ptr` with `std::free`)
  instead of manual `free(const_cast<char*>(...))`.
- `LcdGlyphAtlas::GlyphId::bySymbol` changed from O(n) linear scan to a
  lazily-initialised `QHash` lookup.
- Rendering optimisations: chip layout order rebuild, expand-state signals, and
  reading-group calculation are now skipped when the result would be unchanged.

### Added
- `qWarning` emitted when the LCD glyph atlas resource fails to load, replacing
  previous silent failure.
- `test_sensor_identity`: new test suite covering `chipFingerprint` (deduplication,
  order-independence) and `sensorKey` (uniqueness per chip, category, subfeature).
- `main_window_state_chip_name_with_slash_roundtrip`: new settings-persistence
  test verifying correct round-trip for chip names containing `/`.

## [0.80.8] - 2026-05-27

### Added
- Alert state rendering for Ampere and Watt sensors (same min/max logic as Volt).

### Fixed
- Non-finite values (NaN/Inf) returned by libsensors are now rejected at the read
  site and in the range policy, preventing undefined display behavior on corrupt
  hardware readings.

## [0.80.7] - 2026-05-18

### Added
- Temperature unit setting (`C` / `F`) with persistent `QSettings` storage.
- Fahrenheit rendering support in LCD/unit glyph flow.
- Settings schema versioning and migration plumbing (`meta/schema_version`, v2).
- New test coverage for sensor policy, LCD logic, sensor unit contracts, and
  settings persistence.
- CI check ensuring translation source files are up-to-date.

### Changed
- `SensorsBackend` now receives explicit read parameters for fan fallback max RPM
  and temperature unit instead of consuming `RuntimeConfig` directly.
- Sensor range fallback policy was centralized and expanded across categories
  (temperature, fan, voltage, current, power, other).
- `SensorValueWidget` now renders normalized ranges from backend output rather
  than deriving additional fallback heuristics locally.
- Runtime temperature unit persistence switched to human-readable tokens (`C`/`F`).
- Clarified documentation of runtime settings and backend normalization behavior.

### Fixed
- Completed translation entries for new temperature unit UI strings in `en/de/fr/es`.
- Gentoo packaging README now explicitly requires copying `files/qsensors.desktop`.

[Unreleased]: https://github.com/ccharon/qsensors/compare/0.80.9...develop
[0.80.9]: https://github.com/ccharon/qsensors/compare/0.80.8...0.80.9
[0.80.8]: https://github.com/ccharon/qsensors/compare/0.80.7...0.80.8
[0.80.7]: https://github.com/ccharon/qsensors/releases/tag/0.80.7
