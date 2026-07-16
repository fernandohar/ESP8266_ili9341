# Vendored / patched TFT_eSPI

This is a **project-local, patched copy** of TFT_eSPI (based on v2.4.2). It lives in
`lib/` so PlatformIO uses it instead of any globally-installed TFT_eSPI. All display
configuration is supplied via `-D` build flags in `platformio.ini`
(`USER_SETUP_LOADED`, `ILI9341_DRIVER`, pin defines, etc.), so `User_Setup.h` /
`User_Setups/` are not used by this project.

## Local modifications vs upstream

- **`TFT_eSPI.h`** — removed a stray `#define TOUCH_CS 1`. On this project's ESP32
  wiring that value hijacked GPIO1 (the UART TX pin) as the touch chip-select and
  broke serial output. `TOUCH_CS` is instead provided as a build flag
  (`-D TOUCH_CS=15`) in `platformio.ini`.

## Notes

- The upstream `examples/` folder was omitted to keep the repo small; it is not
  compiled. All files required to build (core, `TFT_Drivers/`, `Processors/`,
  `Fonts/`, `Extensions/`) are present.
- If you upgrade this library, re-apply the patch above (or re-verify the
  `TOUCH_CS` build flag still wins).
