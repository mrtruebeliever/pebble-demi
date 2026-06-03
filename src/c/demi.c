#include <pebble.h>
#include <pebble-fctx/fctx.h>
#include <pebble-fctx/ffont.h>
#include "config.h"

static Window *s_window;
static Layer  *s_clock_layer;
static Layer  *s_progress_layer;
static Layer  *s_bottom_layer;

static FFont  *s_ffont_bold;   // big hours
static FFont  *s_ffont_light;  // big minutes
static GFont   s_font20;       // small labels / values

// Cached PDC icons (recolored at draw time).
static GDrawCommandImage *s_img_shoe, *s_img_battery, *s_img_flame, *s_img_runner, *s_img_heart;
static GDrawCommandImage *s_img_sun, *s_img_partly, *s_img_cloud;
static GDrawCommandImage *s_img_lrain, *s_img_hrain, *s_img_lsnow, *s_img_hsnow;

// Time / date text.
static char s_hours[4];
static char s_minutes[4];
static char s_day[8];
static char s_mon[8];
static int  s_mday;

// Health + battery snapshot.
static int  s_steps    = 0;
static int  s_kcal     = 0;
static int  s_dist_m   = 0;
static int  s_batt_pct = 0;
static bool s_charging = false;
static int  s_hr       = 0;

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

// Computes the active progress metric: percentage, label, icon and fill color.
static void compute_progress(int *pct, char *buf, size_t len,
                             GDrawCommandImage **icon, GColor *fill) {
  DemiConfig *cfg = config_get();
  *fill = cfg->accent_color;
  switch (cfg->progress_type) {
    case PROGRESS_BATTERY:
      *pct = s_batt_pct;
      snprintf(buf, len, "%d%%", s_batt_pct);
      *icon = s_img_battery;
      if (s_batt_pct < 20) *fill = GColorRed;
      break;
    case PROGRESS_CALORIES:
      *pct = s_kcal > 600 ? 100 : s_kcal * 100 / 600;
      snprintf(buf, len, "%d", s_kcal);
      *icon = s_img_flame;
      break;
    case PROGRESS_DISTANCE:
      *pct = s_dist_m > 5000 ? 100 : s_dist_m * 100 / 5000;
      format_k(s_dist_m, buf, len);
      *icon = s_img_runner;
      break;
    case PROGRESS_STEPS:
    default:
      *pct = s_steps > 10000 ? 100 : s_steps * 100 / 10000;
      format_k(s_steps, buf, len);
      *icon = s_img_shoe;
      break;
  }
}

// ---- drawing --------------------------------------------------------------

// Draws the big hours (white, bold, top) and minutes (gray, light, bottom).
// Maps the configured clock scheme to its hour and minute fill colors.
static void clock_scheme_colors(int scheme, GColor *hours, GColor *minutes) {
  switch (scheme) {
    case CLOCK_SCHEME_WHITE_WHITE: *hours = GColorWhite;     *minutes = GColorWhite;     break;
    case CLOCK_SCHEME_WHITE_LIGHT: *hours = GColorWhite;     *minutes = GColorLightGray; break;
    case CLOCK_SCHEME_LIGHT_WHITE: *hours = GColorLightGray; *minutes = GColorWhite;     break;
    case CLOCK_SCHEME_WHITE_GRAY:
    default:                       *hours = GColorWhite;     *minutes = GColorDarkGray;  break;
  }
}

static void clock_update_proc(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  int W = b.size.w, H = b.size.h;

  GColor hour_color, minute_color;
  clock_scheme_colors(config_get()->clock_scheme, &hour_color, &minute_color);

  FContext fctx;
  fctx_init_context(&fctx, ctx);

  // Anchor on the digit cap-height so the placement is predictable (digits
  // have no descenders): hours high in the top third, minutes in the lower third.
  fctx_begin_fill(&fctx);
  fctx_set_fill_color(&fctx, hour_color);
  fctx_set_offset(&fctx, FPointI(W / 2, H * 21 / 100));
  fctx_set_text_em_height(&fctx, s_ffont_bold, H * 49 / 100);
  fctx_draw_string(&fctx, s_hours, s_ffont_bold, GTextAlignmentCenter, FTextAnchorCapMiddle);
  fctx_end_fill(&fctx);

  fctx_begin_fill(&fctx);
  fctx_set_fill_color(&fctx, minute_color);
  fctx_set_offset(&fctx, FPointI(W / 2, H * 73 / 100));
  fctx_set_text_em_height(&fctx, s_ffont_light, H * 49 / 100);
  fctx_draw_string(&fctx, s_minutes, s_ffont_light, GTextAlignmentCenter, FTextAnchorCapMiddle);
  fctx_end_fill(&fctx);

  fctx_deinit_context(&fctx);
}

