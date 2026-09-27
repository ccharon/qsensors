# App icon generator

`resources/icons/qsensors.svg` is generated from the app's own LCD segment font
(`src/ui/widgets/lcd_segment_font.*`), so the icon matches the in-app display.

```bash
cmake -S tools/icon -B build-icon
cmake --build build-icon
./build-icon/generate_icon resources/icons/qsensors.svg
rsvg-convert -w 256 -h 256 resources/icons/qsensors.svg -o resources/icons/qsensors.png
```
