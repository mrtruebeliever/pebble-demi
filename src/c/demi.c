#include <pebble.h>
#include <pebble-fctx/fctx.h>
#include <pebble-fctx/ffont.h>
#include "config.h"

static Window *s_window;
static Layer  *s_clock_layer;
static Layer  *s_progress_layer;
static Layer  *s_bottom_layer;
static Layer  *s_status_layer;

static FFont  *s_ffont_bold;   // big hours
static FFont  *s_ffont_light;  // big minutes
static GFont   s_font20;       // small labels / values

// Cached PDC icons (recolored at draw time).
static GDrawCommandImage *s_img_shoe, *s_img_battery, *s_img_flame, *s_img_runner, *s_img_heart;
static GDrawCommandImage *s_img_sun, *s_img_partly, *s_img_cloud;
static GDrawCommandImage *s_img_lrain, *s_img_hrain, *s_img_lsnow, *s_img_hsnow;
static GDrawCommandImage *s_img_quiet, *s_img_bt_off;  // status-row icons
static GDrawCommandImage *s_img_gauge, *s_img_claude_session, *s_img_claude_week;  // custom-metric icons
static GDrawCommandImage *s_img_sunrise, *s_img_sunset;      // next-sun-event icons
static GDrawCommandImage *s_img_duration;                    // day-elapsed bar icon

// Hidden during a Timeline Quick View slide to reduce clutter.
static bool s_peek_animating = false;

// Time / date text.
static char s_hours[4];
static char s_minutes[4];
static char s_ampm[4];   // "AM"/"PM" in 12h mode; empty in 24h mode
static char s_day[8];
static int  s_mday;

// Tap reveal: the bottom row is replaced by one of these for a few seconds.
static char s_full_date[20];   // "WE 13 AUG 2026"
static char s_seconds[4];      // ":07"
static bool s_revealing = false;
static AppTimer *s_reveal_timer = NULL;

// Health + battery snapshot.
static int  s_steps    = 0;
static int  s_kcal     = 0;
static int  s_dist_m   = 0;
static int  s_batt_pct = 0;
static bool s_charging = false;
static int  s_hr       = 0;

// ---- palette --------------------------------------------------------------
//
// Every neutral in the face is drawn through one of these four roles rather
// than a literal colour, which is what lets the light theme flip the whole
// face coherently. Naming the role at each call site also means a new
// complication cannot quietly end up unreadable in one theme: there is no
// "white" to reach for, only "the strong foreground".

static bool theme_is_light(void) {
  return config_get()->theme == THEME_LIGHT;
}

// The window's own background, and the colour to draw a mark in when it lands
// on top of the accent fill.
static GColor col_bg(void) {
  return theme_is_light() ? GColorWhite : GColorBlack;
}

// Strong foreground: the hour digits, the calendar label, the charging bolt.
static GColor col_fg(void) {
  return theme_is_light() ? GColorBlack : GColorWhite;
}

// Quieter foreground: widget text, status icons, the battery outline — a step
// down from col_fg without disappearing.
static GColor col_fg2(void) {
  return theme_is_light() ? GColorDarkGray : GColorLightGray;
}

// The unfilled part of a progress track, and the divider above the widget row:
// present, but never competing with the content.
static GColor col_track(void) {
  return theme_is_light() ? GColorLightGray : GColorDarkGray;
}

// Rough perceived lightness, 0 (black) to 12 (white). Green is weighted double
// because the eye reads it as much brighter than red or blue at the same value:
// a plain channel sum calls yellow (3,3,0) and cyan (0,3,3) no lighter than
// magenta (3,1,3), when on white the first two are the ones that disappear.
static int color_lightness(GColor c) {
  return c.r + 2 * c.g + c.b;
}

// An accent picked against black can be far too pale for white — yellow washes
// out and a white accent disappears outright. Rather than force a second
// colour setting on the wearer, the light theme darkens an accent one step per
// channel, but only when it is actually too bright to read. Dark accents pass
// through untouched, so most choices look the same in both themes.
// Of the twelve palette swatches this leaves blue, red, mint and cyan-ish
// greens alone, and darkens white, yellow, cyan, spring green, orange, pink,
// purple and magenta — which is the split you get by eye on a white ground.
#define ACCENT_MAX_LIGHTNESS_ON_LIGHT 6

static GColor themed_accent(GColor c) {
  if (!theme_is_light() || color_lightness(c) <= ACCENT_MAX_LIGHTNESS_ON_LIGHT) {
    return c;
  }
  GColor out = c;
  out.r = c.r > 0 ? c.r - 1 : 0;
  out.g = c.g > 0 ? c.g - 1 : 0;
  out.b = c.b > 0 ? c.b - 1 : 0;
  // A neutral accent (white) darkens to light gray, which is no better against
  // white than it started; greys have no hue to preserve, so take them all the
  // way down to a readable one.
  if (c.r == c.g && c.g == c.b) return GColorDarkGray;
  return out;
}

// ---- helpers --------------------------------------------------------------

// gdraw_command_list_iterate callback: render the official line-art icons as a
// colored outline (transparent fill, colored stroke) in the context color.
static bool recolor_iter(GDrawCommand *command, uint32_t index, void *context) {
  GColor c = *(GColor *)context;
  gdraw_command_set_stroke_color(command, c);
  gdraw_command_set_fill_color(command, GColorClear);
  // Thin, uniform 1px outline (the source PDCs carry a heavier 2px stroke).
  gdraw_command_set_stroke_width(command, 1);
  return true;
}

// Recolors a PDC image to a colored outline and draws it with top-left at `origin`.
static void draw_pdc(GContext *ctx, GDrawCommandImage *img, GPoint origin, GColor color) {
  if (!img) return;
  GDrawCommandList *list = gdraw_command_image_get_command_list(img);
  gdraw_command_list_iterate(list, recolor_iter, &color);
  gdraw_command_image_draw(ctx, img, origin);
}

// Returns the metric value for a health metric, or 0 if not accessible.
static int health_today(HealthMetric metric) {
  HealthServiceAccessibilityMask mask =
      health_service_metric_accessible(metric, time_start_of_today(), time(NULL));
  if (mask & HealthServiceAccessibilityMaskAvailable) {
    return (int)health_service_sum_today(metric);
  }
  return 0;
}

// Refreshes cached health values from the health service.
static void update_health(void) {
  s_steps  = health_today(HealthMetricStepCount);
  s_kcal   = health_today(HealthMetricActiveKCalories);
  s_dist_m = health_today(HealthMetricWalkedDistanceMeters);
  s_hr     = (int)health_service_peek_current_value(HealthMetricHeartRateBPM);
}

// Formats a value as "6.1k" / "10k" / "950" into buf.
static void format_k(int value, char *buf, size_t len) {
  if (value >= 10000) {
    snprintf(buf, len, "%dk", value / 1000);
  } else if (value >= 1000) {
    snprintf(buf, len, "%d.%dk", value / 1000, (value % 1000) / 100);
  } else {
    snprintf(buf, len, "%d", value);
  }
}

// Formats a distance held in meters for display. Health always reports meters;
// the wearer's measurement system only decides what they read here, so the
// conversion lives at the point of drawing and nowhere else.
static void format_distance(int meters, char *buf, size_t len) {
  if (config_get()->dist_unit == DIST_MILES) {
    // Tenths of a mile, so a short walk isn't just "0". No "mi" suffix: the
    // value only gets ~44px beside the track, which "2.0mi" overflows — and
    // the metric case is just as bare once past 10k ("12k", "950"). The runner
    // icon says what the number is, and the unit is the wearer's own setting.
    int tenths = (meters * 10 + METERS_PER_MILE / 2) / METERS_PER_MILE;
    if (tenths >= 100) {
      snprintf(buf, len, "%d", tenths / 10);   // 10 miles and up: drop the decimal
    } else {
      snprintf(buf, len, "%d.%d", tenths / 10, tenths % 10);
    }
  } else {
    format_k(meters, buf, len);
  }
}

// Returns the wearer's goal for a health metric. A goal of GOAL_AVERAGE means
// "whatever I normally do in a day", which the watch can answer from its own
// history; with no history to average yet, the old fixed target stands in.
static int goal_for(HealthMetric metric, int configured, int fallback) {
  if (configured != GOAL_AVERAGE) {
    return configured;
  }
  time_t start = time_start_of_today();
  HealthServiceAccessibilityMask mask =
      health_service_metric_averaged_accessible(metric, start, start + SECONDS_PER_DAY,
                                                HealthServiceTimeScopeDaily);
  if (mask & HealthServiceAccessibilityMaskAvailable) {
    int avg = (int)health_service_sum_averaged(metric, start, start + SECONDS_PER_DAY,
                                               HealthServiceTimeScopeDaily);
    if (avg > 0) return avg;
  }
  return fallback;
}

