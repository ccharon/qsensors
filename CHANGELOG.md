# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Fan control outputs (`pwm*`) are shown in the Fans column as a duty cycle
  in percent. libsensors 3.6.2 reports them divided by 2 instead of 2.55;
  qsensors shows the duty cycle the kernel reports.
- Power sensors that only report an average (`power*_average`, e.g. the PPT
  of AMD GPUs) now get a card in the Power column.
- Energy meters (`energy*_input`, in J, kJ, MJ or GJ) in the Power column,
  humidity (`humidity*_input`, %RH) and frequency inputs (`freq*_input`, Hz,
  kHz or MHz) in the Other column. Energy counters and frequencies have no
  natural range and show no bar graph.

### Changed
- The status line says "Sensors unavailable, retrying: …" while the sensors
  cannot be read, instead of "libsensors init failed: …", which was also shown
  when a running source stopped working.
- The settings schema is now v3. The migration removes the unused
  `sensors/fingerprint` key (the saved chip set, which nothing reads any more)
  and is logged at startup. qsensors 0.80.11 and older read a v3 file without
  complaint but find no chip set there, so they skip their expand state check.
- Building needs CMake 3.21 or newer instead of 4.2. Qt 6.8 or newer, the
  version the AppImages are built with, is now checked at configure time.
- The app icon shows `°C` in the LCD's new unit lettering, matching the
  display.
- Values are shown with four significant digits, and the unit prefix follows the
  sensor's range: `880 mV`, `1.052 V`, `12.10 V`, `230.1 V`, `396 mA`,
  `1.234 kW`. Voltages below 1 V now use mV, power and current from 1000 use kW
  and kA, and the 1 mV resolution of voltage inputs is no longer cut to two
  decimals. Milliamperes and milliwatts are whole numbers. The range comes from
  the firmware limit when there is one; otherwise it is kept between polls so
  the unit does not flicker. Temperatures keep one decimal, fans none.
- The LCD is laid out like a multimeter: every value is right-aligned in the
  same five digit cells, with the unit in a fixed field at the right edge, so
  digits, decimal points and units line up across all cards. A wider card
  shows more unlit digits on the left instead of moving the value. Decimal
  points sit between the digit cells.
- Units are drawn as LCD symbols in a bold, narrow face instead of segment
  letters, with the same slant, colour and shadow or glow as the digits.
  "RPM" no longer reads like "APM", and `%`, `%RH`, `kHz` and `MHz` can be
  shown. The symbols are built into qsensors and need no installed font.
- An alert no longer depends on colour alone: a small `HI` or `LO` lights up
  under the unit when the value is above the firmware maximum or below the
  minimum. The bar graph ends before it.
- Changing the polling interval or fan fallback with the spin box arrows no
  longer rewrites the settings file on every step: the display follows each
  step, the file is written once the changes stop (and on close).
- Sensor labels and firmware limits are read once and then cached instead of
  on every poll, which cuts device I/O per poll (limits of NVMe drives and
  `drivetemp` disks are device commands). They are read again after a
  hardware change and once a minute, so a limit changed at runtime shows up
  within a minute; a limit that could not be read is retried on the next poll.

### Fixed
- A chip name containing `&` is shown as is in the chip card header; the
  character was dropped and the next letter underlined as a shortcut.
- A poll whose readings only change values no longer repaints the whole
  sensors panel: only the LCDs whose display changed are redrawn. The panel
  still batches its repaint when chips or sensors appear, disappear or move.
- A start with a different set of sensor chips no longer resets the expand
  state of all chips: only the entries of chips that are gone are dropped,
  and a read without any chip decides nothing. The status bar names the
  missing chips whose collapsed state was dropped, instead of reporting a
  reset of the UI configuration.
- A chip that appears or disappears while a chip card is being dragged no
  longer changes the cards during the drag: the change is applied when the
  drag ends, so the dragged card cannot be deleted under the pointer. Values
  keep updating during the drag.
- A sensor chip that is missing for a while (e.g. an unplugged USB device)
  keeps its place in the chip order instead of moving to the end, also when
  other chips are dragged; it returns where it was. The saved chip order
  keeps at most 16 missing chips, so it no longer grows with every chip ever
  seen.
