# Demi

A configurable watchface for the **Pebble Time 2** (platform `emery`). Demi shows large
anti-aliased vector digits, a configurable progress bar, and three configurable widget
slots (date / weather / battery / heart rate) along the bottom.

![Demi — green, steps, 24h](demi.png)

| | | |
| --- | --- | --- |
| ![12h AM/PM](demi_ampm.png) | ![Cyan, battery](demi_battery.png) | ![Blue, distance](demi_distance.png) |
| 12h AM/PM · orange | Cyan · battery bar + glyph | Blue · distance (Run icon) |
| ![Purple, calories](demi_calories.png) | ![Yellow, accent hours](demi_accent_clock.png) | ![Magenta, three widgets](demi_widgets.png) |
| Purple · calories | Yellow · accent-color hours | Magenta · three widget slots (date / battery / weather) |

UUID: `f6cb4093-9dc1-4c3a-8316-d1d79e9e94d8`

## Design

- **Hours** on top in Rajdhani Bold (large, ~54% of the clock area), **minutes** below in
  Rajdhani Light (~49%).
- A **progress bar** between them (icon + track + value in the accent color).
- A **bottom row** of three configurable slots — left / middle / right — each showing one of:
  date, weather, battery, heart rate, or nothing.
- The layout is derived from the real PT2 screen size (no hardcoded dimensions), so it
  adapts under the Timeline Quick View peek.

## Configuration (Clay)

Open the watchface settings in the Pebble app to configure:

| Setting | Options |
| --- | --- |
| **Accent color** | 12-swatch palette: green, mint, cyan, blue, indigo, purple, magenta, pink, red, orange, yellow, white |
| **Hour/minute colors** | white–darkgrey, white–white, white–lightgrey (e-paper), lightgrey–white (e-paper), **accent–white, white–accent, accent–darkgrey, accent–lightgrey** (accent variants track the chosen accent color) |
| **24-hour clock** | on (24h) / off (12h with AM/PM label right of the hour) — default 24h |
| **Progress bar** | Steps / Battery / Calories / Distance |
| **Bottom widgets** | Three slots (left / middle / right), each: None / Date / Weather / Battery / Heart rate — default date / — / weather |
| **Battery percentage** | on / off — show the % beside the battery glyph, or glyph only — default on |
| **Language** (date) | Nederlands / English / Deutsch / Français |
| **Temperature unit** | Celsius / Fahrenheit |
| **Weather icon in accent color** | on / off (off = per-condition colors) — default off |

The **battery** widget is a graphical glyph filled proportionally to the charge level
(accent fill, red below 20%, lightning bolt while charging), optionally followed by the
percentage. The middle slot is skipped automatically if it would overlap a neighbour.

## Status icons

Two automatic status icons appear in the top corners (subtle light-gray outlines, no
configuration):

![Quiet-time + Bluetooth-disconnect icons](demi_status.png)

- **Quiet Time** (mouse, upper-left) when `quiet_time_is_active()`.
- **Bluetooth disconnected** (upper-right) when `connection_service_peek_pebble_app_connection()`
  is false.

Both are 25→22px PDCs from [pebble-dev/iconography](https://github.com/pebble-dev/iconography)
(`Quiet_time_mouse`, `Watch_disconnected`).

## Timeline Quick View

The whole face compresses upward to stay visible above the Timeline Quick View peek, and the
status icons hide during the slide. The heart-rate widget shows `--` when no sensor reading
is available (e.g. in the emulator).

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

## Building & installing

With the Pebble SDK on your `PATH`:

```bash
export PATH="$HOME/.local/bin:$PATH"
cd "$(git rev-parse --show-toplevel)"
pebble build
pebble install --emulator emery
```

To install on a real Pebble Time 2, use the **Pebble cloud install** flow (Dev Connect +
`pebble install --cloudpebble`).

For the asset toolchain, build caveats, rendering internals and project layout, see
**[docs/DEVELOPMENT.md](docs/DEVELOPMENT.md)**.

## Credits & licenses

Demi's own source is released under the **MIT License** (see `LICENSE`). Bundled
third-party assets keep their original licenses:

- **Rajdhani** font (`resources/fonts/Rajdhani-*.ttf` and the compiled `.ffont`) —
  © 2014 Indian Type Foundry, designed by Satya Rajpurohit & Jyotish Sonowal.
  Licensed under the **SIL Open Font License 1.1** — see
  [`resources/fonts/OFL.txt`](resources/fonts/OFL.txt).
- **Icons** (`resources/icons/*`) — derived from
  [pebble-dev/iconography](https://github.com/pebble-dev/iconography), licensed
  **Apache-2.0**. The distance icon is `Pebble_25x25_Run.svg`; the battery icon is a
  custom 25×25 SVG.
- **`tools/svg2pdc.py`** and **`tools/pebble_image_routines.py`** — © 2015 Pebble
  Technology, from the Pebble SDK examples (ported to Python 3).
- **Weather** data from [Open-Meteo](https://open-meteo.com/) (no API key required).
