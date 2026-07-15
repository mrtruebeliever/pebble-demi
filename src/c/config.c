#include "config.h"

static DemiConfig s_config;
static void (*s_change_cb)(void) = NULL;

// Returns a pointer to the singleton config.
DemiConfig *config_get(void) {
  return &s_config;
}

// Registers the redraw callback invoked after any config/weather change.
void config_set_change_callback(void (*cb)(void)) {
  s_change_cb = cb;
}

// Reads a tuple as an integer, honouring its actual type and width. The phone
// packs integers at their smallest width (a 1 arrives as a single byte) and
// Clay hands select values over as strings, so reading value->int32 blindly
// picks up neighbouring bytes and yields garbage.
static int32_t tuple_int(const Tuple *t) {
  switch (t->type) {
    case TUPLE_CSTRING:
      return (int32_t)atoi(t->value->cstring);
    case TUPLE_INT:
      if (t->length == 1) return t->value->int8;
      if (t->length == 2) return t->value->int16;
      return t->value->int32;
    case TUPLE_UINT:
      if (t->length == 1) return t->value->uint8;
      if (t->length == 2) return t->value->uint16;
      return (int32_t)t->value->uint32;
    default:
      return 0;
  }
}

// Applies an enum setting only when it falls inside [0, count). A malformed or
// unknown value leaves the previous setting alone rather than blanking the UI.
static void set_enum(int *dst, int32_t v, int count) {
  if (v >= 0 && v < count) {
    *dst = (int)v;
  } else {
    APP_LOG(APP_LOG_LEVEL_WARNING, "ignored out-of-range setting: %d", (int)v);
  }
}

// Loads settings from persist storage, using defaults for missing keys.
void config_load(void) {
  s_config.accent_color      = GColorFromHEX(DEFAULT_ACCENT_COLOR);
  s_config.layout_mode       = DEFAULT_LAYOUT_MODE;
  s_config.progress_type     = DEFAULT_PROGRESS_TYPE;
  s_config.progress_type_2   = DEFAULT_PROGRESS_TYPE_2;
  s_config.progress_info     = DEFAULT_PROGRESS_INFO;
  s_config.progress_swap     = DEFAULT_PROGRESS_SWAP;
  s_config.widget_left       = DEFAULT_WIDGET_LEFT;
  s_config.widget_mid        = DEFAULT_WIDGET_MID;
  s_config.widget_right      = DEFAULT_WIDGET_RIGHT;
  s_config.battery_pct       = DEFAULT_BATTERY_PCT;
  s_config.temp_unit         = DEFAULT_TEMP_UNIT;
  s_config.language          = DEFAULT_LANGUAGE;
  s_config.clock_scheme      = DEFAULT_CLOCK_SCHEME;
  s_config.clock_24h         = DEFAULT_CLOCK_24H;
  s_config.weather_accent    = DEFAULT_WEATHER_ACCENT;
  s_config.weather_temp      = WEATHER_TEMP_NONE;
  s_config.weather_condition = WEATHER_COND_NONE;

  // Restore the last weather snapshot unless it has gone stale. A negative age
  // means the clock moved backwards, which makes the timestamp untrustworthy.
  if (persist_exists(PERSIST_WEATHER_TIME) && persist_exists(PERSIST_WEATHER_TEMP)) {
    int age = (int)time(NULL) - persist_read_int(PERSIST_WEATHER_TIME);
    if (age >= 0 && age < WEATHER_MAX_AGE_S) {
      s_config.weather_temp      = persist_read_int(PERSIST_WEATHER_TEMP);
      s_config.weather_condition = persist_read_int(PERSIST_WEATHER_COND);
    }
  }

  if (persist_exists(PERSIST_ACCENT_COLOR)) {
    s_config.accent_color = (GColor){ .argb = (uint8_t)persist_read_int(PERSIST_ACCENT_COLOR) };
  }
  // Enum settings are range-checked on the way in too: an earlier build could
  // have persisted a garbage value, and that must not survive the upgrade.
  if (persist_exists(PERSIST_LAYOUT_MODE)) {
    set_enum(&s_config.layout_mode, persist_read_int(PERSIST_LAYOUT_MODE), LAYOUT_COUNT);
  }
  if (persist_exists(PERSIST_PROGRESS_TYPE)) {
    set_enum(&s_config.progress_type, persist_read_int(PERSIST_PROGRESS_TYPE), PROGRESS_COUNT);
  }
  if (persist_exists(PERSIST_PROGRESS_TYPE_2)) {
    set_enum(&s_config.progress_type_2, persist_read_int(PERSIST_PROGRESS_TYPE_2), PROGRESS_COUNT);
  }
  if (persist_exists(PERSIST_PROGRESS_INFO)) {
    s_config.progress_info = persist_read_bool(PERSIST_PROGRESS_INFO);
  }
  if (persist_exists(PERSIST_PROGRESS_SWAP)) {
    s_config.progress_swap = persist_read_bool(PERSIST_PROGRESS_SWAP);
  }
  if (persist_exists(PERSIST_WIDGET_LEFT)) {
    set_enum(&s_config.widget_left, persist_read_int(PERSIST_WIDGET_LEFT), WIDGET_COUNT);
  }
  if (persist_exists(PERSIST_WIDGET_MID)) {
    set_enum(&s_config.widget_mid, persist_read_int(PERSIST_WIDGET_MID), WIDGET_COUNT);
  }
  if (persist_exists(PERSIST_WIDGET_RIGHT)) {
    set_enum(&s_config.widget_right, persist_read_int(PERSIST_WIDGET_RIGHT), WIDGET_COUNT);
  }
  if (persist_exists(PERSIST_BATTERY_PCT)) {
    s_config.battery_pct = persist_read_bool(PERSIST_BATTERY_PCT);
  }
  if (persist_exists(PERSIST_TEMP_UNIT)) {
    set_enum(&s_config.temp_unit, persist_read_int(PERSIST_TEMP_UNIT), TEMP_UNIT_COUNT);
  }
  if (persist_exists(PERSIST_LANGUAGE)) {
    set_enum(&s_config.language, persist_read_int(PERSIST_LANGUAGE), LANG_COUNT);
  }
  if (persist_exists(PERSIST_CLOCK_SCHEME)) {
    set_enum(&s_config.clock_scheme, persist_read_int(PERSIST_CLOCK_SCHEME), CLOCK_SCHEME_COUNT);
  }
  if (persist_exists(PERSIST_CLOCK_24H)) {
    s_config.clock_24h = persist_read_bool(PERSIST_CLOCK_24H);
  }
  if (persist_exists(PERSIST_WEATHER_ACCENT)) {
    s_config.weather_accent = persist_read_bool(PERSIST_WEATHER_ACCENT);
  }
  APP_LOG(APP_LOG_LEVEL_DEBUG, "config loaded: accent=0x%02x progress=%d slots=%d/%d/%d unit=%d",
          s_config.accent_color.argb, s_config.progress_type, s_config.widget_left,
          s_config.widget_mid, s_config.widget_right, s_config.temp_unit);
}

