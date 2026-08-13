# Demi — development notes

Build internals, asset toolchain and project layout. For features, configuration and
install instructions, see the main [README](../README.md).

## Building

Non-login shells need the SDK on `PATH` first:

```bash
export PATH="$HOME/.local/bin:$PATH"
cd "$(git rev-parse --show-toplevel)"
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

### Emulator freezes on WSL2

Under WSL2, `qemu-pebble`'s emulated timer interrupts stop firing after a minute or
two: the clock freezes, app timers die, and installs and screenshots time out, while
buttons and rendering still appear to work. Running qemu with `-icount` avoids it, but
pebble-tool has no hook for extra qemu arguments — only `PEBBLE_QEMU_PATH`, which points
at the binary. `tools/qemu-icount-wrapper.sh` stands in for that binary and adds the flag:

```bash
export PEBBLE_QEMU_PATH="$PWD/tools/qemu-icount-wrapper.sh"
pebble install --emulator emery
```

Export it in **every** shell that starts the emulator. The wrapper finds the SDK under
`~/.pebble-sdk` (override with `PEBBLE_SDK_HOME`). If the emulator will not even boot —
frozen progress bar, every app failing to install — `~/.pebble-sdk/<version>/emery/
qemu_spi_flash.bin` is corrupt; back it up (it holds every app's emulator data) and
delete it, and the next install recreates it.

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
  - All icons are recolored by `draw_pdc` in `demi.c` (outline/line-art: transparent fill,
    1px accent-colored stroke), used for both the progress-bar and bottom-row icons.
  - `svg2pdc.py` takes an optional `-S/--scale FACTOR` that uniformly scales coordinates
    **and** the declared image bounds — used to bring large sources down to the ~22px
    status-icon size (e.g. `--scale 0.28` for the 80×80 quiet-time mouse, `--scale 0.44`
    for the 50×50 watch-disconnected).
  - `svg2pdc.py` is ported to Python 3 and made resilient: it **rounds** off-grid points
    instead of failing, and **drops degenerate paths** (<2 points). The latter is critical —
    Adobe-exported SVGs can contain a lone `moveto` that yields a 0-point command, which
    makes the **entire** `.pdc` fail to load (`gdraw_command_image_create_with_resource`
    returns NULL → icon invisible).

The small font regex (`RAJDHANI_BOLD_20`, `[-0-9A-Za-z °%.k]`) **must** include a space and
`-`, otherwise `" "` / `"-"` render as missing-glyph boxes.

## Rendering internals

- Large digits are drawn with **FCTX vector fonts** (`pebble-fctx`); small text uses the
  raster TTF `RAJDHANI_BOLD_20`. Icons are **PDC vector** images, recolored at runtime.
- Layout is computed dynamically from `layer_get_unobstructed_bounds()` — **no hardcoded
  144×168** (that's basalt) — in an `apply_layout` helper. The face subscribes to the
  unobstructed-area service so the clock/progress/bottom row compress upward and stay
  visible above the ~51px Timeline Quick View peek; status icons hide during the slide.

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
