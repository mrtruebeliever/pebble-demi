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

// Hidden during a Timeline Quick View slide to reduce clutter.
static bool s_peek_animating = false;

// Time / date text.
static char s_hours[4];
static char s_minutes[4];
static char s_ampm[4];   // "AM"/"PM" in 12h mode; empty in 24h mode
static char s_day[8];
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
  GColor accent = config_get()->accent_color;
  switch (scheme) {
    case CLOCK_SCHEME_WHITE_WHITE:  *hours = GColorWhite;     *minutes = GColorWhite;     break;
    case CLOCK_SCHEME_WHITE_LIGHT:  *hours = GColorWhite;     *minutes = GColorLightGray; break;
    case CLOCK_SCHEME_LIGHT_WHITE:  *hours = GColorLightGray; *minutes = GColorWhite;     break;
    case CLOCK_SCHEME_ACCENT_WHITE: *hours = accent;          *minutes = GColorWhite;     break;
    case CLOCK_SCHEME_WHITE_ACCENT: *hours = GColorWhite;     *minutes = accent;          break;
    case CLOCK_SCHEME_ACCENT_GRAY:  *hours = accent;          *minutes = GColorDarkGray;  break;
    case CLOCK_SCHEME_ACCENT_LIGHT: *hours = accent;          *minutes = GColorLightGray; break;
    case CLOCK_SCHEME_WHITE_GRAY:
    default:                        *hours = GColorWhite;     *minutes = GColorDarkGray;  break;
  }
}

// Horizontal layout: pixels reserved down the middle for the vertical bar.
// Both the clock and the progressbar derive their geometry from this.
static int hbar_reserve(void) { return config_get()->progress_info ? 48 : 20; }

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

// Side-by-side digits, vertically centered, each fitted to its own column
// beside the vertical progressbar.
static void clock_draw_side_by_side(FContext *fctx, int W, int H, GColor hour_color,
                                    GColor minute_color, GRect *ampm, GTextAlignment *ampm_align) {
  int col_w = (W - hbar_reserve()) / 2;
  int max_w = col_w - 8;
  int cy = H / 2;
  int em = H * 40 / 100;

  fctx_begin_fill(fctx);
  fctx_set_fill_color(fctx, hour_color);
  fctx_set_offset(fctx, FPointI(col_w / 2, cy));
  int hour_em = fit_em(fctx, s_ffont_bold, s_hours, max_w, em);
  fctx_draw_string(fctx, s_hours, s_ffont_bold, GTextAlignmentCenter, FTextAnchorCapMiddle);
  fctx_end_fill(fctx);

  fctx_begin_fill(fctx);
  fctx_set_fill_color(fctx, minute_color);
  fctx_set_offset(fctx, FPointI(W - col_w / 2, cy));
  fit_em(fctx, s_ffont_light, s_minutes, max_w, em);
  fctx_draw_string(fctx, s_minutes, s_ffont_light, GTextAlignmentCenter, FTextAnchorCapMiddle);
  fctx_end_fill(fctx);

  // Centered under the hour digits, where the stacked layout has no room.
  *ampm = GRect(0, cy + hour_em / 2 + 4, col_w, 22);
  *ampm_align = GTextAlignmentCenter;
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
  if (config_get()->layout_mode == LAYOUT_HORIZONTAL) {
    clock_draw_side_by_side(&fctx, W, H, hour_color, minute_color, &ampm, &ampm_align);
  } else {
    clock_draw_stacked(&fctx, W, H, hour_color, minute_color, &ampm, &ampm_align);
  }
  fctx_deinit_context(&fctx);

  // AM/PM indicator (12h mode): raster font, so it waits until FCTX is done.
  if (s_ampm[0] && ampm.size.w > 0) {
    graphics_context_set_text_color(ctx, GColorLightGray);
    graphics_draw_text(ctx, s_ampm, s_font20, ampm, GTextOverflowModeFill, ampm_align, NULL);
  }
}