// Persists the user-configurable settings. Weather has its own writer below so
// that a weather-only message never rewrites the whole settings block.
void config_save(void) {
  persist_write_int(PERSIST_ACCENT_COLOR, s_config.accent_color.argb);
  persist_write_int(PERSIST_LAYOUT_MODE, s_config.layout_mode);
  persist_write_int(PERSIST_PROGRESS_TYPE, s_config.progress_type);
  persist_write_int(PERSIST_PROGRESS_TYPE_2, s_config.progress_type_2);
  persist_write_bool(PERSIST_PROGRESS_INFO, s_config.progress_info);
  persist_write_bool(PERSIST_PROGRESS_SWAP, s_config.progress_swap);
  persist_write_int(PERSIST_WIDGET_LEFT, s_config.widget_left);
  persist_write_int(PERSIST_WIDGET_MID, s_config.widget_mid);
  persist_write_int(PERSIST_WIDGET_RIGHT, s_config.widget_right);
  persist_write_bool(PERSIST_BATTERY_PCT, s_config.battery_pct);
  persist_write_int(PERSIST_TEMP_UNIT, s_config.temp_unit);
  persist_write_int(PERSIST_LANGUAGE, s_config.language);
  persist_write_int(PERSIST_CLOCK_SCHEME, s_config.clock_scheme);
  persist_write_bool(PERSIST_CLOCK_24H, s_config.clock_24h);
  persist_write_bool(PERSIST_WEATHER_ACCENT, s_config.weather_accent);
}

