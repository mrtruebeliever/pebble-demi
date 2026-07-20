# Changelog

## 1.5.0

- **Custom JSON progress bars.** Two new progress-bar sources, "Custom 1" and
  "Custom 2", driven by a url you configure in settings. The phone fetches
  `{"items":[{"name":"...","value":0-100}]}` (item 0 → Custom 1, item 1 →
  Custom 2) and forwards only the parsed percentage + an icon id to the watch
  — the url itself is never sent over Bluetooth. Use it to track any
  percentage-based stat you can expose as JSON.
- Icon per item is picked from its `name`: an hourglass for anything
  containing "session", a burst icon for anything containing "week",
  otherwise a generic gauge dial.
- Hardening: the fetch refuses responses over 8KB (before and during
  download) so a misbehaving endpoint can't stall the phone app, and the
  watch independently clamps incoming custom values to 0–100 rather than
  trusting the phone unconditionally.

## 1.4.1

- Store re-upload housekeeping: `tools/release.py` produces a minified,
  source-map-stripped `.pbw` for the appstore.

## 1.4.0

- **Icon-only bars** — the "beside the bar" setting gained an icon-only mode
  alongside nothing / icon + value.
- **Second bar color** — the two-bar layout's lower bar can take its own
  accent color instead of following the main one.
- Bigger digits in the two-bar layout.

## 1.3.0

- **Two-bar horizontal layout** — hours beside minutes, framed by a bar above
  and below, each tracking its own metric.
- Horizontal-layout digits are a fixed size and centered beside the bar,
  rather than rescaling with the column.

## 1.2.0

- The progress bar's fill direction now reverses with the icon/value swap
  (right-to-left / bottom-up), and the bare bar (nothing shown beside it)
  is shortened to match.

## 1.1.1

- Fix: AppMessage tuples are read by their actual type and width instead of
  assumed as `int32` — the phone sends values at their smallest width, so the
  old code could pick up garbage from neighbouring bytes.

## 1.1.0

- **Horizontal layout** — hours beside minutes, split by a vertical
  progress bar.
- Progress-bar info toggle (icon + value / icon only / nothing).
- Weather now persists across launches: the last reading shows immediately
  on relaunch instead of a placeholder while the phone re-fetches, discarded
  after 3 hours.

## 1.0.1

- 24-hour/12-hour toggle (with an AM/PM label) and a weather-icon-in-accent
  toggle.
- Automatic status icons: Quiet Time and Bluetooth-disconnected indicators.
- Timeline Quick View support — the face compresses upward to stay visible
  above the peek.
- Configurable 3-slot bottom widget bar (date / weather / battery / heart
  rate) with a graphical battery glyph and percentage toggle.
- 12-color accent palette, plus accent-colored clock schemes.
- Official PebbleOS icon for the distance metric; enlarged hour digits.
- Launcher menu icon.

## 1.0.0

- Initial release: large FCTX vector digits (hours/minutes), a configurable
  progress bar (steps / battery / calories / distance), Open-Meteo weather,
  Clay settings.