// Percentage of a goal reached, capped at 100. A goal of 0 would divide by
// zero; goal_for() never returns one, but the bar arithmetic is not the place
// to rely on that.
static int pct_of_goal(int value, int goal) {
  if (goal <= 0) return 0;
  return value >= goal ? 100 : value * 100 / goal;
}

// Where the wearer normally stands by this time of day, as a percentage of the
// same goal the fill is measured against — so the mark and the fill can be read
// against each other. Returns PACE_NONE when the watch has nothing to compare
// against, which is also the case for every non-health metric.
#define PACE_NONE (-1)

static int pace_pct_for(HealthMetric metric, int goal) {
  time_t start = time_start_of_today();
  time_t now = time(NULL);
  HealthServiceAccessibilityMask mask =
      health_service_metric_averaged_accessible(metric, start, now, HealthServiceTimeScopeDaily);
  if (!(mask & HealthServiceAccessibilityMaskAvailable)) {
    return PACE_NONE;
  }
  int typical = (int)health_service_sum_averaged(metric, start, now, HealthServiceTimeScopeDaily);
  if (typical <= 0) return PACE_NONE;
  return pct_of_goal(typical, goal);
}

// Minutes since local midnight, the same frame the phone sends sunrise and
// sunset in.
static int minutes_now(void) {
  time_t now = time(NULL);
  struct tm *tm = localtime(&now);
  return tm->tm_hour * 60 + tm->tm_min;
}

// True when both ends of today's daylight are known. A single one is useless:
// every sun calculation here is a position within a range.
static bool sun_known(const DemiConfig *cfg) {
  return cfg->sunrise != SUN_TIME_NONE && cfg->sunset != SUN_TIME_NONE
      && cfg->sunset > cfg->sunrise;
}

// Reference points drawn across a bar's track, on top of the fill: the wearer's
// usual pace on a health bar, or sunrise and sunset on the day bar. Two is all
// any bar needs, and more than two notches would read as noise on a 6px track.
#define BAR_MARKS_MAX 2

typedef struct {
  int count;
  int pct[BAR_MARKS_MAX];
} BarMarks;

static void marks_add(BarMarks *m, int pct) {
  if (m->count < BAR_MARKS_MAX && pct >= 0 && pct <= 100) {
    m->pct[m->count++] = pct;
  }
}

// Adds the wearer's usual-pace mark to a health bar, if they asked for it and
// the watch has enough history to answer.
static void add_pace_mark(BarMarks *marks, HealthMetric metric, int goal) {
  if (!config_get()->pace_mark) return;
  int pace = pace_pct_for(metric, goal);
  if (pace != PACE_NONE) marks_add(marks, pace);
}

// Adds sunrise and sunset to the day bar, as their position within the 24-hour
// span the bar covers. Only the day bar: on the daylight bar they are the two
// ends by definition, and on the week/month/year bars a single day's light is
// too compressed to mean anything.
static void add_sun_marks(BarMarks *marks) {
  DemiConfig *cfg = config_get();
  if (!sun_known(cfg)) return;
  marks_add(marks, cfg->sunrise * 100 / MINUTES_PER_DAY);
  marks_add(marks, cfg->sunset * 100 / MINUTES_PER_DAY);
}

// Draws each mark as a 2px line across the track. The colour follows what the
// mark lands on: black reads on the bright accent fill but nearly vanishes on
// the dark-gray track, and light gray does the reverse. Marks are positioned
// the same way the fill is, so a swapped bar keeps them lined up with what they
// refer to.
static void draw_bar_marks(GContext *ctx, const BarMarks *marks, int fill_pct,
                           bool swapped, GRect track, bool vertical) {
  for (int i = 0; i < marks->count; i++) {
    graphics_context_set_fill_color(
        ctx, marks->pct[i] <= fill_pct ? col_bg() : col_fg2());
    if (vertical) {
      int off = track.size.h * marks->pct[i] / 100;
      int y = swapped ? track.origin.y + track.size.h - off : track.origin.y + off;
      graphics_fill_rect(ctx, GRect(track.origin.x, y - 1, track.size.w, 2), 0, GCornerNone);
    } else {
      int off = track.size.w * marks->pct[i] / 100;
      int x = swapped ? track.origin.x + track.size.w - off : track.origin.x + off;
      graphics_fill_rect(ctx, GRect(x - 1, track.origin.y, 2, track.size.h), 0, GCornerNone);
    }
  }
}

// Formats a duration in minutes as "3h20" or "45m", within the small font's
// character set.
static void format_duration(int minutes, char *buf, size_t len) {
  if (minutes >= 60) {
    snprintf(buf, len, "%dh%02d", minutes / 60, minutes % 60);
  } else {
    snprintf(buf, len, "%dm", minutes);
  }
}

