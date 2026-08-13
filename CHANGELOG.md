# Changelog

## 1.7.0

- **Light theme.** A background setting flips the whole face to black on white.
  Every neutral is now drawn through one of four named roles — background,
  strong foreground, quiet foreground, track — rather than a literal colour, so
  each complication states what it means rather than what colour it happens to
  be, and none of them can quietly turn unreadable in one theme.
- **A light accent is darkened automatically on white, but only when it needs to
  be.** A white accent would otherwise vanish outright and yellow would wash
  out. Lightness is weighted towards green, which is why yellow and cyan get
  darkened while red and blue pass through untouched. Dark accents look the same
  in both themes, and nothing about your stored colour changes — so switching
  back to the dark theme restores exactly what you had.
- Weather icons take deeper hues on the light theme: chrome yellow and celeste
  were chosen against black and are near-invisible on white.
- The eight clock schemes are renamed from colours to contrast — "Strong /
  soft" instead of "White / light grey" — because a colour name is wrong half
  the time once there are two themes. The stored values are unchanged, so your
  current choice carries over.

## 1.6.0

- **Sunrise and sunset.** A new bottom-row widget shows whichever comes next —
  the sunrise time before dawn, the sunset time after it — using PebbleOS's own
  sunrise and sunset icons, a half sun over a horizon with the arrow built in.
  They read at a glance against the weather slot's bare sun. A matching **Daylight** progress bar tracks how much of the day
  between them has gone, labelled with the time still left (`3h15`). Both come
  from the same Open-Meteo request the weather already used: no second fetch,
  no API key. Sunrise and sunset are kept for a full day rather than the
  weather's three hours, since this morning's sunrise is still correct tonight.
- **Your own goals.** The step, calorie and distance bars no longer reach 100%
  at a fixed 10000 / 600 / 5000 — each has its own setting. Enter `0` and the
  watch uses your own daily average from its health history instead, falling
  back to the old fixed target when there is no history to average yet.
- **Usual-pace mark** (off by default) puts a mark on the bar where you
  normally stand at this time of day, so the fill can be read against it.
- The **day-elapsed bar carries sunrise and sunset marks**, so you can see at a
  glance where you stand within the daylight rather than just within the clock.
  Not on the daylight bar: there they would be its two ends by definition. Marks
  are drawn black over the accent fill and light grey over the bare track, so
  they stay legible wherever they land.
- **Metric or imperial.** Distance follows the watch's own measurement system
  by default, and can be pinned to kilometres or miles. The settings page shows
  the goal field in the matching unit.
- **Elapsed-time bars.** Four new progress-bar sources — day, week, month and
  year — computed on the watch, so they need neither the phone nor a sensor.
  The day bar carries a clock glyph; week, month and year each draw the same
  calendar box the date widget uses, with the period's initial inside it —
  `W`/`M`/`J` in Dutch, `W`/`M`/`Y` in English, `S`/`M`/`A` in French and
  Spanish. PebbleOS has no week/month/year icons, and one shared calendar icon
  could not tell the three apart.
- **Tap to reveal** (off by default): a tap on the wrist replaces the widget
  row for five seconds with the seconds, or with the full date
  (`TH 13 AUG 2026`) in your chosen language. A second, independent option lets
  the same tap expand the progress bars to icon + value — so a face kept
  deliberately bare still gives up its numbers on demand. Either option on its
  own is enough for a tap to do something; with both on, a tap reveals both.
- The settings page is now translated into **French, German and Spanish** as
  well as English, matching the five languages the watch face already spoke.
  Its explanatory paragraphs were previously left in Dutch on every non-Dutch
  page; they are now translated too. The language picker itself no longer
  carries a bilingual "Taal datum / Date language" label — that was there to
  stay findable back when the page was Dutch-only, and now it simply follows
  the page's language like everything else.
- The custom-JSON help text moved from a full-width paragraph to the url
  field's own description, so it renders small and grey like the other hints
  rather than dominating the section.
- Carried over from before the 1.5.0 release and never given a version of its
  own: Spanish date abbreviations, a "follow the watch's language" option, the
  translated settings page, a more legible default clock scheme (white hours
  over light-grey minutes rather than dark-grey), and two settings-page
  defaults that disagreed with the watch.
- The AppMessage inbox grew from 256 to 512 bytes. The settings dict was
  already close to the old limit, and the new options would have pushed a full
  save past it — which is dropped silently apart from a log line.

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
