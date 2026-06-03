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

// Loads settings from persist storage, using defaults for missing keys.
void config_load(void) {
  s_config.accent_color      = GColorFromHEX(DEFAULT_ACCENT_COLOR);
  s_config.progress_type     = DEFAULT_PROGRESS_TYPE;
  s_config.widget_left       = DEFAULT_WIDGET_LEFT;
  s_config.widget_mid        = DEFAULT_WIDGET_MID;
  s_config.widget_right      = DEFAULT_WIDGET_RIGHT;
  s_config.temp_unit         = DEFAULT_TEMP_UNIT;
  s_config.language          = DEFAULT_LANGUAGE;
  s_config.clock_scheme      = DEFAULT_CLOCK_SCHEME;
  s_config.clock_24h         = DEFAULT_CLOCK_24H;
  s_config.weather_accent    = DEFAULT_WEATHER_ACCENT;
  s_config.weather_temp      = WEATHER_TEMP_NONE;
  s_config.weather_condition = WEATHER_SUN;

  if (persist_exists(PERSIST_ACCENT_COLOR)) {
    s_config.accent_color = (GColor){ .argb = (uint8_t)persist_read_int(PERSIST_ACCENT_COLOR) };
  }
  if (persist_exists(PERSIST_PROGRESS_TYPE)) {
    s_config.progress_type = persist_read_int(PERSIST_PROGRESS_TYPE);
  }
  if (persist_exists(PERSIST_WIDGET_LEFT)) {
    s_config.widget_left = persist_read_int(PERSIST_WIDGET_LEFT);
  }
  if (persist_exists(PERSIST_WIDGET_MID)) {
    s_config.widget_mid = persist_read_int(PERSIST_WIDGET_MID);
  }
  if (persist_exists(PERSIST_WIDGET_RIGHT)) {
    s_config.widget_right = persist_read_int(PERSIST_WIDGET_RIGHT);
  }
  if (persist_exists(PERSIST_TEMP_UNIT)) {
    s_config.temp_unit = persist_read_int(PERSIST_TEMP_UNIT);
  }
  if (persist_exists(PERSIST_LANGUAGE)) {
    s_config.language = persist_read_int(PERSIST_LANGUAGE);
  }
  if (persist_exists(PERSIST_CLOCK_SCHEME)) {
    s_config.clock_scheme = persist_read_int(PERSIST_CLOCK_SCHEME);
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

// Persists the user-configurable settings (weather is transient, not stored).
void config_save(void) {
  persist_write_int(PERSIST_ACCENT_COLOR, s_config.accent_color.argb);
  persist_write_int(PERSIST_PROGRESS_TYPE, s_config.progress_type);
  persist_write_int(PERSIST_WIDGET_LEFT, s_config.widget_left);
  persist_write_int(PERSIST_WIDGET_MID, s_config.widget_mid);
  persist_write_int(PERSIST_WIDGET_RIGHT, s_config.widget_right);
  persist_write_int(PERSIST_TEMP_UNIT, s_config.temp_unit);
  persist_write_int(PERSIST_LANGUAGE, s_config.language);
  persist_write_int(PERSIST_CLOCK_SCHEME, s_config.clock_scheme);
  persist_write_bool(PERSIST_CLOCK_24H, s_config.clock_24h);
  persist_write_bool(PERSIST_WEATHER_ACCENT, s_config.weather_accent);
}

// Applies any settings/weather present in an inbound AppMessage, then redraws.
void config_inbox_received(DictionaryIterator *iter, void *context) {
  bool settings_changed = false;
  Tuple *t;

  if ((t = dict_find(iter, MESSAGE_KEY_ACCENT_COLOR))) {
    s_config.accent_color = GColorFromHEX(t->value->int32);
    settings_changed = true;
  }
  if ((t = dict_find(iter, MESSAGE_KEY_PROGRESS_TYPE))) {
    s_config.progress_type = t->value->int32;
    settings_changed = true;
  }
  if ((t = dict_find(iter, MESSAGE_KEY_WIDGET_LEFT))) {
    s_config.widget_left = t->value->int32;
    settings_changed = true;
  }
  if ((t = dict_find(iter, MESSAGE_KEY_WIDGET_MID))) {
    s_config.widget_mid = t->value->int32;
    settings_changed = true;
  }
  if ((t = dict_find(iter, MESSAGE_KEY_WIDGET_RIGHT))) {
    s_config.widget_right = t->value->int32;
    settings_changed = true;
  }
  if ((t = dict_find(iter, MESSAGE_KEY_TEMP_UNIT))) {
    s_config.temp_unit = t->value->int32;
    settings_changed = true;
  }
  if ((t = dict_find(iter, MESSAGE_KEY_LANGUAGE))) {
    s_config.language = t->value->int32;
    settings_changed = true;
  }
  if ((t = dict_find(iter, MESSAGE_KEY_CLOCK_SCHEME))) {
    s_config.clock_scheme = t->value->int32;
    settings_changed = true;
  }
  if ((t = dict_find(iter, MESSAGE_KEY_CLOCK_24H))) {
    s_config.clock_24h = (t->value->int32 != 0);
    settings_changed = true;
  }
  if ((t = dict_find(iter, MESSAGE_KEY_WEATHER_ACCENT))) {
    s_config.weather_accent = (t->value->int32 != 0);
    settings_changed = true;
  }

  // Weather updates arrive on the same inbox but are not persisted.
  if ((t = dict_find(iter, MESSAGE_KEY_WEATHER_TEMP))) {
    s_config.weather_temp = t->value->int32;
    APP_LOG(APP_LOG_LEVEL_DEBUG, "weather temp received: %d", s_config.weather_temp);
  }
  if ((t = dict_find(iter, MESSAGE_KEY_WEATHER_COND))) {
    s_config.weather_condition = t->value->int32;
  }

  if (settings_changed) {
    config_save();
  }
  if (s_change_cb) {
    s_change_cb();
  }
}