static bool is_leap_year(int year) {
  return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

static int days_in_month(int mon, int year) {
  static const int DAYS[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
  if (mon == 1 && is_leap_year(year)) return 29;
  return DAYS[mon % 12];
}

// Percentage of a calendar period already elapsed, to the minute. These bars
// need neither the phone nor a sensor, so unlike every other metric they
// always have a real value to show.
static int calendar_pct(int type, const struct tm *tm) {
  int mins_today = tm->tm_hour * 60 + tm->tm_min;
  switch (type) {
    case PROGRESS_WEEK: {
      // tm_wday counts Sunday as 0, which would start the week mid-weekend.
      int wday = (tm->tm_wday + 6) % 7;
      return (wday * MINUTES_PER_DAY + mins_today) * 100 / (7 * MINUTES_PER_DAY);
    }
    case PROGRESS_MONTH: {
      int days = days_in_month(tm->tm_mon, tm->tm_year + 1900);
      return ((tm->tm_mday - 1) * MINUTES_PER_DAY + mins_today) * 100 / (days * MINUTES_PER_DAY);
    }
    case PROGRESS_YEAR: {
      int days = is_leap_year(tm->tm_year + 1900) ? 366 : 365;
      return (tm->tm_yday * MINUTES_PER_DAY + mins_today) * 100 / (days * MINUTES_PER_DAY);
    }
    case PROGRESS_DAY:
    default:
      return mins_today * 100 / MINUTES_PER_DAY;
  }
}

// Maps a custom-metric icon setting (chosen phone-side from the JSON item's
// "name") to its PDC image.
static GDrawCommandImage *custom_icon_image(int icon) {
  switch (icon) {
    case CUSTOM_ICON_CLAUDE_SESSION: return s_img_claude_session;
    case CUSTOM_ICON_CLAUDE_WEEK:    return s_img_claude_week;
    case CUSTOM_ICON_GAUGE:
    default:                         return s_img_gauge;
  }
}

// Calendar glyph geometry, shared by the date widget and the elapsed-period
// bars so the same shape means the same thing in both places.
#define CAL_W  20
#define CAL_H  18

// Draws the calendar box — rounded outline, two binding lines, filled header —
// with `label` centred in its body. The box takes the accent colour and the
// label white, which is how the date widget has always drawn its day number.
static void draw_calendar_box(GContext *ctx, int x, int cy, const char *label, GColor color) {
  int top = cy - CAL_H / 2;
  graphics_context_set_stroke_color(ctx, color);
  graphics_context_set_fill_color(ctx, color);
  graphics_draw_line(ctx, GPoint(x + 5, top - 3), GPoint(x + 5, top));
  graphics_draw_line(ctx, GPoint(x + CAL_W - 5, top - 3), GPoint(x + CAL_W - 5, top));
  graphics_draw_round_rect(ctx, GRect(x, top, CAL_W, CAL_H), 3);
  graphics_fill_rect(ctx, GRect(x + 1, top + 1, CAL_W - 2, 3), 0, GCornerNone);

  graphics_context_set_text_color(ctx, col_fg());
  // GOTHIC has top padding; pull the label up so it sits centered in the body.
  graphics_draw_text(ctx, label, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(x, top - 2, CAL_W, CAL_H),
                     GTextOverflowModeFill, GTextAlignmentCenter, NULL);
}

// The initial of "week" / "month" / "year" in each language, for the calendar
// badge on those bars. Month is M everywhere; week and year are not, so this
// follows the configured language rather than assuming English.
static const char *period_initial(int type) {
  static const char *const INITIAL[LANG_COUNT][3] = {
    { "W", "M", "Y" },  // EN  week / month / year
    { "W", "M", "J" },  // NL  week / maand / jaar
    { "S", "M", "A" },  // FR  semaine / mois / année
    { "W", "M", "J" },  // DE  Woche / Monat / Jahr
    { "S", "M", "A" },  // ES  semana / mes / año
  };
  int lang = config_get()->language;
  if (lang < 0 || lang >= LANG_COUNT) lang = LANG_EN;
  int idx = (type == PROGRESS_WEEK) ? 0 : (type == PROGRESS_MONTH) ? 1 : 2;
  return INITIAL[lang][idx];
}

// Computes a progress metric: percentage, label, icon and fill color. The type
// is passed in rather than read from the config, so the dual layout can render
// two different metrics.
static void compute_progress(int type, GColor accent, int *pct, char *buf, size_t len,
                             GDrawCommandImage **icon, GColor *fill, BarMarks *marks,
                             const char **badge) {
  *fill = accent;
  marks->count = 0;
  // Set only by the week/month/year bars, which draw a calendar box with this
  // letter in it instead of an icon -- see draw_bar_icon().
  *badge = NULL;
  switch (type) {
    case PROGRESS_BATTERY:
      *pct = s_batt_pct;
      snprintf(buf, len, "%d%%", s_batt_pct);
      *icon = s_img_battery;
      if (s_batt_pct < 20) *fill = GColorRed;
      break;
    case PROGRESS_CALORIES: {
      int goal = goal_for(HealthMetricActiveKCalories,
                          config_get()->goal_kcal, FALLBACK_GOAL_KCAL);
      *pct = pct_of_goal(s_kcal, goal);
      add_pace_mark(marks, HealthMetricActiveKCalories, goal);
      snprintf(buf, len, "%d", s_kcal);
      *icon = s_img_flame;
      break;
    }
    case PROGRESS_DISTANCE: {
      int goal = goal_for(HealthMetricWalkedDistanceMeters,
                          config_get()->goal_dist_m, FALLBACK_GOAL_DIST_M);
      *pct = pct_of_goal(s_dist_m, goal);
      add_pace_mark(marks, HealthMetricWalkedDistanceMeters, goal);
      format_distance(s_dist_m, buf, len);
      *icon = s_img_runner;
      break;
    }
    case PROGRESS_CUSTOM_1: {
      DemiConfig *cfg = config_get();
      int v = cfg->custom1_value;
      *pct = (v == CUSTOM_VALUE_NONE) ? 0 : v;
      if (v == CUSTOM_VALUE_NONE) { snprintf(buf, len, "--"); } else { snprintf(buf, len, "%d%%", v); }
      *icon = custom_icon_image(cfg->custom1_icon);
      break;
    }
    case PROGRESS_CUSTOM_2: {
      DemiConfig *cfg = config_get();
      int v = cfg->custom2_value;
      *pct = (v == CUSTOM_VALUE_NONE) ? 0 : v;
      if (v == CUSTOM_VALUE_NONE) { snprintf(buf, len, "--"); } else { snprintf(buf, len, "%d%%", v); }
      *icon = custom_icon_image(cfg->custom2_icon);
      break;
    }
    case PROGRESS_DAYLIGHT: {
      DemiConfig *cfg = config_get();
      *icon = s_img_sun;
      if (!sun_known(cfg)) {
        // Same contract as the custom slots and the weather widget: show that
        // nothing is known rather than a fill that means nothing.
        *pct = 0;
        snprintf(buf, len, "--");
        break;
      }
      int now = minutes_now();
      if (now <= cfg->sunrise) {
        *pct = 0;
        format_duration(cfg->sunrise - now, buf, len);   // until first light
      } else if (now >= cfg->sunset) {
        *pct = 100;
        snprintf(buf, len, "0m");
      } else {
        int span = cfg->sunset - cfg->sunrise;
        *pct = (now - cfg->sunrise) * 100 / span;
        format_duration(cfg->sunset - now, buf, len);    // daylight left
      }
      break;
    }
    case PROGRESS_DAY:
    case PROGRESS_WEEK:
    case PROGRESS_MONTH:
    case PROGRESS_YEAR: {
      time_t now = time(NULL);
      *pct = calendar_pct(type, localtime(&now));
      snprintf(buf, len, "%d%%", *pct);
      if (type == PROGRESS_DAY) {
        *icon = s_img_duration;
        add_sun_marks(marks);
      } else {
        // PebbleOS has no week/month/year icons, so these three draw the same
        // calendar box the date widget uses, with the period's initial in it --
        // which tells them apart in a way one shared calendar icon could not.
        *badge = period_initial(type);
      }
      break;
    }
    case PROGRESS_STEPS:
    default: {
      int goal = goal_for(HealthMetricStepCount,
                          config_get()->goal_steps, FALLBACK_GOAL_STEPS);
      *pct = pct_of_goal(s_steps, goal);
      add_pace_mark(marks, HealthMetricStepCount, goal);
      format_k(s_steps, buf, len);
      *icon = s_img_shoe;
      break;
    }
  }
}

// ---- drawing --------------------------------------------------------------

// Draws the big hours (white, bold, top) and minutes (gray, light, bottom).
// Maps the configured clock scheme to its hour and minute fill colors.
static void clock_scheme_colors(int scheme, GColor *hours, GColor *minutes) {
  GColor accent = themed_accent(config_get()->accent_color);
  switch (scheme) {
    case CLOCK_SCHEME_WHITE_WHITE:  *hours = col_fg();     *minutes = col_fg();     break;
    case CLOCK_SCHEME_WHITE_LIGHT:  *hours = col_fg();     *minutes = col_fg2();    break;
    case CLOCK_SCHEME_LIGHT_WHITE:  *hours = col_fg2();    *minutes = col_fg();     break;
    case CLOCK_SCHEME_ACCENT_WHITE: *hours = accent;       *minutes = col_fg();     break;
    case CLOCK_SCHEME_WHITE_ACCENT: *hours = col_fg();     *minutes = accent;       break;
    case CLOCK_SCHEME_ACCENT_GRAY:  *hours = accent;       *minutes = col_track();  break;
    case CLOCK_SCHEME_ACCENT_LIGHT: *hours = accent;       *minutes = col_fg2();    break;
    case CLOCK_SCHEME_WHITE_GRAY:
    default:                        *hours = col_fg();     *minutes = col_track();  break;
  }
}

// Vertical progressbar geometry (horizontal layout): a 6px track down the
// middle of the clock area, with breathing room kept clear either side of it.
#define VBAR_HALF_W  3
#define VBAR_GAP     8

// Two-bar layout: the digits sit in each half, with this much clearance in the
// middle (total) and against the screen edge.
#define DUAL_GAP   16
#define DUAL_EDGE  8

// Space reserved either side of a track for what sits beside it. Symmetric so
// the track's midpoint stays put; NONE still keeps the bar off the bezel.
static int bar_side_reserve(int info_mode) {
  switch (info_mode) {
    case PROGRESS_INFO_BOTH: return 48;
    case PROGRESS_INFO_ICON: return 32;
    default:                 return 20;
  }
}

// Sets the em height, shrinking it until the string fits within max_w. The
// stacked layout can size digits off the layer height alone; side-by-side they
// also have to clear their column. Returns the em height actually applied.
static int fit_em(FContext *fctx, FFont *font, const char *s, int max_w, int em) {
  fctx_set_text_em_height(fctx, font, em);
  int w = FIXED_TO_INT(fctx_string_width(fctx, s, font));
  if (w > max_w && w > 0) {
    em = em * max_w / w;
    fctx_set_text_em_height(fctx, font, em);
  }
  return em;
}

// Stacked digits with the horizontal progressbar crossing the gap between them.
// Reports where the AM/PM label goes; a zero-width rect means "no room".
static void clock_draw_stacked(FContext *fctx, int W, int H, GColor hour_color,
                               GColor minute_color, GRect *ampm, GTextAlignment *ampm_align) {
  // Anchor on the digit cap-height so the placement is predictable (digits
  // have no descenders): hours high in the top third, minutes in the lower third.
  int hour_y = H * 21 / 100;
  fctx_begin_fill(fctx);
  fctx_set_fill_color(fctx, hour_color);
  fctx_set_offset(fctx, FPointI(W / 2, hour_y));
  fctx_set_text_em_height(fctx, s_ffont_bold, H * 54 / 100);
  int hour_w = FIXED_TO_INT(fctx_string_width(fctx, s_hours, s_ffont_bold));
  fctx_draw_string(fctx, s_hours, s_ffont_bold, GTextAlignmentCenter, FTextAnchorCapMiddle);
  fctx_end_fill(fctx);

  fctx_begin_fill(fctx);
  fctx_set_fill_color(fctx, minute_color);
  fctx_set_offset(fctx, FPointI(W / 2, H * 77 / 100));
  fctx_set_text_em_height(fctx, s_ffont_light, H * 49 / 100);
  fctx_draw_string(fctx, s_minutes, s_ffont_light, GTextAlignmentCenter, FTextAnchorCapMiddle);
  fctx_end_fill(fctx);

  // Tucked right of the hour digits.
  int x = W / 2 + hour_w / 2 + 4;
  int w = W - x - 2;
  *ampm = (w > 18) ? GRect(x, hour_y - 11, w, 22) : GRectZero;
  *ampm_align = GTextAlignmentLeft;
}

// Side-by-side digits, vertically centered, each centred on hour_cx / min_cx and
// fitted to max_w. The digits are width-bound rather than height-bound, so max_w
// is what actually decides how big they come out; em is only the ceiling.
static void clock_draw_side_by_side(FContext *fctx, int W, int H, GColor hour_color,
                                    GColor minute_color, int hour_cx, int min_cx,
                                    int max_w, int em, GRect *ampm, GTextAlignment *ampm_align) {
  int cy = H / 2;

  // Both fonts are sized off a two-digit reference rather than the live string:
  // fitting "11" and "00" individually would resize the digits as time passed.
  fctx_begin_fill(fctx);
  fctx_set_fill_color(fctx, hour_color);
  fctx_set_offset(fctx, FPointI(hour_cx, cy));
  int hour_em = fit_em(fctx, s_ffont_bold, "00", max_w, em);
  fctx_draw_string(fctx, s_hours, s_ffont_bold, GTextAlignmentCenter, FTextAnchorCapMiddle);
  fctx_end_fill(fctx);

  fctx_begin_fill(fctx);
  fctx_set_fill_color(fctx, minute_color);
  fctx_set_offset(fctx, FPointI(min_cx, cy));
  fit_em(fctx, s_ffont_light, "00", max_w, em);
  fctx_draw_string(fctx, s_minutes, s_ffont_light, GTextAlignmentCenter, FTextAnchorCapMiddle);
  fctx_end_fill(fctx);

  // Centered under the hour digits, where the stacked layout has no room.
  *ampm = GRect(hour_cx - max_w / 2, cy + hour_em / 2 + 4, max_w, 22);
  *ampm_align = GTextAlignmentCenter;
}

// Vertical-bar layout: each half is the space between the bar and the screen
// edge, and the digits centre in it. The icon and value sit above and below the
// bar, clear of the digits, so they need no horizontal room.
static void clock_draw_beside_bar(FContext *fctx, int W, int H, GColor hour_color,
                                  GColor minute_color, GRect *ampm, GTextAlignment *ampm_align) {
  int bar_left = W / 2 - VBAR_HALF_W;
  int bar_right = W / 2 + VBAR_HALF_W;
  clock_draw_side_by_side(fctx, W, H, hour_color, minute_color,
                          bar_left / 2, (bar_right + W) / 2,
                          bar_left - 2 * VBAR_GAP, H * 40 / 100, ampm, ampm_align);
}

// Two-bar layout: nothing runs down the middle, so the digits get their whole
// half, less a gap between them and a margin off the bezel.
static void clock_draw_dual(FContext *fctx, int W, int H, GColor hour_color,
                            GColor minute_color, GRect *ampm, GTextAlignment *ampm_align) {
  int max_w = W / 2 - DUAL_GAP / 2 - DUAL_EDGE;
  clock_draw_side_by_side(fctx, W, H, hour_color, minute_color,
                          W / 4, W - W / 4, max_w, H * 50 / 100, ampm, ampm_align);
}

static void clock_update_proc(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  int W = b.size.w, H = b.size.h;

  GColor hour_color, minute_color;
  clock_scheme_colors(config_get()->clock_scheme, &hour_color, &minute_color);

  GRect ampm = GRectZero;
  GTextAlignment ampm_align = GTextAlignmentLeft;

  FContext fctx;
  fctx_init_context(&fctx, ctx);
  switch (config_get()->layout_mode) {
    case LAYOUT_HORIZONTAL:
      clock_draw_beside_bar(&fctx, W, H, hour_color, minute_color, &ampm, &ampm_align);
      break;
    case LAYOUT_DUAL:
      clock_draw_dual(&fctx, W, H, hour_color, minute_color, &ampm, &ampm_align);
      break;
    default:
      clock_draw_stacked(&fctx, W, H, hour_color, minute_color, &ampm, &ampm_align);
      break;
  }
  fctx_deinit_context(&fctx);

  // AM/PM indicator (12h mode): raster font, so it waits until FCTX is done.
  if (s_ampm[0] && ampm.size.w > 0) {
    graphics_context_set_text_color(ctx, col_fg2());
    graphics_draw_text(ctx, s_ampm, s_font20, ampm, GTextOverflowModeFill, ampm_align, NULL);
  }
}

// How much a bar shows beside its track at this moment. Normally the wearer's
// setting, but a tap reveal temporarily promotes it to icon + value: the point
// of keeping the bars bare is a clean face, not never seeing the numbers.
static int active_progress_info(void) {
  DemiConfig *cfg = config_get();
  if (s_revealing && cfg->tap_bars) return PROGRESS_INFO_BOTH;
  return cfg->progress_info;
}

// Draws whatever stands in for the metric beside the track: a PDC icon, or the
// calendar box with a period initial for the week/month/year bars. Returns the
// width it occupied so callers can place it from either edge.
static int bar_icon_width(GDrawCommandImage *icon, const char *badge) {
  if (badge) return CAL_W;
  if (icon) return gdraw_command_image_get_bounds_size(icon).w;
  return 0;
}

static void draw_bar_icon(GContext *ctx, GDrawCommandImage *icon, const char *badge,
                          int x, int cy, GColor color) {
  if (badge) {
    draw_calendar_box(ctx, x, cy, badge, color);
  } else if (icon) {
    GSize sz = gdraw_command_image_get_bounds_size(icon);
    draw_pdc(ctx, icon, GPoint(x, cy - sz.h / 2), color);
  }
}

// Horizontal bar within the given rect: icon left of the track, value right of
// it. Honours b.origin so the dual layout can place it as a strip; the stacked
// layout passes the layer's own bounds and lands at the same place as before.
static void draw_bar_horizontal(GContext *ctx, GRect b, GColor accent, int pct, const char *val,
                                GDrawCommandImage *icon, GColor fill, const BarMarks *marks,
                                const char *badge) {
  DemiConfig *cfg = config_get();
  int cy = b.origin.y + b.size.h / 2;

  // Equal reserve on both sides keeps the track midpoint at the rect's centre,
  // so it stays aligned with the digits. Showing less beside the bar hands part
  // of that reserve back to the track, keeping a margin off the bezel.
  int info = active_progress_info();
  int side = bar_side_reserve(info);
  int track_x = b.origin.x + side;
  int track_right = b.origin.x + b.size.w - side;
  int track_w = track_right - track_x;
  if (track_w < 0) track_w = 0;

  graphics_context_set_fill_color(ctx, col_track());
  graphics_fill_rect(ctx, GRect(track_x, cy - 3, track_w, 6), 3, GCornersAll);

  // The fill grows away from the icon, so swapping the icon over also flips
  // the direction: left-to-right normally, right-to-left when swapped.
  int fill_w = track_w * pct / 100;
  int fill_x = cfg->progress_swap ? track_right - fill_w : track_x;
  graphics_context_set_fill_color(ctx, fill);
  graphics_fill_rect(ctx, GRect(fill_x, cy - 3, fill_w, 6), 3, GCornersAll);

  draw_bar_marks(ctx, marks, pct, cfg->progress_swap, GRect(track_x, cy - 3, track_w, 6), false);

  if (info == PROGRESS_INFO_NONE) return;

  // Icon as an accent outline, matching the line-art style of the bottom bar.
  // Swapped, the icon takes the right edge and the value the left.
  int iw = bar_icon_width(icon, badge);
  if (iw > 0) {
    int ix = cfg->progress_swap ? b.origin.x + b.size.w - 4 - iw : b.origin.x + 4;
    draw_bar_icon(ctx, icon, badge, ix, cy, accent);
  }

  if (info != PROGRESS_INFO_BOTH) return;

  // The value hugs the track on whichever side it lands, so it aligns away
  // from its own edge.
  graphics_context_set_text_color(ctx, accent);
  graphics_draw_text(ctx, val, s_font20,
                     GRect(cfg->progress_swap ? b.origin.x + 4 : track_right,
                           cy - 11, side - 4, 22),
                     GTextOverflowModeFill,
                     cfg->progress_swap ? GTextAlignmentLeft : GTextAlignmentRight, NULL);
}

// Side-by-side layout: vertical bar splitting the digits, icon above the track
// and value below it — the stacked layout's arrangement rotated a quarter turn.
static void draw_bar_vertical(GContext *ctx, GRect b, GColor accent, int pct, const char *val,
                              GDrawCommandImage *icon, GColor fill, const BarMarks *marks,
                              const char *badge) {
  DemiConfig *cfg = config_get();
  int cx = b.size.w / 2;

  // Room above for the icon and below for the value, symmetric so the track
  // stays centered on the digits' midline. Showing less keeps a margin rather
  // than running the bar to the very edge.
  int info = active_progress_info();
  int side = info == PROGRESS_INFO_BOTH ? 34
           : info == PROGRESS_INFO_ICON ? 30 : 20;
  int track_h = b.size.h - 2 * side;
  if (track_h < 0) track_h = 0;

  graphics_context_set_fill_color(ctx, col_track());
  graphics_fill_rect(ctx, GRect(cx - VBAR_HALF_W, side, VBAR_HALF_W * 2, track_h), 3, GCornersAll);

  // Fills away from the icon, mirroring the horizontal bar: top-down normally,
  // bottom-up when swapped drops the icon below the track.
  int fill_h = track_h * pct / 100;
  int fill_y = cfg->progress_swap ? side + track_h - fill_h : side;
  graphics_context_set_fill_color(ctx, fill);
  graphics_fill_rect(ctx, GRect(cx - VBAR_HALF_W, fill_y, VBAR_HALF_W * 2, fill_h), 3, GCornersAll);

  draw_bar_marks(ctx, marks, pct, cfg->progress_swap,
                 GRect(cx - VBAR_HALF_W, side, VBAR_HALF_W * 2, track_h), true);

  if (info == PROGRESS_INFO_NONE) return;

  // Swapped, the icon drops below the track and the value rises above it.
  int iw = bar_icon_width(icon, badge);
  if (iw > 0) {
    int ih = badge ? CAL_H : gdraw_command_image_get_bounds_size(icon).h;
    int iy = cfg->progress_swap ? b.size.h - 4 - ih : 4;
    // draw_calendar_box centres on cy, the PDC path draws from its top edge.
    draw_bar_icon(ctx, icon, badge, cx - iw / 2, badge ? iy + ih / 2 : iy, accent);
  }

  if (info != PROGRESS_INFO_BOTH) return;

  graphics_context_set_text_color(ctx, accent);
  graphics_draw_text(ctx, val, s_font20,
                     GRect(cx - 30, cfg->progress_swap ? 4 : b.size.h - side + 4, 60, 22),
                     GTextOverflowModeFill, GTextAlignmentCenter, NULL);
}

// Draws one bar for the given metric within rect r, in the given accent color.
static void draw_metric_bar(GContext *ctx, GRect r, int type, GColor accent, bool vertical) {
  int pct;
  BarMarks marks;
  const char *badge;
  char val[8];
  GDrawCommandImage *icon = NULL;
  GColor fill;
  compute_progress(type, accent, &pct, val, sizeof(val), &icon, &fill, &marks, &badge);
  if (vertical) {
    draw_bar_vertical(ctx, r, accent, pct, val, icon, fill, &marks, badge);
  } else {
    draw_bar_horizontal(ctx, r, accent, pct, val, icon, fill, &marks, badge);
  }
}

// Draws the progressbar(s): icon, track, accent fill and the value label.
static void progress_update_proc(Layer *layer, GContext *ctx) {
  DemiConfig *cfg = config_get();
  GRect b = layer_get_bounds(layer);

  // The second bar only takes its own color once that is switched on, so
  // changing the main accent doesn't leave it stranded on an old hue.
  GColor accent = themed_accent(cfg->accent_color);
  GColor accent_2 = themed_accent(cfg->accent_2_enable ? cfg->accent_color_2
                                                       : cfg->accent_color);

  switch (cfg->layout_mode) {
    case LAYOUT_HORIZONTAL:
      draw_metric_bar(ctx, b, cfg->progress_type, accent, true);
      break;

    case LAYOUT_DUAL: {
      // Two strips framing the digits: the top one clears the status icons in
      // the corners, the bottom one stays off the widget row's divider.
      int strip_h = 28;
      int top_cy = b.size.h * 25 / 100;
      int bot_cy = b.size.h * 75 / 100;
      draw_metric_bar(ctx, GRect(0, top_cy - strip_h / 2, b.size.w, strip_h),
                      cfg->progress_type, accent, false);
      draw_metric_bar(ctx, GRect(0, bot_cy - strip_h / 2, b.size.w, strip_h),
                      cfg->progress_type_2, accent_2, false);
      break;
    }

    default:
      draw_metric_bar(ctx, b, cfg->progress_type, accent, false);
      break;
  }
}

// Selects the icon and color for a weather condition code.
static GDrawCommandImage *weather_icon(int cond, GColor *color) {
  // The dark theme's hues were chosen against black: chrome yellow and celeste
  // are near-invisible on white, so the light theme takes deeper equivalents of
  // the same three families rather than a different palette.
  bool light = theme_is_light();
  GColor sunny = light ? GColorWindsorTan : GColorChromeYellow;
  GColor blue  = light ? GColorCobaltBlue : GColorPictonBlue;
  GColor snow  = light ? GColorBlueMoon   : GColorCeleste;
  switch (cond) {
    case WEATHER_SUN:        *color = sunny; return s_img_sun;
    case WEATHER_PARTLY:     *color = blue;  return s_img_partly;
    case WEATHER_CLOUD:      *color = blue;  return s_img_cloud;
    case WEATHER_LIGHT_RAIN: *color = blue;  return s_img_lrain;
    case WEATHER_HEAVY_RAIN: *color = blue;  return s_img_hrain;
    case WEATHER_LIGHT_SNOW: *color = snow;  return s_img_lsnow;
    case WEATHER_HEAVY_SNOW: *color = snow;  return s_img_hsnow;
    default:                 *color = blue;  return s_img_cloud;
  }
}

// Battery glyph geometry (a small body rect plus a positive-terminal nub).
#define BATT_BODY_W 22
#define BATT_H      12
#define BATT_NUB_W  2

// The next sun event: writes its clock time into buf and sets *is_rise.
// Returns false when today's daylight is unknown, which is how the widget
// knows to claim no space at all. After sunset the answer is the next
// sunrise — today's, since tomorrow's differs by barely a minute.
static bool next_sun_event(char *buf, size_t len, bool *is_rise) {
  DemiConfig *cfg = config_get();
  if (!sun_known(cfg)) return false;
  int now = minutes_now();
  *is_rise = (now < cfg->sunrise) || (now >= cfg->sunset);
  int at = *is_rise ? cfg->sunrise : cfg->sunset;
  int hour = at / 60;
  if (!cfg->clock_24h) {
    hour = hour % 12;
    if (hour == 0) hour = 12;
    // No AM/PM label: a sunrise is always morning and a sunset always evening,
    // and the arrow already says which one this is.
  }
  snprintf(buf, len, "%d:%02d", hour, at % 60);
  return true;
}

// Returns the pixel width a widget needs, or 0 for WIDGET_NONE / no content.
// Mirrors the layout each draw_widget_* path produces so slots can be placed.
static int widget_width(int type) {
  DemiConfig *cfg = config_get();
  switch (type) {
    case WIDGET_DATE: {
      char da[3] = { s_day[0], s_day[1], 0 };
      GSize daw = graphics_text_layout_get_content_size(da, s_font20, GRect(0, 0, 40, 22),
                                                       GTextOverflowModeFill, GTextAlignmentLeft);
      return daw.w + 5 + CAL_W;  // weekday + gap + calendar box
    }
    case WIDGET_WEATHER: {
      // Nothing known yet (or the stored reading expired): claim no space rather
      // than show a placeholder condition the watch cannot vouch for.
      if (cfg->weather_temp == WEATHER_TEMP_NONE) return 0;
      char ws[8];
      snprintf(ws, sizeof(ws), "%d°", cfg->weather_temp);
      GColor wc;
      GDrawCommandImage *wi = weather_icon(cfg->weather_condition, &wc);
      if (!wi) return 0;
      GSize sz = gdraw_command_image_get_bounds_size(wi);
      GSize vw = graphics_text_layout_get_content_size(ws, s_font20, GRect(0, 0, 60, 22),
                                                       GTextOverflowModeFill, GTextAlignmentLeft);
      return sz.w + 1 + vw.w;
    }
    case WIDGET_HEART: {
      char hs[8];
      if (s_hr > 0) snprintf(hs, sizeof(hs), "%d", s_hr);
      else snprintf(hs, sizeof(hs), "--");
      if (!s_img_heart) return 0;
      GSize sz = gdraw_command_image_get_bounds_size(s_img_heart);
      GSize vw = graphics_text_layout_get_content_size(hs, s_font20, GRect(0, 0, 60, 22),
                                                       GTextOverflowModeFill, GTextAlignmentLeft);
      return sz.w + 1 + vw.w;
    }
    case WIDGET_SUN: {
      char ss[8];
      bool is_rise;
      // Mirrors the weather slot: nothing known yet means no space claimed,
      // rather than a placeholder time the watch cannot vouch for.
      if (!next_sun_event(ss, sizeof(ss), &is_rise)) return 0;
      GDrawCommandImage *si = is_rise ? s_img_sunrise : s_img_sunset;
      if (!si) return 0;
      GSize sz = gdraw_command_image_get_bounds_size(si);
      GSize vw = graphics_text_layout_get_content_size(ss, s_font20, GRect(0, 0, 60, 22),
                                                       GTextOverflowModeFill, GTextAlignmentLeft);
      return sz.w + 1 + vw.w;
    }
    case WIDGET_BATTERY: {
      int w = BATT_BODY_W + BATT_NUB_W;
      if (cfg->battery_pct) {
        char bs[8];
        snprintf(bs, sizeof(bs), "%d%%", s_batt_pct);
        GSize vw = graphics_text_layout_get_content_size(bs, s_font20, GRect(0, 0, 60, 22),
                                                         GTextOverflowModeFill, GTextAlignmentLeft);
        w += 3 + vw.w;
      }
      return w;
    }
    default:
      return 0;
  }
}

// Draws a single widget left-aligned starting at x, vertically centered on cy
// (ty is the text-box top for a ~22px line). Unknown/NONE types draw nothing.
static void draw_widget_at(GContext *ctx, int type, int x, int cy, int ty) {
  DemiConfig *cfg = config_get();
  switch (type) {
    case WIDGET_DATE: {
      // Weekday abbreviation (2 letters) + a calendar box with the day number.
      char da[3] = { s_day[0], s_day[1], 0 };
      GSize daw = graphics_text_layout_get_content_size(da, s_font20, GRect(0, 0, 40, 22),
                                                       GTextOverflowModeFill, GTextAlignmentLeft);
      graphics_context_set_text_color(ctx, col_fg2());
      graphics_draw_text(ctx, da, s_font20, GRect(x, ty, daw.w, 22),
                         GTextOverflowModeFill, GTextAlignmentLeft, NULL);
      x += daw.w + 5;

      char dn[4];
      snprintf(dn, sizeof(dn), "%d", s_mday);
      draw_calendar_box(ctx, x, cy, dn, themed_accent(cfg->accent_color));
      break;
    }
    case WIDGET_WEATHER: {
      if (cfg->weather_temp == WEATHER_TEMP_NONE) break;  // mirrors widget_width
      char ws[8];
      snprintf(ws, sizeof(ws), "%d°", cfg->weather_temp);
      GColor wc;
      GDrawCommandImage *wi = weather_icon(cfg->weather_condition, &wc);
      if (cfg->weather_accent) wc = themed_accent(cfg->accent_color);
      if (!wi) break;
      GSize sz = gdraw_command_image_get_bounds_size(wi);
      draw_pdc(ctx, wi, GPoint(x, cy - sz.h / 2), wc);
      x += sz.w + 1;
      GSize vw = graphics_text_layout_get_content_size(ws, s_font20, GRect(0, 0, 60, 22),
                                                       GTextOverflowModeFill, GTextAlignmentLeft);
      graphics_context_set_text_color(ctx, col_fg2());
      graphics_draw_text(ctx, ws, s_font20, GRect(x, ty, vw.w, 22),
                         GTextOverflowModeFill, GTextAlignmentLeft, NULL);
      break;
    }
    case WIDGET_HEART: {
      char hs[8];
      if (s_hr > 0) snprintf(hs, sizeof(hs), "%d", s_hr);
      else snprintf(hs, sizeof(hs), "--");
      if (!s_img_heart) break;
      GSize sz = gdraw_command_image_get_bounds_size(s_img_heart);
      draw_pdc(ctx, s_img_heart, GPoint(x, cy - sz.h / 2), GColorRed);
      x += sz.w + 1;
      GSize vw = graphics_text_layout_get_content_size(hs, s_font20, GRect(0, 0, 60, 22),
                                                       GTextOverflowModeFill, GTextAlignmentLeft);
      graphics_context_set_text_color(ctx, col_fg2());
      graphics_draw_text(ctx, hs, s_font20, GRect(x, ty, vw.w, 22),
                         GTextOverflowModeFill, GTextAlignmentLeft, NULL);
      break;
    }
    case WIDGET_SUN: {
      char ss[8];
      bool is_rise;
      // Mirrors widget_width: unknown daylight draws nothing at all.
      if (!next_sun_event(ss, sizeof(ss), &is_rise)) break;
      // The official icons are a sun over a horizon with the arrow built in,
      // so which event this is reads without a separate marker -- and without
      // being mistaken for the "sunny" weather icon, which is a bare sun.
      GDrawCommandImage *si = is_rise ? s_img_sunrise : s_img_sunset;
      if (!si) break;
      GSize sz = gdraw_command_image_get_bounds_size(si);
      draw_pdc(ctx, si, GPoint(x, cy - sz.h / 2),
               theme_is_light() ? GColorWindsorTan : GColorChromeYellow);
      x += sz.w + 1;
      GSize vw = graphics_text_layout_get_content_size(ss, s_font20, GRect(0, 0, 60, 22),
                                                       GTextOverflowModeFill, GTextAlignmentLeft);
      graphics_context_set_text_color(ctx, col_fg2());
      graphics_draw_text(ctx, ss, s_font20, GRect(x, ty, vw.w, 22),
                         GTextOverflowModeFill, GTextAlignmentLeft, NULL);
      break;
    }
    case WIDGET_BATTERY: {
      // Battery body outline + nub, filled proportionally to the charge level.
      int top = cy - BATT_H / 2;
      GColor fill = (s_batt_pct < 20) ? GColorRed : themed_accent(cfg->accent_color);
      graphics_context_set_stroke_color(ctx, col_fg2());
      graphics_draw_round_rect(ctx, GRect(x, top, BATT_BODY_W, BATT_H), 2);
      graphics_context_set_fill_color(ctx, col_fg2());
      graphics_fill_rect(ctx, GRect(x + BATT_BODY_W, cy - 3, BATT_NUB_W, 6), 0, GCornerNone);

      int inner_w = BATT_BODY_W - 4;
      int fw = inner_w * s_batt_pct / 100;
      if (fw < 1 && s_batt_pct > 0) fw = 1;  // keep a sliver visible at low %
      if (fw > inner_w) fw = inner_w;
      if (fw > 0) {
        graphics_context_set_fill_color(ctx, fill);
        graphics_fill_rect(ctx, GRect(x + 2, top + 2, fw, BATT_H - 4), 0, GCornerNone);
      }

      if (s_charging) {
        // A small lightning bolt across the body.
        int mx = x + BATT_BODY_W / 2;
        graphics_context_set_stroke_color(ctx, col_fg());
        graphics_draw_line(ctx, GPoint(mx + 2, top + 2), GPoint(mx - 2, cy));
        graphics_draw_line(ctx, GPoint(mx - 2, cy), GPoint(mx + 2, cy));
        graphics_draw_line(ctx, GPoint(mx + 2, cy), GPoint(mx - 2, top + BATT_H - 2));
      }

      // Optional percentage label beside the glyph.
      if (cfg->battery_pct) {
        char bs[8];
        snprintf(bs, sizeof(bs), "%d%%", s_batt_pct);
        int tx = x + BATT_BODY_W + BATT_NUB_W + 3;
        GSize vw = graphics_text_layout_get_content_size(bs, s_font20, GRect(0, 0, 60, 22),
                                                         GTextOverflowModeFill, GTextAlignmentLeft);
        graphics_context_set_text_color(ctx, (s_batt_pct < 20) ? GColorRed : col_fg2());
        graphics_draw_text(ctx, bs, s_font20, GRect(tx, ty, vw.w, 22),
                           GTextOverflowModeFill, GTextAlignmentLeft, NULL);
      }
      break;
    }
    default:
      break;
  }
}

// Draws the bottom widget row: three configurable slots (left / middle / right).
// Left is left-aligned, right is right-aligned, middle is centered and skipped
// if it would overlap either neighbour.
static void bottom_update_proc(Layer *layer, GContext *ctx) {
  DemiConfig *cfg = config_get();
  GRect b = layer_get_bounds(layer);
  int W = b.size.w;
  int cy = b.size.h / 2;
  int ty = cy - 11;  // text box top for a ~22px line
  const int gap = 6;

  // Top divider.
  graphics_context_set_stroke_color(ctx, col_track());
  graphics_draw_line(ctx, GPoint(0, 0), GPoint(W, 0));

  // A tap reveal takes the whole row for its few seconds rather than squeezing
  // in beside the widgets, which have no room to spare.
  if (s_revealing && cfg->tap_mode != TAP_OFF) {
    const char *text = (cfg->tap_mode == TAP_DATE) ? s_full_date : s_seconds;
    graphics_context_set_text_color(ctx, themed_accent(cfg->accent_color));
    graphics_draw_text(ctx, text, s_font20, GRect(0, ty, W, 22),
                       GTextOverflowModeFill, GTextAlignmentCenter, NULL);
    return;
  }

  // Left slot.
  int lw = widget_width(cfg->widget_left);
  if (lw > 0) draw_widget_at(ctx, cfg->widget_left, 4, cy, ty);
  int left_end = (lw > 0) ? 4 + lw : 4;

  // Right slot.
  int rw = widget_width(cfg->widget_right);
  int right_start = W - 4;
  if (rw > 0) {
    right_start = W - 4 - rw;
    draw_widget_at(ctx, cfg->widget_right, right_start, cy, ty);
  }

  // Middle slot: centered, but only if it clears both neighbours.
  int mw = widget_width(cfg->widget_mid);
  if (mw > 0) {
    int mx = (W - mw) / 2;
    if (mx >= left_end + gap && mx + mw <= right_start - gap) {
      draw_widget_at(ctx, cfg->widget_mid, mx, cy, ty);
    }
  }
}

// Draws the top status row: quiet-time mouse (left) and BT-disconnect (right).
// Both are subtle light-gray outlines; hidden during a Timeline peek slide.
static void status_update_proc(Layer *layer, GContext *ctx) {
  if (s_peek_animating) return;
  GRect b = layer_get_bounds(layer);

  if (quiet_time_is_active() && s_img_quiet) {
    draw_pdc(ctx, s_img_quiet, GPoint(4, 3), col_fg2());
  }

  if (!connection_service_peek_pebble_app_connection() && s_img_bt_off) {
    GSize sz = gdraw_command_image_get_bounds_size(s_img_bt_off);
    draw_pdc(ctx, s_img_bt_off, GPoint(b.size.w - 4 - sz.w, 3), col_fg2());
  }
}

// ---- services & updates ---------------------------------------------------

static void update_time(struct tm *tm);
static void apply_layout(GRect ub);
static void apply_tap_subscription(void);

// Config-change callback: refresh date strings (language may have changed) and redraw.
static void redraw_all(void) {
  time_t now = time(NULL);
  update_time(localtime(&now));
  // The layout mode and the icon/value toggle both move layer frames, so the
  // layout has to be recomputed — marking dirty alone would redraw stale frames.
  // Guard: this callback is registered before the window pushes its layers.
  if (s_window) {
    apply_layout(layer_get_unobstructed_bounds(window_get_root_layer(s_window)));
    // Turning taps on or off in settings takes effect straight away, rather
    // than at the next launch.
    apply_tap_subscription();
    // The theme repaints the window itself, not just the layers drawn on it.
    window_set_background_color(s_window, col_bg());
  }
  if (s_clock_layer)    layer_mark_dirty(s_clock_layer);
  if (s_progress_layer) layer_mark_dirty(s_progress_layer);
  if (s_bottom_layer)   layer_mark_dirty(s_bottom_layer);
  if (s_status_layer)   layer_mark_dirty(s_status_layer);
}

// Refreshes the cached time/date strings from the current local time.
static void update_time(struct tm *tm) {
  bool h24 = config_get()->clock_24h;
  strftime(s_hours, sizeof(s_hours), h24 ? "%H" : "%I", tm);
  if (s_hours[0] == '0') {            // strip leading zero from the hour
    memmove(s_hours, s_hours + 1, strlen(s_hours));
  }
  strftime(s_minutes, sizeof(s_minutes), "%M", tm);

  // AM/PM indicator (12h mode only); derived from tm_hour to avoid %p locale gaps.
  if (h24) {
    s_ampm[0] = 0;
  } else {
    strncpy(s_ampm, tm->tm_hour < 12 ? "AM" : "PM", sizeof(s_ampm) - 1);
    s_ampm[sizeof(s_ampm) - 1] = 0;
  }

  // Locale-independent weekday/month abbreviations per configured language
  // (ASCII only — the small font's characterRegex excludes accented glyphs).
  static const char *const WDAY[LANG_COUNT][7] = {
    { "SU", "MO", "TU", "WE", "TH", "FR", "SA" },  // EN
    { "ZO", "MA", "DI", "WO", "DO", "VR", "ZA" },  // NL
    { "DI", "LU", "MA", "ME", "JE", "VE", "SA" },  // FR
    { "SO", "MO", "DI", "MI", "DO", "FR", "SA" },  // DE
    { "DO", "LU", "MA", "MI", "JU", "VI", "SA" },  // ES
  };
  // Month abbreviations for the tap-to-reveal date, same constraints: three
  // ASCII letters, so no "FÉV" or "DÉC".
  static const char *const MON[LANG_COUNT][12] = {
    { "JAN", "FEB", "MAR", "APR", "MAY", "JUN",
      "JUL", "AUG", "SEP", "OCT", "NOV", "DEC" },  // EN
    { "JAN", "FEB", "MRT", "APR", "MEI", "JUN",
      "JUL", "AUG", "SEP", "OKT", "NOV", "DEC" },  // NL
    { "JAN", "FEV", "MAR", "AVR", "MAI", "JUN",
      "JUL", "AOU", "SEP", "OCT", "NOV", "DEC" },  // FR
    { "JAN", "FEB", "MAR", "APR", "MAI", "JUN",
      "JUL", "AUG", "SEP", "OKT", "NOV", "DEZ" },  // DE
    { "ENE", "FEB", "MAR", "ABR", "MAY", "JUN",
      "JUL", "AGO", "SEP", "OCT", "NOV", "DIC" },  // ES
  };
  int lang = config_get()->language;
  if (lang < 0 || lang >= LANG_COUNT) lang = LANG_EN;
  strncpy(s_day, WDAY[lang][tm->tm_wday % 7], sizeof(s_day) - 1);
  s_day[sizeof(s_day) - 1] = 0;
  s_mday = tm->tm_mday;

  snprintf(s_full_date, sizeof(s_full_date), "%s %d %s %d",
           WDAY[lang][tm->tm_wday % 7], tm->tm_mday, MON[lang][tm->tm_mon % 12],
           tm->tm_year + 1900);
  snprintf(s_seconds, sizeof(s_seconds), ":%02d", tm->tm_sec);
}

// Minute tick: refresh time, redraw clock + bottom row. Ticks every second
// instead while a seconds reveal is on screen.
static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_time(tick_time);
  if (s_clock_layer)  layer_mark_dirty(s_clock_layer);
  if (s_bottom_layer) layer_mark_dirty(s_bottom_layer);
  // Quiet time is schedule-driven (no event); re-check it every minute.
  if (s_status_layer) layer_mark_dirty(s_status_layer);
}

// True when a tap has something to show: the bottom row, the bars, or both.
static bool tap_reveals_anything(void) {
  DemiConfig *cfg = config_get();
  return cfg->tap_mode != TAP_OFF || cfg->tap_bars;
}

// Ends a tap reveal: drop back to minute ticks and restore the widget row.
static void reveal_end(void *context) {
  s_reveal_timer = NULL;
  s_revealing = false;
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  if (s_bottom_layer)   layer_mark_dirty(s_bottom_layer);
  if (s_progress_layer) layer_mark_dirty(s_progress_layer);
}

// Wrist tap: show the seconds or the full date over the widget row for a few
// seconds. A tap while one is already up restarts the countdown rather than
// stacking a second timer.
static void tap_handler(AccelAxisType axis, int32_t direction) {
  if (!tap_reveals_anything()) return;

  if (s_reveal_timer) {
    app_timer_reschedule(s_reveal_timer, TAP_REVEAL_MS);
  } else {
    s_reveal_timer = app_timer_register(TAP_REVEAL_MS, reveal_end, NULL);
  }

  s_revealing = true;
  // Seconds have to advance while they are on screen; the date does not, so it
  // leaves the tick alone and costs nothing extra.
  if (config_get()->tap_mode == TAP_SECONDS) {
    tick_timer_service_subscribe(SECOND_UNIT, tick_handler);
  }
  time_t now = time(NULL);
  update_time(localtime(&now));
  if (s_bottom_layer)   layer_mark_dirty(s_bottom_layer);
  if (s_progress_layer) layer_mark_dirty(s_progress_layer);
}

// Subscribes to wrist taps only when the setting asks for them, so a watch with
// the feature off never wakes for the accelerometer at all.
static void apply_tap_subscription(void) {
  if (!tap_reveals_anything()) {
    accel_tap_service_unsubscribe();
    if (s_reveal_timer) {
      app_timer_cancel(s_reveal_timer);
      reveal_end(NULL);
    }
  } else {
    accel_tap_service_subscribe(tap_handler);
  }
}

// Bluetooth/phone connection change: redraw the status row.
static void conn_handler(bool connected) {
  if (s_status_layer) layer_mark_dirty(s_status_layer);
}

// Battery change: cache state, redraw progress + bottom row.
static void battery_handler(BatteryChargeState charge) {
  s_batt_pct = charge.charge_percent;
  s_charging = charge.is_charging;
  if (s_progress_layer) layer_mark_dirty(s_progress_layer);
  if (s_bottom_layer)   layer_mark_dirty(s_bottom_layer);
}

// Health update: refresh step/calorie/distance totals, redraw progress.
static void health_handler(HealthEventType event, void *context) {
  update_health();
  if (s_progress_layer) layer_mark_dirty(s_progress_layer);
  if (s_bottom_layer)   layer_mark_dirty(s_bottom_layer);
}

// ---- window ---------------------------------------------------------------

// Positions every layer within the given (unobstructed) bounds. Called at load
// and on each Timeline Quick View frame so the layout compresses to stay visible
// above the peek. The update procs read layer_get_bounds(), so digits/widgets
// re-fit automatically.
static void apply_layout(GRect ub) {
  int W = ub.size.w, H = ub.size.h;
  int bottom_h = H * 17 / 100;
  int clock_h = H - bottom_h;

  if (s_clock_layer) layer_set_frame(s_clock_layer, GRect(0, 0, W, clock_h));

  // The progressbar overlays the clock rather than sitting between two halves,
  // so the side-by-side layout can simply claim the whole clock area: its value
  // label would clip against a narrow middle strip (layers clip to bounds).
  if (s_progress_layer) {
    bool overlay = (config_get()->layout_mode != LAYOUT_VERTICAL);
    layer_set_frame(s_progress_layer, overlay ? GRect(0, 0, W, clock_h)
                                              : GRect(0, clock_h * 50 / 100 - 14, W, 28));
  }

  if (s_bottom_layer) layer_set_frame(s_bottom_layer, GRect(0, H - bottom_h, W, bottom_h));
  if (s_status_layer) layer_set_frame(s_status_layer, GRect(0, 0, W, 28));
}

// Timeline Quick View: hide the status icons during the slide to reduce clutter.
static void unobstructed_will_change(GRect final_unobstructed, void *context) {
  s_peek_animating = true;
  if (s_status_layer) layer_mark_dirty(s_status_layer);
}

// Reposition every frame so the face slides smoothly with the peek.
static void unobstructed_change(AnimationProgress progress, void *context) {
  apply_layout(layer_get_unobstructed_bounds(window_get_root_layer(s_window)));
}

// Settle into the final layout and restore the status icons.
static void unobstructed_did_change(void *context) {
  s_peek_animating = false;
  apply_layout(layer_get_unobstructed_bounds(window_get_root_layer(s_window)));
  if (s_status_layer) layer_mark_dirty(s_status_layer);
}

// Builds fonts, icons, layers and subscribes to services.
static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);

  // Fonts.
  s_ffont_bold  = ffont_create_from_resource(RESOURCE_ID_RAJDHANI_BOLD_FFONT);
  s_ffont_light = ffont_create_from_resource(RESOURCE_ID_RAJDHANI_LIGHT_FFONT);
  s_font20      = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_RAJDHANI_BOLD_20));
  APP_LOG(APP_LOG_LEVEL_DEBUG, "fonts loaded: bold=%p light=%p f20=%p",
          s_ffont_bold, s_ffont_light, s_font20);

  // Icons.
  s_img_shoe     = gdraw_command_image_create_with_resource(RESOURCE_ID_IMG_SHOE);
  s_img_battery  = gdraw_command_image_create_with_resource(RESOURCE_ID_IMG_BATTERY);
  s_img_flame    = gdraw_command_image_create_with_resource(RESOURCE_ID_IMG_FLAME);
  s_img_runner   = gdraw_command_image_create_with_resource(RESOURCE_ID_IMG_RUNNER);
  s_img_heart    = gdraw_command_image_create_with_resource(RESOURCE_ID_IMG_HEART);
  s_img_sun      = gdraw_command_image_create_with_resource(RESOURCE_ID_IMG_SUN);
  s_img_partly   = gdraw_command_image_create_with_resource(RESOURCE_ID_IMG_PARTLY_CLOUDY);
  s_img_cloud    = gdraw_command_image_create_with_resource(RESOURCE_ID_IMG_CLOUD);
  s_img_lrain    = gdraw_command_image_create_with_resource(RESOURCE_ID_IMG_LIGHT_RAIN);
  s_img_hrain    = gdraw_command_image_create_with_resource(RESOURCE_ID_IMG_HEAVY_RAIN);
  s_img_lsnow    = gdraw_command_image_create_with_resource(RESOURCE_ID_IMG_LIGHT_SNOW);
  s_img_hsnow    = gdraw_command_image_create_with_resource(RESOURCE_ID_IMG_HEAVY_SNOW);
  s_img_quiet    = gdraw_command_image_create_with_resource(RESOURCE_ID_IMG_QUIET);
  s_img_bt_off   = gdraw_command_image_create_with_resource(RESOURCE_ID_IMG_BT_OFF);
  s_img_gauge          = gdraw_command_image_create_with_resource(RESOURCE_ID_IMG_GAUGE);
  s_img_sunrise        = gdraw_command_image_create_with_resource(RESOURCE_ID_IMG_SUNRISE);
  s_img_sunset         = gdraw_command_image_create_with_resource(RESOURCE_ID_IMG_SUNSET);
  s_img_duration       = gdraw_command_image_create_with_resource(RESOURCE_ID_IMG_DURATION);
  s_img_claude_session = gdraw_command_image_create_with_resource(RESOURCE_ID_IMG_CLAUDE_SESSION);
  s_img_claude_week    = gdraw_command_image_create_with_resource(RESOURCE_ID_IMG_CLAUDE_WEEK);

  // Layers (frames set by apply_layout below). The status layer is added last so
  // its corner icons overlay the clock.
  s_clock_layer = layer_create(GRectZero);
  layer_set_update_proc(s_clock_layer, clock_update_proc);
  layer_add_child(root, s_clock_layer);

  s_progress_layer = layer_create(GRectZero);
  layer_set_update_proc(s_progress_layer, progress_update_proc);
  layer_add_child(root, s_progress_layer);

  s_bottom_layer = layer_create(GRectZero);
  layer_set_update_proc(s_bottom_layer, bottom_update_proc);
  layer_add_child(root, s_bottom_layer);

  s_status_layer = layer_create(GRectZero);
  layer_set_update_proc(s_status_layer, status_update_proc);
  layer_add_child(root, s_status_layer);

  apply_layout(layer_get_unobstructed_bounds(root));

  // Initial data.
  time_t now = time(NULL);
  update_time(localtime(&now));
  update_health();
  battery_handler(battery_state_service_peek());

  // Services.
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  battery_state_service_subscribe(battery_handler);
  health_service_events_subscribe(health_handler, NULL);
  connection_service_subscribe((ConnectionHandlers){
    .pebble_app_connection_handler = conn_handler,
  });
  unobstructed_area_service_subscribe((UnobstructedAreaHandlers){
    .will_change = unobstructed_will_change,
    .change      = unobstructed_change,
    .did_change  = unobstructed_did_change,
  }, NULL);
  apply_tap_subscription();
}

