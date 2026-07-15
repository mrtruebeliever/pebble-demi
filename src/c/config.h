#pragma once
#include <pebble.h>

// Persist storage keys (independent of the AppMessage message keys).
#define PERSIST_ACCENT_COLOR   1
#define PERSIST_PROGRESS_TYPE  2
#define PERSIST_TEMP_UNIT      6
#define PERSIST_LANGUAGE       8
#define PERSIST_CLOCK_SCHEME   9
#define PERSIST_CLOCK_24H      10
#define PERSIST_WEATHER_ACCENT 11
#define PERSIST_WIDGET_LEFT    12
#define PERSIST_WIDGET_MID     13
#define PERSIST_WIDGET_RIGHT   14
#define PERSIST_BATTERY_PCT    15
#define PERSIST_LAYOUT_MODE    16
#define PERSIST_PROGRESS_INFO  17
#define PERSIST_WEATHER_TEMP   18
#define PERSIST_WEATHER_COND   19
#define PERSIST_WEATHER_TIME   20
#define PERSIST_PROGRESS_SWAP  21

// Layout modes: hours above minutes with a horizontal bar between them, or
// hours beside minutes split by a vertical bar.
#define LAYOUT_VERTICAL    0
#define LAYOUT_HORIZONTAL  1
#define LAYOUT_COUNT       2

// Bottom-bar widget types (one per slot: left / middle / right).
#define WIDGET_NONE     0
#define WIDGET_DATE     1
#define WIDGET_WEATHER  2
#define WIDGET_BATTERY  3
#define WIDGET_HEART    4
#define WIDGET_COUNT    5

// Progressbar types.
#define PROGRESS_STEPS     0
#define PROGRESS_BATTERY   1
#define PROGRESS_CALORIES  2
#define PROGRESS_DISTANCE  3
#define PROGRESS_COUNT     4

// Temperature units.
#define TEMP_CELSIUS     0
#define TEMP_FAHRENHEIT  1
#define TEMP_UNIT_COUNT  2

// Clock color schemes (hour color / minute color). The high-contrast variants
// (white/white, white/light) read best on the e-paper display.
#define CLOCK_SCHEME_WHITE_GRAY   0  // white hours, dark-gray minutes
#define CLOCK_SCHEME_WHITE_WHITE  1  // white hours, white minutes
#define CLOCK_SCHEME_WHITE_LIGHT  2  // white hours, light-gray minutes (e-paper)
#define CLOCK_SCHEME_LIGHT_WHITE  3  // light-gray hours, white minutes (e-paper)
#define CLOCK_SCHEME_ACCENT_WHITE 4  // accent hours, white minutes
#define CLOCK_SCHEME_WHITE_ACCENT 5  // white hours, accent minutes
#define CLOCK_SCHEME_ACCENT_GRAY  6  // accent hours, dark-gray minutes
#define CLOCK_SCHEME_ACCENT_LIGHT 7  // accent hours, light-gray minutes
#define CLOCK_SCHEME_COUNT 8

// Languages for the weekday/month abbreviations.
#define LANG_NL  0
#define LANG_EN  1
#define LANG_DE  2
#define LANG_FR  3
#define LANG_COUNT 4

// Weather conditions (icon selector), as sent by the JS WMO mapping.
#define WEATHER_SUN          0
#define WEATHER_PARTLY       1
#define WEATHER_CLOUD        2
#define WEATHER_LIGHT_RAIN   3
#define WEATHER_HEAVY_RAIN   4
#define WEATHER_LIGHT_SNOW   5
#define WEATHER_HEAVY_SNOW   6

// Sentinels meaning "no weather data received yet" (or the stored data expired).
// The widget draws nothing in that state rather than inventing a condition.
#define WEATHER_TEMP_NONE  INT32_MIN
#define WEATHER_COND_NONE  -1

// Stored weather older than this is discarded on load and treated as absent.
#define WEATHER_MAX_AGE_S  (3 * 60 * 60)

// Default values. The slot defaults reproduce the previous look:
// date left, weather right, middle empty (battery is opt-in via a slot).
#define DEFAULT_ACCENT_COLOR   0x00FF7F  // GColorMediumSpringGreen
#define DEFAULT_PROGRESS_TYPE  PROGRESS_STEPS
#define DEFAULT_TEMP_UNIT      TEMP_CELSIUS
#define DEFAULT_LANGUAGE       LANG_NL
#define DEFAULT_CLOCK_SCHEME   CLOCK_SCHEME_WHITE_GRAY
#define DEFAULT_CLOCK_24H      true   // NL convention; 12h + AM/PM is opt-in
#define DEFAULT_WEATHER_ACCENT false  // keep per-condition weather colors
#define DEFAULT_WIDGET_LEFT    WIDGET_DATE
#define DEFAULT_WIDGET_MID     WIDGET_NONE
#define DEFAULT_WIDGET_RIGHT   WIDGET_WEATHER
#define DEFAULT_BATTERY_PCT    true   // show the % beside the battery glyph
#define DEFAULT_LAYOUT_MODE    LAYOUT_VERTICAL
#define DEFAULT_PROGRESS_INFO  true   // show the icon + value flanking the bar
#define DEFAULT_PROGRESS_SWAP  false  // icon leads, value trails

// All user-configurable state plus the latest weather snapshot.
typedef struct {
  GColor accent_color;
  int    layout_mode;   // LAYOUT_VERTICAL / LAYOUT_HORIZONTAL
  int    progress_type;
  bool   progress_info; // show the progressbar icon and value label
  bool   progress_swap; // trade the icon and value places around the bar
  int    widget_left;   // WIDGET_* type shown in the left bottom slot
  int    widget_mid;    // WIDGET_* type shown in the middle bottom slot
  int    widget_right;  // WIDGET_* type shown in the right bottom slot
  bool   battery_pct;   // show the % label beside the battery widget glyph
  int    temp_unit;
  int    language;
  int    clock_scheme;
  bool   clock_24h;       // true = 24h display, false = 12h with AM/PM
  bool   weather_accent;  // true = draw weather icon in the accent color
  int    weather_temp;       // WEATHER_TEMP_NONE until first fetch
  int    weather_condition;  // WEATHER_SUN / WEATHER_CLOUD / WEATHER_RAIN
} DemiConfig;

// Returns a pointer to the singleton config (valid after config_load).
DemiConfig *config_get(void);

// Registers a callback fired whenever settings/weather change (redraw hook).
void config_set_change_callback(void (*cb)(void));

// Loads settings from persist storage, falling back to defaults.
void config_load(void);

// Writes the current user settings (not weather) to persist storage.
void config_save(void);

// AppMessage inbox handler: applies incoming settings + weather, then redraws.
void config_inbox_received(DictionaryIterator *iter, void *context);
