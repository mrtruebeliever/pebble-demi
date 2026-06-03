#pragma once
#include <pebble.h>

// Persist storage keys (independent of the AppMessage message keys).
#define PERSIST_ACCENT_COLOR   1
#define PERSIST_PROGRESS_TYPE  2
#define PERSIST_SHOW_DATE      3
#define PERSIST_SHOW_WEATHER   4
#define PERSIST_SHOW_BATTERY   5
#define PERSIST_TEMP_UNIT      6
#define PERSIST_SHOW_HEART     7
#define PERSIST_LANGUAGE       8
#define PERSIST_CLOCK_SCHEME   9
#define PERSIST_CLOCK_24H      10
#define PERSIST_WEATHER_ACCENT 11

// Progressbar types.
#define PROGRESS_STEPS     0
#define PROGRESS_BATTERY   1
#define PROGRESS_CALORIES  2
#define PROGRESS_DISTANCE  3

// Temperature units.
#define TEMP_CELSIUS     0
#define TEMP_FAHRENHEIT  1

// Clock color schemes (hour color / minute color). The high-contrast variants
// (white/white, white/light) read best on the e-paper display.
#define CLOCK_SCHEME_WHITE_GRAY   0  // white hours, dark-gray minutes
#define CLOCK_SCHEME_WHITE_WHITE  1  // white hours, white minutes
#define CLOCK_SCHEME_WHITE_LIGHT  2  // white hours, light-gray minutes (e-paper)
#define CLOCK_SCHEME_LIGHT_WHITE  3  // light-gray hours, white minutes (e-paper)
#define CLOCK_SCHEME_COUNT 4

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

// Sentinel meaning "no weather data received yet".
#define WEATHER_TEMP_NONE  INT32_MIN

// Default values.
#define DEFAULT_ACCENT_COLOR   0x00FF7F  // GColorMediumSpringGreen
#define DEFAULT_PROGRESS_TYPE  PROGRESS_STEPS
#define DEFAULT_SHOW_DATE      true
#define DEFAULT_SHOW_WEATHER   true
#define DEFAULT_SHOW_BATTERY   false
#define DEFAULT_TEMP_UNIT      TEMP_CELSIUS
#define DEFAULT_SHOW_HEART     false
#define DEFAULT_LANGUAGE       LANG_NL
#define DEFAULT_CLOCK_SCHEME   CLOCK_SCHEME_WHITE_GRAY
#define DEFAULT_CLOCK_24H      true   // NL convention; 12h + AM/PM is opt-in
#define DEFAULT_WEATHER_ACCENT false  // keep per-condition weather colors

// All user-configurable state plus the latest weather snapshot.
typedef struct {
  GColor accent_color;
  int    progress_type;
  bool   show_date;
  bool   show_weather;
  bool   show_battery;
  bool   show_heart;
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
