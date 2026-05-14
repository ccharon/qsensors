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

## Configuration

`QSettings` on Linux, typically:

`~/.config/qsensors/qsensors.conf`

## License

This project is licensed under **GPL-2.0-or-later**.

- Full text: `LICENSE`
- Third-party notices: `THIRD_PARTY.md` and `third_party/xsensors/NOTICE.md`
