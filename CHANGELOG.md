# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

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

[Unreleased]: https://github.com/ccharon/qsensors/compare/0.80.8...develop
[0.80.8]: https://github.com/ccharon/qsensors/compare/0.80.7...0.80.8
[0.80.7]: https://github.com/ccharon/qsensors/releases/tag/0.80.7