// Tears down everything created in window_load.
static void window_unload(Window *window) {
  if (s_reveal_timer) {
    app_timer_cancel(s_reveal_timer);
    s_reveal_timer = NULL;
  }
  accel_tap_service_unsubscribe();
  tick_timer_service_unsubscribe();
  battery_state_service_unsubscribe();
  health_service_events_unsubscribe();
  connection_service_unsubscribe();
  unobstructed_area_service_unsubscribe();

  layer_destroy(s_clock_layer);
  layer_destroy(s_progress_layer);
  layer_destroy(s_bottom_layer);
  layer_destroy(s_status_layer);

  ffont_destroy(s_ffont_bold);
  ffont_destroy(s_ffont_light);
  fonts_unload_custom_font(s_font20);

  gdraw_command_image_destroy(s_img_shoe);
  gdraw_command_image_destroy(s_img_battery);
  gdraw_command_image_destroy(s_img_flame);
  gdraw_command_image_destroy(s_img_runner);
  gdraw_command_image_destroy(s_img_heart);
  gdraw_command_image_destroy(s_img_sun);
  gdraw_command_image_destroy(s_img_partly);
  gdraw_command_image_destroy(s_img_cloud);
  gdraw_command_image_destroy(s_img_lrain);
  gdraw_command_image_destroy(s_img_hrain);
  gdraw_command_image_destroy(s_img_lsnow);
  gdraw_command_image_destroy(s_img_hsnow);
  gdraw_command_image_destroy(s_img_quiet);
  gdraw_command_image_destroy(s_img_bt_off);
  gdraw_command_image_destroy(s_img_gauge);
  gdraw_command_image_destroy(s_img_sunrise);
  gdraw_command_image_destroy(s_img_sunset);
  gdraw_command_image_destroy(s_img_duration);
  gdraw_command_image_destroy(s_img_claude_session);
  gdraw_command_image_destroy(s_img_claude_week);
}

// ---- app lifecycle --------------------------------------------------------

// Logs when an inbound settings/weather message is dropped (e.g. inbox busy or
// too small) so a "settings didn't stick" symptom is visible rather than silent.
static void inbox_dropped(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_WARNING, "AppMessage inbox dropped: %d", (int)reason);
}

// Loads config, builds the window and opens the AppMessage inbox.
static void init(void) {
  config_load();
  config_set_change_callback(redraw_all);

  s_window = window_create();
  window_set_background_color(s_window, col_bg());
  window_set_window_handlers(s_window, (WindowHandlers){
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);

  app_message_register_inbox_received(config_inbox_received);
  app_message_register_inbox_dropped(inbox_dropped);
  // 256 was already close to the limit for the settings dict Clay sends; the
  // goals, units and tap mode added here would push a full save past it, and
  // an over-large dict is dropped silently apart from inbox_dropped's log.
  app_message_open(512, 64);
}

// Destroys the window.
static void deinit(void) {
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
