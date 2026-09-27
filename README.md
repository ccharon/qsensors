# qsensors

Qt6/Wayland-oriented sensor monitor with an xsensors-inspired UI and direct
`libsensors` access.

![qsensors](./docs/qsensors.png)

## Dependencies

- C++20 compiler
- CMake
- Qt6 (`Core`, `Gui`, `Widgets`)
- `libsensors` (lm-sensors)

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

## Run

```bash
./build/qsensors
```

## Releases

Development happens on `develop`; `main` only contains released states.
Releases are tagged `vX.Y.Z` on `main`, which builds both AppImages and creates
a GitHub release draft. Prebuilt AppImages are attached to the
[GitHub releases](https://github.com/ccharon/qsensors/releases). They bundle
Qt 6.8 with the Wayland and X11 platform plugins; Qt picks one at startup.

| AppImage | Architecture | Minimum glibc | Examples |
|---|---|---|---|
| `qsensors-X.Y.Z-x86_64.AppImage` | x86_64 | 2.28 | RHEL/Alma/Rocky 8+, Debian 11+, Ubuntu 20.04+ |
| `qsensors-X.Y.Z-aarch64.AppImage` | arm64 | 2.39 | Ubuntu 24.04+, Debian 13+, Fedora 40+ |

Build an AppImage locally inside the matching container, e.g. for arm64:
`docker run --rm -v "$PWD":/src -w /src -e VERSION=0.80.10 ubuntu:24.04 bash packaging/appimage/build.sh`

## Configuration

`QSettings` on Linux, typically:

`~/.config/qsensors/qsensors.conf`

Runtime keys currently used:
- `runtime/polling_interval_sec`
- `runtime/fan_default_max_rpm`
- `runtime/temperature_unit` (`C` or `F`, default `C`)

Temperature display is selectable in the settings panel (`Celsius` / `Fahrenheit`).

## Sensor range policy (normalization)

The backend returns normalized readings intended for direct rendering:
- native firmware limits are used when available
- missing limits are completed by qsensors category policy defaults
- for temperature readings, values and limits are returned in the requested
  display unit (`C` or `F`)

## License

This project is licensed under **GPL-2.0-or-later**.

- Full text: [`LICENSE`](./LICENSE)
