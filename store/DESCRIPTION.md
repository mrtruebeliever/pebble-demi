# Demi — appstore listing

**Tagline:** A vector watchface that tracks whatever matters to you.

**Short description (≤ 140 chars):**
Bold vector digits, a progress bar for steps/battery/calories/distance or your own JSON stat, plus weather, date and heart-rate widgets.

**Full description:**

Demi is a configurable watchface for the Pebble Time 2, built around large
anti-aliased vector digits and a progress bar that stays out of the way until
you need it. Choose steps, battery, calories or distance, or point it at your
own JSON feed to track anything else you can express as a percentage. Round
out the bottom row with date, weather, battery, or heart rate — pick any
three.

FEATURES
• Large FCTX vector digits (hours/minutes), scaled from the real screen size —
  no fixed layout, so it adapts under the Timeline Quick View peek.
• Three layouts: vertical (hours above minutes), horizontal (hours beside
  minutes, vertical bar), or horizontal with two independent bars, each
  tracking its own metric and optionally its own color.
• Progress bar sources: steps, battery, calories, distance, or a custom
  JSON url of your own — see below.
• Beside the bar: icon + value, icon only, or nothing (lengthens the track).
  Swap icon and value to reverse the fill direction.
• Three configurable bottom-row widgets (left/middle/right): date, weather,
  battery (graphical glyph + %), or heart rate.
• 12-color accent palette, 8 hour/minute color schemes including
  accent-tinted variants tuned for e-paper.
• Automatic status icons: Quiet Time and Bluetooth-disconnected indicators.
• Weather via Open-Meteo — no API key, no account. Persists across launches
  so returning to the face never flashes a placeholder.
• Nederlands / English / Deutsch / Français for the date.

CUSTOM JSON METRIC
Point "Custom 1" or "Custom 2" at a url returning
`{"items":[{"name":"...","value":0-100}]}` (item 0 → Custom 1, item 1 →
Custom 2) to track anything you can turn into a percentage — a self-hosted
stat, a script's output, whatever you track. The url stays on your phone and
is never sent to the watch; only the resulting percentage and a matching
icon (hourglass / burst / generic gauge, picked from the item's name) cross
Bluetooth. Refreshed every few minutes; oversized or malformed responses are
refused rather than parsed.

Works on Pebble Time 2 (Emery).

**Category:** Watchfaces
**Icons:** marketing/icon-80.png, marketing/icon-144.png
**Screenshots:** demi.png, demi_ampm.png, demi_battery.png, demi_calories.png,
demi_distance.png, demi_accent_clock.png, demi_widgets.png, demi_dual.png,
demi_dual_icons.png, demi_horizontal.png, demi_horizontal_minimal.png,
demi_dual_minimal.png, demi_minimal.png, demi_swap.png, demi_status.png
