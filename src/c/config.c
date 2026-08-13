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
// Maps the watch's system locale (e.g. "en_US", "nl_NL") to a UI language.
// Anything this face does not carry falls back to English.
static int detect_locale_lang(void) {
  const char *loc = i18n_get_system_locale();
  if (loc && loc[0] && loc[1]) {
    if (loc[0] == 'n' && loc[1] == 'l') return LANG_NL;
    if (loc[0] == 'f' && loc[1] == 'r') return LANG_FR;
    if (loc[0] == 'd' && loc[1] == 'e') return LANG_DE;
    if (loc[0] == 'e' && loc[1] == 's') return LANG_ES;
  }
  return LANG_EN;
}

// The user's raw choice, which may be LANG_AUTO. s_config.language always holds
// a concrete index for drawing, so the preference has to be kept separately --
// otherwise the first config_save() would write the resolved language back and
// silently pin a watch that was set to follow its locale.
static int s_lang_pref = LANG_AUTO;

// LANG_AUTO means "follow the watch"; anything else is the user's own pick.
static void set_lang(int *dst, int32_t v) {
  s_lang_pref = (int)v;
  *dst = (v >= 0 && v < LANG_COUNT) ? (int)v : detect_locale_lang();
}

// The watch carries its own metric/imperial preference; asking the user again
// in Clay would be a second source of truth for something it already knows.
// MeasurementSystemUnknown (no health data, or a metric this watch cannot
// express) falls back to metric.
static int detect_measurement_system(void) {
  MeasurementSystem sys =
      health_service_get_measurement_system_for_display(HealthMetricWalkedDistanceMeters);
  return (sys == MeasurementSystemImperial) ? DIST_MILES : DIST_KM;
}

// The user's raw choice, which may be DIST_AUTO. Kept apart from the resolved
// value for the same reason as s_lang_pref above: saving the resolved unit
// would silently pin a watch that was set to follow its own setting.
static int s_dist_pref = DIST_AUTO;

// DIST_AUTO means "follow the watch"; anything else is the user's own pick.
static void set_dist_unit(int *dst, int32_t v) {
  s_dist_pref = (int)v;
  *dst = (v >= 0 && v < DIST_UNIT_COUNT) ? (int)v : detect_measurement_system();
}

// Applies a goal, which is a positive target or GOAL_AVERAGE (0) meaning "use
// my own average". A negative value is meaningless and leaves the goal alone.
static void set_goal(int *dst, int32_t v) {
  if (v >= 0) {
    *dst = (int)v;
  } else {
    APP_LOG(APP_LOG_LEVEL_WARNING, "ignored negative goal: %d", (int)v);
  }
}

static void set_enum(int *dst, int32_t v, int count) {
  if (v >= 0 && v < count) {
    *dst = (int)v;
  } else {
    APP_LOG(APP_LOG_LEVEL_WARNING, "ignored out-of-range setting: %d", (int)v);
  }
}

// Clamps an incoming custom-metric value to a valid percentage. Defense in
// depth for a value sourced from an arbitrary user-configured url: the phone
// already clamps before sending, but this feeds directly into bar-width
// arithmetic (track_w * pct / 100), so the watch shouldn't trust that
// unconditionally.
static int32_t clamp_pct(int32_t v) {
  if (v < 0) return 0;
  if (v > 100) return 100;
  return v;
}

// Validates a sunrise/sunset as minutes since local midnight. Anything outside
// a day means the phone sent something nonsensical, and is stored as "unknown"
// so the daylight bar draws nothing instead of a bogus fill.
static int32_t sun_minutes_or_none(int32_t v) {
  if (v < 0 || v >= 24 * 60) return SUN_TIME_NONE;
  return v;
}