// Stacked layout: horizontal bar, icon left of the track and value right of it.
static void draw_bar_horizontal(GContext *ctx, GRect b, int pct, const char *val,
                                GDrawCommandImage *icon, GColor fill) {
  DemiConfig *cfg = config_get();
  int cy = b.size.h / 2;

  // Equal reserve on both sides keeps the track midpoint at W/2. Hiding the
  // icon and value hands most of their reserve back to the track, keeping a
  // margin so the bar stays inset from the bezel.
  int side = cfg->progress_info ? 48 : 12;
  int track_x = side;
  int track_right = b.size.w - side;
  int track_w = track_right - track_x;
  if (track_w < 0) track_w = 0;

  graphics_context_set_fill_color(ctx, GColorDarkGray);
  graphics_fill_rect(ctx, GRect(track_x, cy - 3, track_w, 6), 3, GCornersAll);

  graphics_context_set_fill_color(ctx, fill);
  graphics_fill_rect(ctx, GRect(track_x, cy - 3, track_w * pct / 100, 6), 3, GCornersAll);

  if (!cfg->progress_info) return;

  // Icon as an accent outline, matching the line-art style of the bottom bar.
  // Swapped, the icon takes the right edge and the value the left.
  if (icon) {
    GSize sz = gdraw_command_image_get_bounds_size(icon);
    int ix = cfg->progress_swap ? b.size.w - 4 - sz.w : 4;
    draw_pdc(ctx, icon, GPoint(ix, cy - sz.h / 2), cfg->accent_color);
  }

  // The value hugs the track on whichever side it lands, so it aligns away
  // from its own edge.
  graphics_context_set_text_color(ctx, cfg->accent_color);
  graphics_draw_text(ctx, val, s_font20,
                     GRect(cfg->progress_swap ? 4 : track_right, cy - 11, side - 4, 22),
                     GTextOverflowModeFill,
                     cfg->progress_swap ? GTextAlignmentLeft : GTextAlignmentRight, NULL);
}

// Side-by-side layout: vertical bar splitting the digits, icon above the track
// and value below it — the stacked layout's arrangement rotated a quarter turn.
static void draw_bar_vertical(GContext *ctx, GRect b, int pct, const char *val,
                              GDrawCommandImage *icon, GColor fill) {
  DemiConfig *cfg = config_get();
  int cx = b.size.w / 2;

  // Room above for the icon and below for the value, symmetric so the track
  // stays centered on the digits' midline. Hiding both keeps a small margin
  // rather than running the bar to the very edge.
  int side = cfg->progress_info ? 34 : 12;
  int track_h = b.size.h - 2 * side;
  if (track_h < 0) track_h = 0;

  graphics_context_set_fill_color(ctx, GColorDarkGray);
  graphics_fill_rect(ctx, GRect(cx - 3, side, 6, track_h), 3, GCornersAll);

  // Fills top-down, mirroring the left-to-right fill of the horizontal bar.
  graphics_context_set_fill_color(ctx, fill);
  graphics_fill_rect(ctx, GRect(cx - 3, side, 6, track_h * pct / 100), 3, GCornersAll);

  if (!cfg->progress_info) return;

  // Swapped, the icon drops below the track and the value rises above it.
  if (icon) {
    GSize sz = gdraw_command_image_get_bounds_size(icon);
    int iy = cfg->progress_swap ? b.size.h - 4 - sz.h : 4;
    draw_pdc(ctx, icon, GPoint(cx - sz.w / 2, iy), cfg->accent_color);
  }

  graphics_context_set_text_color(ctx, cfg->accent_color);
  graphics_draw_text(ctx, val, s_font20,
                     GRect(cx - 30, cfg->progress_swap ? 4 : b.size.h - side + 4, 60, 22),
                     GTextOverflowModeFill, GTextAlignmentCenter, NULL);
}

// Draws the progressbar: icon, track, accent fill and the value label.
static void progress_update_proc(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);

  int pct;
  char val[8];
  GDrawCommandImage *icon = NULL;
  GColor fill;
  compute_progress(&pct, val, sizeof(val), &icon, &fill);

  if (config_get()->layout_mode == LAYOUT_HORIZONTAL) {
    draw_bar_vertical(ctx, b, pct, val, icon, fill);
  } else {
    draw_bar_horizontal(ctx, b, pct, val, icon, fill);
  }
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

// Battery glyph geometry (a small body rect plus a positive-terminal nub).
#define BATT_BODY_W 22
#define BATT_H      12
#define BATT_NUB_W  2

