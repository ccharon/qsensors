# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Changed
- Sensors are read in a background thread, so slow drivers (e.g. drive
  temperatures via `drivetemp`) no longer freeze the window. A read that takes
  longer than the polling interval delays the next one instead of piling up.
- Changing the temperature unit or the fan fallback maximum applies to the
  current readings right away, without reading the sensors again.
- Resizing the window only moves the sensor cards; they are no longer
  recreated when the number of columns changes. The layout itself is unchanged.

## [0.80.10] - 2026-09-27

### Added
- Unit symbols for Ampere (`A`, `mA`) and Watt (`W`, `mW`) on the LCD;
  previously Current/Power tiles rendered digits with no unit suffix at all,
  since the xsensors theme sprite never included them.
- Automatic mA/mW scaling for Current and Power readings below 1 unit.
  `libsensors` always reports base SI units (A, W), so sub-1 readings (e.g. a
  laptop's AC input current, often a few mA) used to floor to `0.00` under the
  fixed 2-decimal format. The scale is derived from the sensor's native
  min/max limits when available. Sensors without limits switch with wide
  hysteresis (to milli below 0.1, back to the base unit only from 10), so a
  reading hovering around 1 W does not flicker between `mW` and `W`.
  Milli-unit values are shown with one decimal so negative readings (e.g. a
  discharging battery at `-396.0 mA`) still fit the display.
- Native `power*_max`/`power*_min` limits (with `power*_cap`/`power*_crit`
  fallback) are now read for Power sensors (`power*_min` needs a libsensors
  that knows it, so not in the x86_64 AppImage built against lm-sensors 3.4); previously only Temperature,
  Voltage and Fan/Current categories read native firmware limits, and Power
  tiles always relied on the synthetic default-range policy.

- Sensor card tooltip with the full sensor name, chip and the min/max limits
  in display units (or a note when the sensor has no limits).

- `cmake --install` now installs the desktop entry and the app icon
  (256 px PNG and scalable SVG) to the XDG locations, so packages no longer
  need to copy them separately. The desktop entry gained a Portuguese comment.

- arm64 (aarch64) AppImage for releases, alongside the x86_64 one.
- Chip panels can be reordered by dragging their header; the order is saved
  and restored on the next start. New chips appear at the end.

### Changed
- Gentoo ebuild: fetches the `v`-prefixed release tag, relies on `cmake --install`
  for the desktop entry and icons (no `files/` directory anymore) and adds the
  `~arm64` keyword.
- The settings section moved from the top to the bottom of the window.
- Wider windows add sensor columns one at a time, first to the category with
  the most rows, instead of only when every category of a chip fits another
  column. Remaining width stretches the sensor cards (150 to 200 px), so rows
  fill the window instead of leaving an empty margin; all cards share one
  width, set by the chip with the least spare width.
- The window cannot be dragged taller than its content; it is never shrunk
  automatically when the content gets shorter. Maximized and full-screen
  windows fill the screen. Without a saved window size the first height fits
  the content.
- The vertical scrollbar only appears when the content does not fit; the
  cards narrow instead of being covered by it.
- AppImages bundle Qt 6.8 and the Wayland platform plugin, so they run
  natively on Wayland and still on X11. The x86_64 AppImage now needs glibc
  2.28 (was 2.35), which adds RHEL/Alma/Rocky 8 and 9, Debian 11 and Ubuntu
  20.04; the new arm64 AppImage needs glibc 2.39.
- The LCD value display is now drawn as a vector segment display instead of
  glyphs cut from the xsensors theme bitmap. Digits keep the slanted
  xsensors look on a recessed, slightly green-tinted backplane where lit
  segments cast a faint shadow (dark themes use brighter, softly glowing
  "backlit" segments instead); unlit segments are shown faintly like on a
  real LCD, units are rendered smaller on the digit baseline, and the display
  stays sharp on HiDPI screens. The display keeps a small inner margin and
  shrinks unusually long readings to fit instead of clipping them. New unit
  symbols no longer require editing a sprite sheet.
- Sensor cards are simplified: the name is a plain theme-styled label above
  the LCD (long names are elided, see tooltip) instead of a framed
  group box, and the min/max range is shown as a segment bar graph inside the
  LCD panel instead of a separate theme-colored progress bar.

- New application icon showing the segment LCD (digits, degree unit and bar
  graph), generated from the app's own segment font; available as SVG and
  256 px PNG (`resources/icons/qsensors.{svg,png}`).

### Fixed
- Status bar notices (e.g. "Sensor layout changed", "Settings could not be
  saved") are shown in bold on their own for 10 seconds; previously the
  reading count replaced them immediately or was drawn on top of them when the
  notice appeared during startup.
- Resizing the window no longer recreates every sensor card; sections are only
  rebuilt when their column count changes.
- The minimum window width is updated when a chip gains or loses a sensor
  category; previously only added or removed chips were taken into account.
- Invalid settings values fall back to the documented defaults (and are logged)
  instead of the minimum; out-of-range values are clamped with a log entry.
- Runtime settings are saved as soon as they change, and a failed write is
  shown in the status bar; previously they were only written on exit.
- Settings written by a newer qsensors version keep their schema version
  instead of being silently downgraded; a notice is shown.
- When libsensors reports no sensors at all, the status bar explains how to
  fix the configuration instead of showing an empty window.
- Unreadable sensors and chips with unusable names are logged once instead of
  being dropped silently; truncated chip names are detected.
- Negative current and power readings (e.g. a discharging laptop battery)
  are no longer shown as alerts when the firmware provides no minimum; the
  synthetic range now extends below zero for these signed sensor types.
- Switching the desktop between light and dark mode now restyles the whole
  window immediately; previously cards, headers and borders kept the old
  colors until qsensors was restarted, because style sheet `palette(...)`
  references were only resolved once at startup.

### Removed
- Bundled xsensors theme bitmap (`resources/themes/xsensors-theme.png`),
  no longer needed for LCD rendering.
- Bundled xsensors application icon and the accompanying third-party notice;
  qsensors no longer ships any xsensors assets.

## [0.80.9] - 2026-06-04

### Added
- Portuguese (`pt`) translation added.

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

[Unreleased]: https://github.com/ccharon/qsensors/compare/v0.80.10...develop
[0.80.10]: https://github.com/ccharon/qsensors/compare/0.80.9...v0.80.10
[0.80.9]: https://github.com/ccharon/qsensors/compare/0.80.8...0.80.9
[0.80.8]: https://github.com/ccharon/qsensors/compare/0.80.7...0.80.8
[0.80.7]: https://github.com/ccharon/qsensors/releases/tag/0.80.7