// Stores the latest weather with a timestamp, so a relaunch shows the last
// known values instead of an empty slot while the phone re-fetches.
static void weather_save(void) {
  persist_write_int(PERSIST_WEATHER_TEMP, s_config.weather_temp);
  persist_write_int(PERSIST_WEATHER_COND, s_config.weather_condition);
  persist_write_int(PERSIST_WEATHER_TIME, (int)time(NULL));
}

// Applies any settings/weather present in an inbound AppMessage, then redraws.
void config_inbox_received(DictionaryIterator *iter, void *context) {
  bool settings_changed = false;
  bool weather_changed = false;
  Tuple *t;

  if ((t = dict_find(iter, MESSAGE_KEY_ACCENT_COLOR))) {
    s_config.accent_color = GColorFromHEX(tuple_int(t));
    settings_changed = true;
  }
  if ((t = dict_find(iter, MESSAGE_KEY_LAYOUT_MODE))) {
    set_enum(&s_config.layout_mode, tuple_int(t), LAYOUT_COUNT);
    settings_changed = true;
  }
  if ((t = dict_find(iter, MESSAGE_KEY_PROGRESS_TYPE))) {
    set_enum(&s_config.progress_type, tuple_int(t), PROGRESS_COUNT);
    settings_changed = true;
  }
  if ((t = dict_find(iter, MESSAGE_KEY_PROGRESS_TYPE_2))) {
    set_enum(&s_config.progress_type_2, tuple_int(t), PROGRESS_COUNT);
    settings_changed = true;
  }
  if ((t = dict_find(iter, MESSAGE_KEY_PROGRESS_INFO))) {
    s_config.progress_info = (tuple_int(t) != 0);
    settings_changed = true;
  }
  if ((t = dict_find(iter, MESSAGE_KEY_PROGRESS_SWAP))) {
    s_config.progress_swap = (tuple_int(t) != 0);
    settings_changed = true;
  }
  if ((t = dict_find(iter, MESSAGE_KEY_WIDGET_LEFT))) {
    set_enum(&s_config.widget_left, tuple_int(t), WIDGET_COUNT);
    settings_changed = true;
  }
  if ((t = dict_find(iter, MESSAGE_KEY_WIDGET_MID))) {
    set_enum(&s_config.widget_mid, tuple_int(t), WIDGET_COUNT);
    settings_changed = true;
  }
  if ((t = dict_find(iter, MESSAGE_KEY_WIDGET_RIGHT))) {
    set_enum(&s_config.widget_right, tuple_int(t), WIDGET_COUNT);
    settings_changed = true;
  }
  if ((t = dict_find(iter, MESSAGE_KEY_BATTERY_PCT))) {
    s_config.battery_pct = (tuple_int(t) != 0);
    settings_changed = true;
  }
  if ((t = dict_find(iter, MESSAGE_KEY_TEMP_UNIT))) {
    set_enum(&s_config.temp_unit, tuple_int(t), TEMP_UNIT_COUNT);
    settings_changed = true;
  }
  if ((t = dict_find(iter, MESSAGE_KEY_LANGUAGE))) {
    set_enum(&s_config.language, tuple_int(t), LANG_COUNT);
    settings_changed = true;
  }
  if ((t = dict_find(iter, MESSAGE_KEY_CLOCK_SCHEME))) {
    set_enum(&s_config.clock_scheme, tuple_int(t), CLOCK_SCHEME_COUNT);
    settings_changed = true;
  }
  if ((t = dict_find(iter, MESSAGE_KEY_CLOCK_24H))) {
    s_config.clock_24h = (tuple_int(t) != 0);
    settings_changed = true;
  }
  if ((t = dict_find(iter, MESSAGE_KEY_WEATHER_ACCENT))) {
    s_config.weather_accent = (tuple_int(t) != 0);
    settings_changed = true;
  }

  // Weather updates arrive on the same inbox, under their own persist keys.
  if ((t = dict_find(iter, MESSAGE_KEY_WEATHER_TEMP))) {
    s_config.weather_temp = tuple_int(t);
    weather_changed = true;
    APP_LOG(APP_LOG_LEVEL_DEBUG, "weather temp received: %d", s_config.weather_temp);
  }
  if ((t = dict_find(iter, MESSAGE_KEY_WEATHER_COND))) {
    s_config.weather_condition = tuple_int(t);
    weather_changed = true;
  }

  if (settings_changed) {
    config_save();
  }
  if (weather_changed) {
    weather_save();
  }
  if (s_change_cb) {
    s_change_cb();
  }
}