// Loads settings from persist storage, using defaults for missing keys.
void config_load(void) {
  s_config.accent_color      = GColorFromHEX(DEFAULT_ACCENT_COLOR);
  s_config.accent_color_2    = GColorFromHEX(DEFAULT_ACCENT_COLOR_2);
  s_config.accent_2_enable   = DEFAULT_ACCENT_2_ENABLE;
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
  s_config.language          = LANG_EN;   // resolved by set_lang() below
  s_config.clock_scheme      = DEFAULT_CLOCK_SCHEME;
  s_config.clock_24h         = DEFAULT_CLOCK_24H;
  s_config.weather_accent    = DEFAULT_WEATHER_ACCENT;
  s_config.goal_steps        = DEFAULT_GOAL_STEPS;
  s_config.goal_kcal         = DEFAULT_GOAL_KCAL;
  s_config.goal_dist_m       = DEFAULT_GOAL_DIST;
  s_config.dist_unit         = DIST_KM;   // resolved by set_dist_unit() below
  s_config.pace_mark         = DEFAULT_PACE_MARK;
  s_config.tap_mode          = DEFAULT_TAP_MODE;
  s_config.tap_bars          = DEFAULT_TAP_BARS;
  s_config.theme             = DEFAULT_THEME;
  s_config.weather_temp      = WEATHER_TEMP_NONE;
  s_config.weather_condition = WEATHER_COND_NONE;
  s_config.sunrise           = SUN_TIME_NONE;
  s_config.sunset            = SUN_TIME_NONE;
  s_config.custom1_value     = CUSTOM_VALUE_NONE;
  s_config.custom1_icon      = CUSTOM_ICON_GAUGE;
  s_config.custom2_value     = CUSTOM_VALUE_NONE;
  s_config.custom2_icon      = CUSTOM_ICON_GAUGE;

  // Restore the last weather snapshot unless it has gone stale. A negative age
  // means the clock moved backwards, which makes the timestamp untrustworthy.
  if (persist_exists(PERSIST_WEATHER_TIME) && persist_exists(PERSIST_WEATHER_TEMP)) {
    int age = (int)time(NULL) - persist_read_int(PERSIST_WEATHER_TIME);
    if (age >= 0 && age < WEATHER_MAX_AGE_S) {
      s_config.weather_temp      = persist_read_int(PERSIST_WEATHER_TEMP);
      s_config.weather_condition = persist_read_int(PERSIST_WEATHER_COND);
    }
  }

  // Sunrise/sunset ride along with the weather fetch and share its timestamp,
  // but they are judged against a full day rather than WEATHER_MAX_AGE_S: this
  // morning's sunrise is still correct tonight, this morning's temperature is
  // not.
  if (persist_exists(PERSIST_WEATHER_TIME) && persist_exists(PERSIST_SUNRISE)) {
    int age = (int)time(NULL) - persist_read_int(PERSIST_WEATHER_TIME);
    if (age >= 0 && age < SUN_MAX_AGE_S) {
      s_config.sunrise = persist_read_int(PERSIST_SUNRISE);
      s_config.sunset  = persist_read_int(PERSIST_SUNSET);
    }
  }

  // Restore the last custom-metric snapshot unless it has gone stale, same
  // reasoning as weather above. Both slots share one timestamp since they
  // always arrive together from the same fetch.
  if (persist_exists(PERSIST_CUSTOM_TIME) && persist_exists(PERSIST_CUSTOM1_VALUE)) {
    int age = (int)time(NULL) - persist_read_int(PERSIST_CUSTOM_TIME);
    if (age >= 0 && age < CUSTOM_MAX_AGE_S) {
      s_config.custom1_value = persist_read_int(PERSIST_CUSTOM1_VALUE);
      s_config.custom1_icon  = persist_read_int(PERSIST_CUSTOM1_ICON);
      s_config.custom2_value = persist_read_int(PERSIST_CUSTOM2_VALUE);
      s_config.custom2_icon  = persist_read_int(PERSIST_CUSTOM2_ICON);
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
  // Was a bool ("show icon + value") before the icon-only option existed. Fall
  // back to the old key so an upgrade keeps the user's choice.
  if (persist_exists(PERSIST_PROGRESS_INFO_MODE)) {
    set_enum(&s_config.progress_info, persist_read_int(PERSIST_PROGRESS_INFO_MODE),
             PROGRESS_INFO_COUNT);
  } else if (persist_exists(PERSIST_PROGRESS_INFO)) {
    s_config.progress_info = persist_read_bool(PERSIST_PROGRESS_INFO) ? PROGRESS_INFO_BOTH
                                                                      : PROGRESS_INFO_NONE;
  }
  if (persist_exists(PERSIST_ACCENT_2_ENABLE)) {
    s_config.accent_2_enable = persist_read_bool(PERSIST_ACCENT_2_ENABLE);
  }
  if (persist_exists(PERSIST_ACCENT_COLOR_2)) {
    s_config.accent_color_2 = (GColor){ .argb = (uint8_t)persist_read_int(PERSIST_ACCENT_COLOR_2) };
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
  if (persist_exists(PERSIST_GOAL_STEPS)) {
    set_goal(&s_config.goal_steps, persist_read_int(PERSIST_GOAL_STEPS));
  }
  if (persist_exists(PERSIST_GOAL_KCAL)) {
    set_goal(&s_config.goal_kcal, persist_read_int(PERSIST_GOAL_KCAL));
  }
  if (persist_exists(PERSIST_GOAL_DIST)) {
    set_goal(&s_config.goal_dist_m, persist_read_int(PERSIST_GOAL_DIST));
  }
  // DIST_AUTO (the default) and any stale out-of-range value both resolve to
  // the watch's own measurement system, same as the language below.
  set_dist_unit(&s_config.dist_unit,
                persist_exists(PERSIST_DIST_UNIT) ? persist_read_int(PERSIST_DIST_UNIT)
                                                  : DEFAULT_DIST_UNIT);
  if (persist_exists(PERSIST_PACE_MARK)) {
    s_config.pace_mark = persist_read_bool(PERSIST_PACE_MARK);
  }
  if (persist_exists(PERSIST_TAP_MODE)) {
    set_enum(&s_config.tap_mode, persist_read_int(PERSIST_TAP_MODE), TAP_COUNT);
  }
  if (persist_exists(PERSIST_TAP_BARS)) {
    s_config.tap_bars = persist_read_bool(PERSIST_TAP_BARS);
  }
  if (persist_exists(PERSIST_THEME)) {
    set_enum(&s_config.theme, persist_read_int(PERSIST_THEME), THEME_COUNT);
  }
  // LANG_AUTO (the default) and any stale out-of-range value both resolve to
  // the watch's own locale rather than to a fixed language.
  set_lang(&s_config.language,
           persist_exists(PERSIST_LANGUAGE) ? persist_read_int(PERSIST_LANGUAGE)
                                            : DEFAULT_LANGUAGE);
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
  persist_write_int(PERSIST_PROGRESS_INFO_MODE, s_config.progress_info);
  persist_write_bool(PERSIST_ACCENT_2_ENABLE, s_config.accent_2_enable);
  persist_write_int(PERSIST_ACCENT_COLOR_2, s_config.accent_color_2.argb);
  persist_write_bool(PERSIST_PROGRESS_SWAP, s_config.progress_swap);
  persist_write_int(PERSIST_WIDGET_LEFT, s_config.widget_left);
  persist_write_int(PERSIST_WIDGET_MID, s_config.widget_mid);
  persist_write_int(PERSIST_WIDGET_RIGHT, s_config.widget_right);
  persist_write_bool(PERSIST_BATTERY_PCT, s_config.battery_pct);
  persist_write_int(PERSIST_TEMP_UNIT, s_config.temp_unit);
  persist_write_int(PERSIST_GOAL_STEPS, s_config.goal_steps);
  persist_write_int(PERSIST_GOAL_KCAL, s_config.goal_kcal);
  persist_write_int(PERSIST_GOAL_DIST, s_config.goal_dist_m);
  persist_write_int(PERSIST_DIST_UNIT, s_dist_pref);  // keep AUTO as AUTO
  persist_write_bool(PERSIST_PACE_MARK, s_config.pace_mark);
  persist_write_int(PERSIST_TAP_MODE, s_config.tap_mode);
  persist_write_bool(PERSIST_TAP_BARS, s_config.tap_bars);
  persist_write_int(PERSIST_THEME, s_config.theme);
  persist_write_int(PERSIST_LANGUAGE, s_lang_pref);   // keep AUTO as AUTO
  persist_write_int(PERSIST_CLOCK_SCHEME, s_config.clock_scheme);
  persist_write_bool(PERSIST_CLOCK_24H, s_config.clock_24h);
  persist_write_bool(PERSIST_WEATHER_ACCENT, s_config.weather_accent);
}

// Stores the latest weather with a timestamp, so a relaunch shows the last
// known values instead of an empty slot while the phone re-fetches.
static void weather_save(void) {
  persist_write_int(PERSIST_WEATHER_TEMP, s_config.weather_temp);
  persist_write_int(PERSIST_WEATHER_COND, s_config.weather_condition);
  persist_write_int(PERSIST_SUNRISE, s_config.sunrise);
  persist_write_int(PERSIST_SUNSET, s_config.sunset);
  persist_write_int(PERSIST_WEATHER_TIME, (int)time(NULL));
}

// Stores the latest custom-metric values with a shared timestamp, same
// reasoning as weather_save above.
static void custom_save(void) {
  persist_write_int(PERSIST_CUSTOM1_VALUE, s_config.custom1_value);
  persist_write_int(PERSIST_CUSTOM1_ICON, s_config.custom1_icon);
  persist_write_int(PERSIST_CUSTOM2_VALUE, s_config.custom2_value);
  persist_write_int(PERSIST_CUSTOM2_ICON, s_config.custom2_icon);
  persist_write_int(PERSIST_CUSTOM_TIME, (int)time(NULL));
}

// Applies any settings/weather present in an inbound AppMessage, then redraws.
void config_inbox_received(DictionaryIterator *iter, void *context) {
  bool settings_changed = false;
  bool weather_changed = false;
  bool custom_changed = false;
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
    set_enum(&s_config.progress_info, tuple_int(t), PROGRESS_INFO_COUNT);
    settings_changed = true;
  }
  if ((t = dict_find(iter, MESSAGE_KEY_ACCENT_2_ENABLE))) {
    s_config.accent_2_enable = (tuple_int(t) != 0);
    settings_changed = true;
  }
  if ((t = dict_find(iter, MESSAGE_KEY_ACCENT_COLOR_2))) {
    s_config.accent_color_2 = GColorFromHEX(tuple_int(t));
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
  if ((t = dict_find(iter, MESSAGE_KEY_GOAL_STEPS))) {
    set_goal(&s_config.goal_steps, tuple_int(t));
    settings_changed = true;
  }
  if ((t = dict_find(iter, MESSAGE_KEY_GOAL_KCAL))) {
    set_goal(&s_config.goal_kcal, tuple_int(t));
    settings_changed = true;
  }
  // Always meters: the phone converts whichever unit the user typed in before
  // sending, so the watch never has to know about kilometres or miles here.
  if ((t = dict_find(iter, MESSAGE_KEY_GOAL_DIST))) {
    set_goal(&s_config.goal_dist_m, tuple_int(t));
    settings_changed = true;
  }
  if ((t = dict_find(iter, MESSAGE_KEY_DIST_UNIT))) {
    set_dist_unit(&s_config.dist_unit, tuple_int(t));
    settings_changed = true;
  }
  if ((t = dict_find(iter, MESSAGE_KEY_PACE_MARK))) {
    s_config.pace_mark = (tuple_int(t) != 0);
    settings_changed = true;
  }
  if ((t = dict_find(iter, MESSAGE_KEY_TAP_MODE))) {
    set_enum(&s_config.tap_mode, tuple_int(t), TAP_COUNT);
    settings_changed = true;
  }
  if ((t = dict_find(iter, MESSAGE_KEY_TAP_BARS))) {
    s_config.tap_bars = (tuple_int(t) != 0);
    settings_changed = true;
  }
  if ((t = dict_find(iter, MESSAGE_KEY_THEME))) {
    set_enum(&s_config.theme, tuple_int(t), THEME_COUNT);
    settings_changed = true;
  }
  if ((t = dict_find(iter, MESSAGE_KEY_LANGUAGE))) {
    set_lang(&s_config.language, tuple_int(t));
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

  // Sunrise/sunset arrive with the weather, as minutes since local midnight.
  // Anything outside a day is dropped rather than stored: it would otherwise
  // feed straight into the daylight bar's arithmetic.
  if ((t = dict_find(iter, MESSAGE_KEY_SUN_RISE))) {
    s_config.sunrise = sun_minutes_or_none(tuple_int(t));
    weather_changed = true;
  }
  if ((t = dict_find(iter, MESSAGE_KEY_SUN_SET))) {
    s_config.sunset = sun_minutes_or_none(tuple_int(t));
    weather_changed = true;
  }

  // Custom-metric updates arrive on the same inbox, under their own persist
  // keys. The phone always sends both value+icon for a slot together.
  if ((t = dict_find(iter, MESSAGE_KEY_CUSTOM1_VALUE))) {
    s_config.custom1_value = clamp_pct(tuple_int(t));
    custom_changed = true;
  }
  if ((t = dict_find(iter, MESSAGE_KEY_CUSTOM1_ICON))) {
    set_enum(&s_config.custom1_icon, tuple_int(t), CUSTOM_ICON_COUNT);
    custom_changed = true;
  }
  if ((t = dict_find(iter, MESSAGE_KEY_CUSTOM2_VALUE))) {
    s_config.custom2_value = clamp_pct(tuple_int(t));
    custom_changed = true;
  }
  if ((t = dict_find(iter, MESSAGE_KEY_CUSTOM2_ICON))) {
    set_enum(&s_config.custom2_icon, tuple_int(t), CUSTOM_ICON_COUNT);
    custom_changed = true;
  }

  if (settings_changed) {
    config_save();
  }
  if (weather_changed) {
    weather_save();
  }
  if (custom_changed) {
    custom_save();
  }

  if (s_change_cb) {
    s_change_cb();
  }
}