// Returns the pixel width a widget needs, or 0 for WIDGET_NONE / no content.
// Mirrors the layout each draw_widget_* path produces so slots can be placed.
static int widget_width(int type) {
  DemiConfig *cfg = config_get();
  switch (type) {
    case WIDGET_DATE: {
      char da[3] = { s_day[0], s_day[1], 0 };
      GSize daw = graphics_text_layout_get_content_size(da, s_font20, GRect(0, 0, 40, 22),
                                                       GTextOverflowModeFill, GTextAlignmentLeft);
      return daw.w + 5 + 20;  // weekday + gap + calendar box
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
      GFont num = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
      char da[3] = { s_day[0], s_day[1], 0 };
      GSize daw = graphics_text_layout_get_content_size(da, s_font20, GRect(0, 0, 40, 22),
                                                       GTextOverflowModeFill, GTextAlignmentLeft);
      graphics_context_set_text_color(ctx, GColorLightGray);
      graphics_draw_text(ctx, da, s_font20, GRect(x, ty, daw.w, 22),
                         GTextOverflowModeFill, GTextAlignmentLeft, NULL);
      x += daw.w + 5;

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
      break;
    }
    case WIDGET_WEATHER: {
      if (cfg->weather_temp == WEATHER_TEMP_NONE) break;  // mirrors widget_width
      char ws[8];
      snprintf(ws, sizeof(ws), "%d°", cfg->weather_temp);
      GColor wc;
      GDrawCommandImage *wi = weather_icon(cfg->weather_condition, &wc);
      if (cfg->weather_accent) wc = cfg->accent_color;
      if (!wi) break;
      GSize sz = gdraw_command_image_get_bounds_size(wi);
      draw_pdc(ctx, wi, GPoint(x, cy - sz.h / 2), wc);
      x += sz.w + 1;
      GSize vw = graphics_text_layout_get_content_size(ws, s_font20, GRect(0, 0, 60, 22),
                                                       GTextOverflowModeFill, GTextAlignmentLeft);
      graphics_context_set_text_color(ctx, GColorLightGray);
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
      graphics_context_set_text_color(ctx, GColorLightGray);
      graphics_draw_text(ctx, hs, s_font20, GRect(x, ty, vw.w, 22),
                         GTextOverflowModeFill, GTextAlignmentLeft, NULL);
      break;
    }
    case WIDGET_BATTERY: {
      // Battery body outline + nub, filled proportionally to the charge level.
      int top = cy - BATT_H / 2;
      GColor fill = (s_batt_pct < 20) ? GColorRed : cfg->accent_color;
      graphics_context_set_stroke_color(ctx, GColorLightGray);
      graphics_draw_round_rect(ctx, GRect(x, top, BATT_BODY_W, BATT_H), 2);
      graphics_context_set_fill_color(ctx, GColorLightGray);
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
        graphics_context_set_stroke_color(ctx, GColorWhite);
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
        graphics_context_set_text_color(ctx, (s_batt_pct < 20) ? GColorRed : GColorLightGray);
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
  graphics_context_set_stroke_color(ctx, GColorDarkGray);
  graphics_draw_line(ctx, GPoint(0, 0), GPoint(W, 0));

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
    draw_pdc(ctx, s_img_quiet, GPoint(4, 3), GColorLightGray);
  }

  if (!connection_service_peek_pebble_app_connection() && s_img_bt_off) {
    GSize sz = gdraw_command_image_get_bounds_size(s_img_bt_off);
    draw_pdc(ctx, s_img_bt_off, GPoint(b.size.w - 4 - sz.w, 3), GColorLightGray);
  }
}

// ---- services & updates ---------------------------------------------------

static void update_time(struct tm *tm);
static void apply_layout(GRect ub);

// Config-change callback: refresh date strings (language may have changed) and redraw.
static void redraw_all(void) {
  time_t now = time(NULL);
  update_time(localtime(&now));
  // The layout mode and the icon/value toggle both move layer frames, so the
  // layout has to be recomputed — marking dirty alone would redraw stale frames.
  // Guard: this callback is registered before the window pushes its layers.
  if (s_window) {
    apply_layout(layer_get_unobstructed_bounds(window_get_root_layer(s_window)));
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
    { "ZO", "MA", "DI", "WO", "DO", "VR", "ZA" },  // NL
    { "SU", "MO", "TU", "WE", "TH", "FR", "SA" },  // EN
    { "SO", "MO", "DI", "MI", "DO", "FR", "SA" },  // DE
    { "DI", "LU", "MA", "ME", "JE", "VE", "SA" },  // FR
  };
  int lang = config_get()->language;
  if (lang < 0 || lang >= LANG_COUNT) lang = LANG_NL;
  strncpy(s_day, WDAY[lang][tm->tm_wday % 7], sizeof(s_day) - 1);
  s_day[sizeof(s_day) - 1] = 0;
  s_mday = tm->tm_mday;
}

// Minute tick: refresh time, redraw clock + bottom row.
static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_time(tick_time);
  if (s_clock_layer)  layer_mark_dirty(s_clock_layer);
  if (s_bottom_layer) layer_mark_dirty(s_bottom_layer);
  // Quiet time is schedule-driven (no event); re-check it every minute.
  if (s_status_layer) layer_mark_dirty(s_status_layer);
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
    bool horizontal = (config_get()->layout_mode == LAYOUT_HORIZONTAL);
    layer_set_frame(s_progress_layer, horizontal ? GRect(0, 0, W, clock_h)
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
}

// Tears down everything created in window_load.
static void window_unload(Window *window) {
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
  window_set_background_color(s_window, GColorBlack);
  window_set_window_handlers(s_window, (WindowHandlers){
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);

  app_message_register_inbox_received(config_inbox_received);
  app_message_register_inbox_dropped(inbox_dropped);
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