// Draws the progressbar: icon, track, accent fill and the value label.
static void progress_update_proc(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  int cy = b.size.h / 2;

  int pct;
  char val[8];
  GDrawCommandImage *icon = NULL;
  GColor fill;
  compute_progress(&pct, val, sizeof(val), &icon, &fill);

  // Centered bar: equal reserve on both sides so the track midpoint = W/2.
  int side = 48;
  int track_x = side;
  int track_right = b.size.w - side;
  int track_w = track_right - track_x;
  if (track_w < 0) track_w = 0;

  // Icon (left edge, vertically centered) as an accent outline, matching the
  // line-art style of the bottom-bar icons.
  if (icon) {
    GSize sz = gdraw_command_image_get_bounds_size(icon);
    draw_pdc(ctx, icon, GPoint(4, cy - sz.h / 2), config_get()->accent_color);
  }

  graphics_context_set_fill_color(ctx, GColorDarkGray);
  graphics_fill_rect(ctx, GRect(track_x, cy - 3, track_w, 6), 3, GCornersAll);

  graphics_context_set_fill_color(ctx, fill);
  graphics_fill_rect(ctx, GRect(track_x, cy - 3, track_w * pct / 100, 6), 3, GCornersAll);

  // Value label (right aligned to the right edge, mirroring the left icon).
  graphics_context_set_text_color(ctx, config_get()->accent_color);
  graphics_draw_text(ctx, val, s_font20,
                     GRect(track_right, cy - 11, side - 4, 22),
                     GTextOverflowModeFill, GTextAlignmentRight, NULL);
}

// Selects the icon and color for a weather condition code.
static GDrawCommandImage *weather_icon(int cond, GColor *color) {
  switch (cond) {
    case WEATHER_SUN:        *color = GColorChromeYellow; return s_img_sun;
    case WEATHER_PARTLY:     *color = GColorPictonBlue;   return s_img_partly;
    case WEATHER_CLOUD:      *color = GColorPictonBlue;   return s_img_cloud;
    case WEATHER_LIGHT_RAIN: *color = GColorPictonBlue;   return s_img_lrain;
    case WEATHER_HEAVY_RAIN: *color = GColorPictonBlue;   return s_img_hrain;
    case WEATHER_LIGHT_SNOW: *color = GColorCeleste;      return s_img_lsnow;
    case WEATHER_HEAVY_SNOW: *color = GColorCeleste;      return s_img_hsnow;
    default:                 *color = GColorPictonBlue;   return s_img_cloud;
  }
}

// Draws a right-aligned "[icon] value" widget ending at *xr and advancing *xr
// left. Skips drawing (and leaves *xr unchanged) if it would cross `floor`,
// so the right cluster never overlaps the date widget on the left.
static void draw_right_widget(GContext *ctx, GDrawCommandImage *icon, GColor icon_color,
                              const char *value, GColor text_color, int *xr, int floor,
                              int cy, int ty) {
  GSize vw = graphics_text_layout_get_content_size(value, s_font20, GRect(0, 0, 60, 22),
                                                   GTextOverflowModeFill, GTextAlignmentRight);
  int iw = 0;
  GSize sz = GSizeZero;
  if (icon) {
    sz = gdraw_command_image_get_bounds_size(icon);
    iw = sz.w + 1;
  }
  int needed = vw.w + iw;
  if (*xr - needed < floor) {
    return;  // not enough room; skip rather than overlap the date
  }
  *xr -= vw.w;
  graphics_context_set_text_color(ctx, text_color);
  graphics_draw_text(ctx, value, s_font20, GRect(*xr, ty, vw.w, 22),
                     GTextOverflowModeFill, GTextAlignmentRight, NULL);
  if (icon) {
    *xr -= sz.w + 1;
    draw_pdc(ctx, icon, GPoint(*xr, cy - sz.h / 2), icon_color);
  }
  *xr -= 4;
}