- A chip card dragged from another qsensors window is no longer accepted as
  a drop, so it cannot reorder (and save) the chip order of this window.
- Sensor names and status text are always shown as plain text; a sensor label
  that looks like HTML is no longer rendered as rich text.
- The height of a sensor card follows a font change instead of staying at the
  value computed at creation.
- Status notices no longer replace each other: when several appear at once
  (for example at startup) they are shown one after another.
- Settings written by a newer qsensors version are no longer overwritten: this
  version reads them (clamping out-of-range values in memory only), does not
  write any setting, layout or window state back during the session, and says
  so in a status notice.
- A setting too large for a 32-bit integer is clamped to its range like any
  other out-of-range value instead of being replaced by the default.
- A garbled settings schema version is logged and treated as unversioned, and
  a failed write of the migrated schema version is logged and shown as a
  notice instead of passing silently.
- Sensor hardware that appears or disappears while qsensors runs (USB
  devices, NVMe drives, GPUs after a driver reload) is now picked up: when the
  hwmon devices change, libsensors is re-initialized, so new devices get cards
  and removed ones lose theirs. Before, a removed device kept a card of dashes,
  and a device that reused its hwmon index could show its values under the old
  chip's name. A failed re-initialization is shown in the status line.
- A failed libsensors initialization, at startup or after a hardware change,
  no longer needs a restart of qsensors. It is retried on the next change of
  the hwmon devices and on a timer, first after one polling interval, then
  with doubling delays up to once a minute. The status line shows the error
  until a retry succeeds, then the readings return with the notice "Sensors
  are available again". The log names the failure once, a different error
  again, and the recovery.
- Two chips with the same name (for example two ACPI fans, both
  `acpi_fan-acpi-0`) no longer share one set of cards, where one card froze
  and the other jumped between both sensors. The second and further chips of
  that name are shown as `acpi_fan-acpi-0 #2` and so on, numbered by their
  device path so the numbering stays the same across restarts. Chips with a
  unique name keep their name and their saved order and expand state.
- A driver that hangs at startup no longer keeps the window from appearing.
  If the first sensor read takes longer than 10 seconds, the window opens
  without readings and the status line says the sensors are not responding;
  the readings appear as soon as the read returns. On a first start without a
  saved window size, the window is then fitted to them as on a normal start,
  unless it was resized or maximized in the meantime.
- Closing qsensors while a sensor read hangs can no longer crash at exit: when
  the read has not returned within the shutdown timeout, the process now ends
  without tearing down Qt under the still running sensor thread.
- A value no longer takes more digit cells than the others: `-2000 mA` and
  values that round up to the next decade drop a decimal or switch to the next
  unit instead of shrinking the digits of that one card.
- A value just below zero is shown as `0.0`, not `-0.0`.
- A current or power sensor whose firmware limit could not be read for one poll
  no longer switches between A and mA (or W and mW) for that poll.

### Security
- The build enables hardening flags for the project's own targets
  (position independent executable, stack protector, stack clash protection,
  `-fcf-protection` on x86, `_FORTIFY_SOURCE` in optimized builds, full RELRO).
  The new CMake option `QSENSORS_HARDENING` (default `ON`) turns them off.
- The AppImage build installs its Python tools (cmake, ninja, aqtinstall) from a
  hash-locked requirements file, pins the build container images by digest and
  the GitHub Actions by commit SHA, and no longer persists checkout credentials.

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

[Unreleased]: https://github.com/ccharon/qsensors/compare/v0.80.11...develop
[0.80.11]: https://github.com/ccharon/qsensors/compare/v0.80.10...v0.80.11
[0.80.10]: https://github.com/ccharon/qsensors/compare/0.80.9...v0.80.10
[0.80.9]: https://github.com/ccharon/qsensors/compare/0.80.8...0.80.9
[0.80.8]: https://github.com/ccharon/qsensors/compare/0.80.7...0.80.8
[0.80.7]: https://github.com/ccharon/qsensors/releases/tag/0.80.7
