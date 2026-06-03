# Demi

A configurable watchface for the **Pebble Time 2** (platform `emery`). Demi shows large
anti-aliased vector digits, a configurable progress bar, and a row of toggleable widgets
(date / weather / battery / heart rate) along the bottom.

![Demi](demi.png)
![Demi — English](demi_en.png)
![Demi — all widgets](demi_all.png)

UUID: `f6cb4093-9dc1-4c3a-8316-d1d79e9e94d8`

## Design

- **Hours** on top in Rajdhani Bold ~70, **minutes** below in Rajdhani Light ~70.
- A **progress bar** between them (icon + track + value in the accent color).
- A **bottom row** of complications: date, weather, battery, heart rate.
- Layout is computed dynamically from `layer_get_unobstructed_bounds()` — **no hardcoded
  144×168** (that's basalt). Proportions are derived from the real PT2 screen so the face
  stays correct under the Timeline quick-view peek.

Large digits are drawn with **FCTX vector fonts** (`pebble-fctx`), small text with the
raster TTF `RAJDHANI_BOLD_20`. Icons are **PDC vector** images, recolored at runtime.

## Configuration (Clay)

Open the watchface settings in the Pebble app to configure:

| Setting | Options |
| --- | --- |
| **Accent color** | green / orange / red / blue / purple |
| **Hour/minute colors** | white–darkgrey, white–white, white–lightgrey (e-paper), lightgrey–white (e-paper) |
| **Progress bar** | Steps / Battery / Calories / Distance |
| **Bottom widgets** | Date, Weather, Battery, Heart rate (each toggleable) |
| **Language** (date) | Nederlands / English / Deutsch / Français |
| **Temperature unit** | Celsius / Fahrenheit |

The heart-rate widget reads `HealthMetricHeartRateBPM` via `health_service_peek_current_value`
and shows `--` when no sensor is available (e.g. in the emulator).

## Weather

Weather is fetched from **[Open-Meteo](https://open-meteo.com/)** (no API key required) in
`src/pkjs/index.js`. WMO weather codes are mapped to 7 conditions by `condFromWMO`:

| # | Condition | Color |
| --- | --- | --- |
| 0 | Sunny | ChromeYellow |
| 1 | Partly cloudy | PictonBlue |
| 2 | Cloudy | PictonBlue |
| 3 | Light rain | PictonBlue |
| 4 | Heavy rain | PictonBlue |
| 5 | Light snow | Celeste |
| 6 | Heavy snow | Celeste |

## Building & running

Non-login shells need the SDK on `PATH` first:

```bash
export PATH="$HOME/.local/bin:$PATH"
cd /home/jscheyving/Development/pebble/Demi
pebble build
pebble install --emulator emery
pebble screenshot --emulator emery demi.png
```

> **Always `pebble clean` before `pebble build` after changing any resource.** A plain
> incremental build (~0.1s) does **not** re-pack changed resources that keep the same
> filename — font subsets and `.pdc`/`.ffont`/raw files stay cached. A clean build (~1.3s)
> fixes "icon swap / font regex seems ignored / icon invisible" symptoms.

Other notes:

- After changing `messageKeys` in `package.json`, run `pebble clean` so the generated
  `message_keys.auto.h` is regenerated.
- `pebble`/`uv` print harmless Python 3.13 `SyntaxWarning`s from libpebble2 — filter with
  `grep -viE 'SyntaxWarning|escape sequence|:param'`.
- To stop the emulator use `pebble kill` or `pkill -x qemu-pebble` — **not**
  `pkill -f qemu-pebble` (it matches your own shell command → self-kill, exit 144).
- On an emulator bootloop/hang: `pebble wipe` (do **not** `kill -9` qemu — that corrupts
  state into a bootloop).

To install on a real Pebble Time 2, see the **Pebble cloud install** flow (Dev Connect +
`pebble install --cloudpebble`).

## Asset toolchain (`tools/`)

Vector fonts and icons are precompiled into `resources/`:

- **`.ffont`** — `tools/ttf2svgfont.py` (`uv run --with fonttools`) builds an SVG font of
  digits 0–9 from a TTF, then `node_modules/.bin/fctx-compiler <svg> -r '[0-9]'` produces
  the `.ffont`.
- **`.pdc`** — icons from [pebble-dev/iconography](https://github.com/pebble-dev/iconography)
  (official PebbleOS SVGs, Apache 2.0, 25×25, white-fill + black-stroke) → `tools/svg2pdc.py`
  → `.pdc` (run via `uv run --with svg.path`). The battery icon is a custom 25×25 SVG.
  - Two recolor modes in `demi.c`: `draw_pdc_solid` (filled silhouette) for the small
    progress-bar icons, and `draw_pdc` (outline/line-art) for the bottom-row icons.
  - `svg2pdc.py` is ported to Python 3 and made resilient: it **rounds** off-grid points
    instead of failing, and **drops degenerate paths** (<2 points). The latter is critical —
    Adobe-exported SVGs can contain a lone `moveto` that yields a 0-point command, which
    makes the **entire** `.pdc` fail to load (`gdraw_command_image_create_with_resource`
    returns NULL → icon invisible).

The small font regex (`RAJDHANI_BOLD_20`, `[-0-9A-Za-z °%.k]`) **must** include a space and
`-`, otherwise `" "` / `"-"` render as missing-glyph boxes.

## Project layout

```
package.json          # app config, messageKeys, resources (Pebble "pebble" block)
src/c/demi.c          # watchface: layout, FCTX digits, PDC recolor, widgets
src/c/config.h/.c     # Clay config keys + AppMessage handling
src/pkjs/index.js     # phone JS: Open-Meteo fetch, WMO mapping, AppMessage
src/pkjs/config.json  # Clay configuration UI
resources/fonts/      # Rajdhani TTF + compiled .ffont
resources/icons/      # source .svg + compiled .pdc
tools/                # ttf2svgfont.py, svg2pdc.py, pebble_image_routines.py
```

## Dependencies

- `pebble-clay` — configuration UI
- `pebble-fctx` — anti-aliased vector text
- `pebble-fctx-compiler` (dev) — builds `.ffont` files