// Draws the bottom widget row: date (left), heart/weather/battery (right).
static void bottom_update_proc(Layer *layer, GContext *ctx) {
  DemiConfig *cfg = config_get();
  GRect b = layer_get_bounds(layer);
  int W = b.size.w;
  int cy = b.size.h / 2;
  int ty = cy - 11;  // text box top for a ~22px line

  // Top divider.
  graphics_context_set_stroke_color(ctx, GColorDarkGray);
  graphics_draw_line(ctx, GPoint(0, 0), GPoint(W, 0));

  // Left: date widget -> "MA" [bigger calendar with day number]. (Month dropped.)
  int dx = 4;  // right edge of the date widget, used as collision floor
  if (cfg->show_date) {
    GFont num = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
    int x = 4;

    // Weekday abbreviation (2 letters).
    char da[3] = { s_day[0], s_day[1], 0 };
    GSize daw = graphics_text_layout_get_content_size(da, s_font20, GRect(0, 0, 40, 22),
                                                     GTextOverflowModeFill, GTextAlignmentLeft);
    graphics_context_set_text_color(ctx, GColorLightGray);
    graphics_draw_text(ctx, da, s_font20, GRect(x, ty, daw.w, 22),
                       GTextOverflowModeFill, GTextAlignmentLeft, NULL);
    x += daw.w + 5;

    // Calendar icon (bigger): two rings, header strip, outlined body, day number.
    int cw = 20, ch = 18;
    int box_top = cy - ch / 2;
    graphics_context_set_stroke_color(ctx, cfg->accent_color);
    graphics_context_set_fill_color(ctx, cfg->accent_color);
    graphics_draw_line(ctx, GPoint(x + 5, box_top - 3), GPoint(x + 5, box_top));
    graphics_draw_line(ctx, GPoint(x + cw - 5, box_top - 3), GPoint(x + cw - 5, box_top));
    graphics_draw_round_rect(ctx, GRect(x, box_top, cw, ch), 3);
    graphics_fill_rect(ctx, GRect(x + 1, box_top + 1, cw - 2, 3), 0, GCornerNone);
    char dn[4];
    snprintf(dn, sizeof(dn), "%d", s_mday);
    graphics_context_set_text_color(ctx, GColorWhite);
    // GOTHIC has top padding; pull the number up so it sits centered in the body.
    graphics_draw_text(ctx, dn, num, GRect(x, box_top - 2, cw, ch),
                       GTextOverflowModeFill, GTextAlignmentCenter, NULL);
    x += cw + 4;
    dx = x;
  }

  // Right side builds from the right edge inward: battery, weather, heart.
  // Each widget is skipped (not overlapped) if it would cross the date edge.
  int xr = W - 4;

  if (cfg->show_battery) {
    char bs[8];
    snprintf(bs, sizeof(bs), "%d%%", s_batt_pct);
    draw_right_widget(ctx, NULL, GColorClear, bs,
                      s_batt_pct < 20 ? GColorRed : GColorLightGray, &xr, dx, cy, ty);
  }

  if (cfg->show_weather) {
    char ws[8];
    if (cfg->weather_temp == WEATHER_TEMP_NONE) snprintf(ws, sizeof(ws), "--");
    else snprintf(ws, sizeof(ws), "%d°", cfg->weather_temp);
    GColor wc;
    GDrawCommandImage *wi = weather_icon(cfg->weather_condition, &wc);
    draw_right_widget(ctx, wi, wc, ws, GColorLightGray, &xr, dx, cy, ty);
  }

  if (cfg->show_heart) {
    char hs[8];
    if (s_hr > 0) snprintf(hs, sizeof(hs), "%d", s_hr);
    else snprintf(hs, sizeof(hs), "--");
    draw_right_widget(ctx, s_img_heart, GColorRed, hs, GColorLightGray, &xr, dx, cy, ty);
  }
}

// ---- services & updates ---------------------------------------------------

static void update_time(struct tm *tm);

// Config-change callback: refresh date strings (language may have changed) and redraw.
static void redraw_all(void) {
  time_t now = time(NULL);
  update_time(localtime(&now));
  if (s_clock_layer)    layer_mark_dirty(s_clock_layer);
  if (s_progress_layer) layer_mark_dirty(s_progress_layer);
  if (s_bottom_layer)   layer_mark_dirty(s_bottom_layer);
}

