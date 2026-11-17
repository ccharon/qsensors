# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.81.0] - 2026-11-18

### Added
- New sensor types: fan control outputs (`pwm*`, duty cycle in percent) in the
  Fans column, average-only power sensors (`power*_average`, e.g. the PPT of
  AMD GPUs) and energy meters (`energy*_input`) in the Power column, humidity
  (`humidity*_input`) and frequency inputs (`freq*_input`) in the Other column.
  Energy and frequency have no bar graph.
- Hardware that appears or disappears while qsensors runs (USB devices, NVMe
  drives, GPUs after a driver reload) is picked up: libsensors is
  re-initialized when the hwmon devices change.
- A failed libsensors initialization, at startup or after a hardware change, is
  retried on the next hwmon change and on a timer (doubling delays up to once a
  minute) instead of needing a restart. The status line shows "Sensors
  unavailable, retrying: …" until it succeeds.

### Changed
- Values use four significant digits and a unit prefix that follows the
  sensor's range (`880 mV`, `1.052 V`, `12.10 V`, `1.234 kW`). The range comes
  from the firmware limit, or is kept between polls so the unit does not
  flicker.
- The LCD is laid out like a multimeter: values right-aligned in five digit
  cells, the unit in a fixed field at the right edge, so digits and units line
  up across all cards. Units are drawn as built-in LCD symbols ("RPM" no longer
  reads like "APM"), and an alert lights a `HI` or `LO` under the unit instead
  of relying on colour alone. The app icon uses the same lettering.
- Sensor labels and firmware limits are cached and re-read once a minute and
  after a hardware change, which cuts device I/O per poll (NVMe and `drivetemp`
  limits are device commands).
- The spin boxes write the settings file once the changes stop, not on every
  step.
- The settings schema is now v3; the logged migration removes the unused
  `sensors/fingerprint` key.
- Building needs CMake 3.21 or newer (was 4.2) and Qt 6.8 or newer, checked at
  configure time.

### Fixed
- Two chips with the same name (e.g. two `acpi_fan-acpi-0`) no longer share one
  set of cards; the second is shown as `… #2`, numbered stably by device path.
- A driver that hangs at startup no longer keeps the window from appearing
  (after 10 s it opens without readings and says so), and closing qsensors
  while a read hangs no longer crashes at exit.
- Settings written by a newer qsensors version are no longer overwritten: they
  are read, but nothing is written back during the session, and a notice says
  so. Values too large for 32 bits are clamped instead of reset, and a garbled
  schema version or a failed write is reported instead of passing silently.
- A changed set of chips at startup no longer resets the expand state of all
  chips; only missing chips are dropped, and the collapsed ones are named in
  the status bar.
- A temporarily missing chip keeps its place in the chip order (at most 16
  missing chips are kept).
- Dragging chip cards: chips appearing or disappearing during a drag are
  applied after it, and drops from another qsensors window are refused.
- A value update repaints only the LCDs whose display changed, not the whole
  panel.
- Values always fit the five digit cells, `-0.0` is shown as `0.0`, and a
  failed limit read no longer switches A/mA or W/mW for one poll.
- Chip names with `&` and sensor labels that look like HTML are shown as plain
  text; card heights follow font changes; status notices arriving together are
  queued instead of replacing each other.

### Security
- Hardened builds by default (PIE, stack protector, stack clash protection,
  `-fcf-protection` on x86, `_FORTIFY_SOURCE`, full RELRO); the CMake option
  `QSENSORS_HARDENING` turns them off.
- The AppImage build pins its inputs: Python tools by hash, container images by
  digest, GitHub Actions by commit SHA; checkout credentials are not persisted.

## [0.80.11] - 2026-11-15

### Added
- CMake options `QSENSORS_WARNINGS_AS_ERRORS` and `QSENSORS_SANITIZE` (both
  `OFF` by default) for developers: warnings as errors, and a build with
  AddressSanitizer and UndefinedBehaviorSanitizer.

### Changed
- Qt Test is only required when building the tests (`BUILD_TESTING=ON`); a
  build without tests no longer needs it.