// Refreshes the cached time/date strings from the current local time.
static void update_time(struct tm *tm) {
  strftime(s_hours, sizeof(s_hours), "%I", tm);
  if (s_hours[0] == '0') {            // strip leading zero from the hour
    memmove(s_hours, s_hours + 1, strlen(s_hours));
  }
  strftime(s_minutes, sizeof(s_minutes), "%M", tm);

  // Locale-independent weekday/month abbreviations per configured language
  // (ASCII only — the small font's characterRegex excludes accented glyphs).
  static const char *const WDAY[LANG_COUNT][7] = {
    { "ZO", "MA", "DI", "WO", "DO", "VR", "ZA" },  // NL
    { "SU", "MO", "TU", "WE", "TH", "FR", "SA" },  // EN
    { "SO", "MO", "DI", "MI", "DO", "FR", "SA" },  // DE
    { "DI", "LU", "MA", "ME", "JE", "VE", "SA" },  // FR
  };
  static const char *const MON[LANG_COUNT][12] = {
    { "JAN","FEB","MRT","APR","MEI","JUN","JUL","AUG","SEP","OKT","NOV","DEC" },  // NL
    { "JAN","FEB","MAR","APR","MAY","JUN","JUL","AUG","SEP","OCT","NOV","DEC" },  // EN
    { "JAN","FEB","MRZ","APR","MAI","JUN","JUL","AUG","SEP","OKT","NOV","DEZ" },  // DE
    { "JAN","FEV","MAR","AVR","MAI","JUN","JUL","AOU","SEP","OCT","NOV","DEC" },  // FR
  };
  int lang = config_get()->language;
  if (lang < 0 || lang >= LANG_COUNT) lang = LANG_NL;
  strncpy(s_day, WDAY[lang][tm->tm_wday % 7], sizeof(s_day) - 1);
  s_day[sizeof(s_day) - 1] = 0;
  strncpy(s_mon, MON[lang][tm->tm_mon % 12], sizeof(s_mon) - 1);
  s_mon[sizeof(s_mon) - 1] = 0;
  s_mday = tm->tm_mday;
}

// Minute tick: refresh time, redraw clock + bottom row.
static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_time(tick_time);
  if (s_clock_layer)  layer_mark_dirty(s_clock_layer);
  if (s_bottom_layer) layer_mark_dirty(s_bottom_layer);
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

// Builds fonts, icons, layers and subscribes to services.
static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect b = layer_get_unobstructed_bounds(root);
  int W = b.size.w, H = b.size.h;
  int bottom_h = H * 17 / 100;
  int clock_h = H - bottom_h;

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

  // Layers.
  s_clock_layer = layer_create(GRect(0, 0, W, clock_h));
  layer_set_update_proc(s_clock_layer, clock_update_proc);
  layer_add_child(root, s_clock_layer);

  s_progress_layer = layer_create(GRect(0, clock_h * 47 / 100 - 14, W, 28));
  layer_set_update_proc(s_progress_layer, progress_update_proc);
  layer_add_child(root, s_progress_layer);

  s_bottom_layer = layer_create(GRect(0, H - bottom_h, W, bottom_h));
  layer_set_update_proc(s_bottom_layer, bottom_update_proc);
  layer_add_child(root, s_bottom_layer);

  // Initial data.
  time_t now = time(NULL);
  update_time(localtime(&now));
  update_health();
  battery_handler(battery_state_service_peek());

  // Services.
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  battery_state_service_subscribe(battery_handler);
  health_service_events_subscribe(health_handler, NULL);
}

// Tears down everything created in window_load.
static void window_unload(Window *window) {
  tick_timer_service_unsubscribe();
  battery_state_service_unsubscribe();
  health_service_events_unsubscribe();

  layer_destroy(s_clock_layer);
  layer_destroy(s_progress_layer);
  layer_destroy(s_bottom_layer);

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
}

// ---- app lifecycle --------------------------------------------------------

// Loads config, builds the window and opens the AppMessage inbox.
static void init(void) {
  config_load();
  config_set_change_callback(redraw_all);

  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
  window_set_window_handlers(s_window, (WindowHandlers){
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);

  app_message_register_inbox_received(config_inbox_received);
  app_message_open(256, 64);
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