- The window cannot be dragged wider than the full layout: once every
  category has all its columns and all cards have their maximum width, more
  width would only add empty space. Collapsed chips count, maximized and
  full-screen windows still fill the screen, and a wider window is never
  shrunk automatically. Without a saved window size the first width fits
  this limit when it is narrower than the default.
- Sensors are read in a background thread, so slow drivers (e.g. drive
  temperatures via `drivetemp`) no longer freeze the window. A read that takes
  longer than the polling interval delays the next one instead of piling up.
- Changing the temperature unit or the fan fallback maximum applies to the
  current readings right away, without reading the sensors again.
- Resizing the window only moves the sensor cards; they are no longer
  recreated when the number of columns changes. The layout itself is unchanged.
- Settings from an older qsensors are migrated once at startup and the
  migration is logged; reading the settings no longer writes to the file.

### Fixed
- The x86_64 AppImage did not show NVMe, ACPI and virtual chips (e.g. `nvme-pci-*`,
  `iwlwifi_*-virtual-*`): it bundled libsensors 3.4.0 from its build container.
  Both AppImages now bundle libsensors 3.6.2, built from source, so they find the
  same chips as a current system `sensors` and can read `power*_min` limits.
- The order of the chips and which chips are collapsed are saved as soon as
  you change them, not only when the window closes. They were lost when
  qsensors was killed, crashed or the session ended. A failed write is
  reported in the status bar. The window size is still saved on close.
- Sensors no longer show an alert because of a limit qsensors made up. When
  the firmware reports only a maximum (e.g. a GPU power cap or a USB-C port's
  maximum current), the bar graph assumes a minimum of 20 % of it; values
  below that, such as an idle GPU or an unused port, were shown in red. Alerts
  now come from firmware limits only. Temperatures without a firmware maximum
  still turn red above 100 °C.
- The sensor tooltip lists only the limits the firmware reports, or "No limits
  available"; previously it showed the assumed bar graph range as if it came
  from the firmware.
- When libsensors cannot be initialized, the error stays in the status bar;
  changing a setting used to replace it with "No sensors found".
- A sensor read that hangs (e.g. a blocked I2C bus) is shown in the status
  bar: after three polling intervals, at least 10 s, it says that the sensors
  are not responding and the values are outdated. The normal status returns
  as soon as a read completes. Previously the old values stayed on screen as
  if they were current.
- Closing the window no longer hangs when a sensor read does not return. qsensors
  waits up to 3 s for the read, logs a warning and exits.
- The libsensors initialization error in the status bar includes the reason
  from libsensors, not only the error number.
- Firmware limits that are placeholders are ignored, as if the firmware had
  reported none: no alert, the default bar graph range, not listed in the
  tooltip. This covers an unconfigured voltage input with min = max = 0 V,
  which showed every voltage in red, a maximum not above the minimum, and the
  -273.15 °C / 65261.85 °C limits of NVMe secondary sensors, which left the bar
  graph empty. Temperatures with a placeholder maximum still turn red above
  100 °C.
- A current or power sensor with a firmware limit of 0 no longer switches to
  mA or mW because of it; 12 A was shown as 12000.0 mA.
- A sensor that cannot be read for a moment keeps its card. The LCD shows
  dashes instead of the value, without bar graph or alert, and the tooltip
  says that the sensor cannot be read at the moment. Previously the card, or
  the whole chip when none of its sensors could be read, disappeared and the
  layout was rebuilt, again and again for sensors that fail now and then: a
  GPU in runtime suspend, a stopped fan, a drive in standby. A current or
  power sensor keeps its mA/mW unit while it cannot be read.
- Temperatures whose maximum is a placeholder (NVMe 65261.85 °C) use the
  critical limit as maximum when the firmware reports one.

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

[0.81.0]: https://github.com/ccharon/qsensors/compare/v0.80.11...v0.81.0
[0.80.11]: https://github.com/ccharon/qsensors/compare/v0.80.10...v0.80.11
[0.80.10]: https://github.com/ccharon/qsensors/compare/0.80.9...v0.80.10
[0.80.9]: https://github.com/ccharon/qsensors/compare/0.80.8...0.80.9
[0.80.8]: https://github.com/ccharon/qsensors/compare/0.80.7...0.80.8
[0.80.7]: https://github.com/ccharon/qsensors/releases/tag/0.80.7
