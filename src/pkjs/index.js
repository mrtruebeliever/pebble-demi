var Clay = require('pebble-clay');
var clayConfig = require('./config.json');
var configI18n = require('./config_i18n');
var keys = require('message_keys');

// Language indices, matching LANG_* in config.h. LANG_AUTO (255) is not
// listed: it is simply anything outside this range, which is how settingsLang()
// treats a missing value too.
var LANG_EN = 0, LANG_NL = 1, LANG_FR = 2, LANG_DE = 3, LANG_ES = 4;

// Maps a locale tag ("nl-BE", "fr_FR") to a language index, in the same order
// as detect_locale_lang() on the watch. English is the fallback.
function langFromLocale(locale) {
  var p = String(locale || '').slice(0, 2).toLowerCase();
  if (p === 'nl') return LANG_NL;
  if (p === 'fr') return LANG_FR;
  if (p === 'de') return LANG_DE;
  if (p === 'es') return LANG_ES;
  return LANG_EN;
}

// The settings page follows the language the user picked for the watch. On
// LANG_AUTO the watch reads its own locale, which the phone cannot see, so the
// page uses the phone's locale instead — the closest thing available, and in
// practice the same language.
function settingsLang() {
  var v = parseInt(localStorage.getItem('LANG'), 10);
  if (v >= LANG_EN && v <= LANG_ES) { return v; }
  // LANG_AUTO, or a first run with nothing stored.
  return langFromLocale(navigator.language || navigator.userLanguage);
}

// The distance goal has two fields — kilometres and miles — and only the one
// matching the unit setting is shown. Clay cannot relabel a field at runtime,
// and the page has no way to know what "Automatic" will resolve to on the
// watch, so Automatic shows the kilometre field.
//
// This runs inside the settings webview rather than here: Clay serialises the
// function across, so it must not close over anything in this file.
function customClay() {
  var page = this;

  page.on(page.EVENTS.AFTER_BUILD, function () {
    var unit = page.getItemByMessageKey('DIST_UNIT');
    var km = page.getItemByMessageKey('GOAL_DIST_KM');
    var mi = page.getItemByMessageKey('GOAL_DIST_MI');
    if (!unit || !km || !mi) { return; }

    function sync() {
      if (String(unit.get()) === '1') {   // DIST_MILES
        km.hide();
        mi.show();
      } else {
        mi.hide();
        km.show();
      }
    }

    unit.on('change', sync);
    sync();
  });
}

var clay = new Clay(configI18n.buildConfig(clayConfig, settingsLang()), customClay);

var WEATHER_REFRESH_MS = 30 * 60 * 1000;  // 30 minutes
var CUSTOM_REFRESH_MS = 3 * 60 * 1000;    // 3 minutes — below the gist CDN's own 5-min cache floor, so shorter gains little

// Distance unit ids, matching DIST_* in config.h. DIST_AUTO means the watch
// decides from its own measurement system, in which case the page shows the
// kilometre field — the phone cannot know what the watch will resolve to.
var DIST_MILES = 1;

// Custom-metric icon ids, matching CUSTOM_ICON_* in config.h.
var CUSTOM_ICON_GAUGE = 0;
var CUSTOM_ICON_CLAUDE_SESSION = 1;
var CUSTOM_ICON_CLAUDE_WEEK = 2;

// Picks an icon for a custom-metric item from its "name" field: recognized
// Claude Code usage names get their matching icon, anything else falls back
// to a generic gauge.
function customIconFor(name) {
  var n = String(name || '').toLowerCase();
  if (n.indexOf('session') !== -1) return CUSTOM_ICON_CLAUDE_SESSION;
  if (n.indexOf('week') !== -1) return CUSTOM_ICON_CLAUDE_WEEK;
  return CUSTOM_ICON_GAUGE;
}

// Clamps a value to the 0-100 range a progressbar can render; anything
// unparseable is dropped so a malformed item doesn't paint a bogus bar.
function clampPct(v) {
  var n = Math.round(Number(v));
  if (isNaN(n)) return null;
  return Math.max(0, Math.min(100, n));
}

// Maps an Open-Meteo WMO weather code to our condition:
// 0 sun, 1 partly cloudy, 2 cloudy, 3 light rain, 4 heavy rain, 5 light snow, 6 heavy snow.
function condFromWMO(code) {
  if (code === 0) return 0;                                   // clear
  if (code === 1 || code === 2) return 1;                     // mainly clear / partly cloudy
  if (code === 3 || code === 45 || code === 48) return 2;     // overcast / fog
  if ((code >= 51 && code <= 61) || code === 80) return 3;    // drizzle / slight rain / showers
  if ((code >= 63 && code <= 67) || code === 81 || code === 82 || code >= 95) return 4; // rain / storm
  if (code === 71 || code === 73 || code === 85) return 5;    // slight/moderate snow
  if (code === 75 || code === 77 || code === 86) return 6;    // heavy snow / grains
  return 2;                                                   // fallback: cloudy
}

// Returns the Open-Meteo temperature_unit based on the saved TEMP_UNIT setting.
function tempUnit() {
  return localStorage.getItem('TEMP_UNIT') === '1' ? 'fahrenheit' : 'celsius';
}

// Parses an Open-Meteo local time ("2026-08-13T06:23") to minutes since local
// midnight, which is all the watch needs and survives an AppMessage as one
// small int. Returns null if the string isn't in that shape.
function minutesFromISO(iso) {
  var m = /T(\d{2}):(\d{2})/.exec(String(iso || ''));
  if (!m) return null;
  return parseInt(m[1], 10) * 60 + parseInt(m[2], 10);
}

// Fetches current weather by geolocation and sends it to the watch.
function fetchWeather() {
  navigator.geolocation.getCurrentPosition(
    function(pos) {
      // Sunrise/sunset ride along with the same request: no second round trip,
      // no API key, no location leaving the phone that wasn't already.
      // timezone=auto makes the daily times local to the coordinates, which is
      // what "minutes since midnight" has to mean on the watch.
      var url = 'https://api.open-meteo.com/v1/forecast?latitude='
        + pos.coords.latitude + '&longitude=' + pos.coords.longitude
        + '&current_weather=true&daily=sunrise,sunset&timezone=auto'
        + '&temperature_unit=' + tempUnit();
      var xhr = new XMLHttpRequest();
      xhr.open('GET', url);
      xhr.onload = function() {
        try {
          var data = JSON.parse(xhr.responseText);
          var cw = data.current_weather;
          console.log('weather: ' + cw.temperature + ' code ' + cw.weathercode);
          var msg = {};
          msg[keys.WEATHER_TEMP] = Math.round(cw.temperature);
          msg[keys.WEATHER_COND] = condFromWMO(cw.weathercode);

          // Only sent when both are present: half a pair would leave the
          // daylight bar with one end of a range it cannot measure.
          var daily = data.daily || {};
          var rise = minutesFromISO((daily.sunrise || [])[0]);
          var set = minutesFromISO((daily.sunset || [])[0]);
          if (rise !== null && set !== null) {
            msg[keys.SUN_RISE] = rise;
            msg[keys.SUN_SET] = set;
            console.log('sun: rise ' + rise + ' set ' + set + ' (minutes)');
          }

          Pebble.sendAppMessage(msg);
          localStorage.setItem('lastWeather', String(Date.now()));
        } catch (err) {
          console.log('weather parse error: ' + err);
        }
      };
      xhr.onerror = function() { console.log('weather request failed'); };
      xhr.send();
    },
    function(err) { console.log('geolocation error: ' + err.message); },
    { timeout: 15000, maximumAge: 60000 }
  );
}

// Fetches weather only if the last update is older than the refresh interval.
function maybeFetchWeather() {
  var last = parseInt(localStorage.getItem('lastWeather') || '0', 10);
  if (Date.now() - last > WEATHER_REFRESH_MS) {
    fetchWeather();
  }
}

// Fetches the user-configured custom-metric JSON and sends up to two items
// to the watch as "Aangepast 1/2" progressbars. The url itself never leaves
// the phone — only the parsed percentages + icon ids go over AppMessage.
// Expected shape: {"items":[{"name":"...","value":42}, ...]}.
function fetchCustom() {
  var url = localStorage.getItem('customUrl') || '';
  if (!url) { return; }
  // A malicious or compromised endpoint could return a huge/pathological body
  // to try to stall or OOM the phone's JS runtime; only ever 2 small items are
  // needed, so anything past a few KB is refused outright rather than parsed.
  var MAX_CUSTOM_RESPONSE_BYTES = 8192;
  var xhr = new XMLHttpRequest();
  xhr.open('GET', url);
  xhr.timeout = 15000;
  if (typeof xhr.onprogress !== 'undefined') {
    xhr.onprogress = function(event) {
      var size = (event && (event.loaded || event.total)) || 0;
      if (size > MAX_CUSTOM_RESPONSE_BYTES) {
        xhr.abort();
        console.log('custom response too large, aborted');
      }
    };
  }
  xhr.onload = function() {
    try {
      if (xhr.responseText.length > MAX_CUSTOM_RESPONSE_BYTES) {
        console.log('custom response too large, ignored');
        return;
      }
      var items = JSON.parse(xhr.responseText).items || [];
      var msg = {};
      if (items[0]) {
        var v0 = clampPct(items[0].value);
        if (v0 !== null) {
          msg[keys.CUSTOM1_VALUE] = v0;
          msg[keys.CUSTOM1_ICON] = customIconFor(items[0].name);
        }
      }
      if (items[1]) {
        var v1 = clampPct(items[1].value);
        if (v1 !== null) {
          msg[keys.CUSTOM2_VALUE] = v1;
          msg[keys.CUSTOM2_ICON] = customIconFor(items[1].name);
        }
      }
      if (Object.keys(msg).length > 0) {
        Pebble.sendAppMessage(msg);
      }
      localStorage.setItem('lastCustom', String(Date.now()));
    } catch (err) {
      console.log('custom JSON parse error: ' + err);
    }
  };
  xhr.onerror = function() { console.log('custom request failed'); };
  xhr.ontimeout = function() { console.log('custom request timed out'); };
  xhr.send();
}

// Fetches the custom metric only if the last update is older than the
// refresh interval, and only when a url is configured.
function maybeFetchCustom() {
  if (!localStorage.getItem('customUrl')) { return; }
  var last = parseInt(localStorage.getItem('lastCustom') || '0', 10);
  if (Date.now() - last > CUSTOM_REFRESH_MS) {
    fetchCustom();
  }
}

// On launch: fetch weather + custom metric and start the periodic refresh checks.
Pebble.addEventListener('ready', function() {
  fetchWeather();
  fetchCustom();
  setInterval(maybeFetchWeather, 5 * 60 * 1000);
  setInterval(maybeFetchCustom, 60 * 1000);  // must be <= CUSTOM_REFRESH_MS or the check itself becomes the bottleneck
});

// Coerces a Clay select (returned as a string) to an int. A missing or
// unparseable value is dropped rather than forced to 0: 0 is a real setting
// ("None" for a widget slot), so sending it would silently reset the face.
function toInt(dict, key) {
  var n = parseInt(dict[key], 10);
  if (isNaN(n)) {
    delete dict[key];
  } else {
    dict[key] = n;
  }
}

// Converts the distance goal to meters, which is the only unit the watch
// knows. The page shows one of the two fields depending on the unit setting,
// so the other one is whatever was last typed there and must be ignored.
// Both raw fields are deleted afterwards: they are page-side inputs, not
// settings the watch has any use for.
var METERS_PER_KM = 1000;
var METERS_PER_MILE = 1609;

function resolveDistanceGoal(dict) {
  var miles = (dict[keys.DIST_UNIT] === DIST_MILES);
  var typed = parseFloat(miles ? dict[keys.GOAL_DIST_MI] : dict[keys.GOAL_DIST_KM]);
  delete dict[keys.GOAL_DIST_KM];
  delete dict[keys.GOAL_DIST_MI];
  if (isNaN(typed) || typed < 0) { return; }
  // 0 stays 0: that is the "use my own average" sentinel, not a zero-meter goal.
  dict[keys.GOAL_DIST] = Math.round(typed * (miles ? METERS_PER_MILE : METERS_PER_KM));
}

// On Clay close: forward the settings to the watch (coercing selects to ints).
Pebble.addEventListener('webviewclosed', function(e) {
  if (!e || !e.response) { return; }
  var dict = clay.getSettings(e.response);
  [keys.LAYOUT_MODE, keys.PROGRESS_TYPE, keys.PROGRESS_TYPE_2, keys.PROGRESS_INFO,
   keys.WIDGET_LEFT, keys.WIDGET_MID, keys.WIDGET_RIGHT, keys.TEMP_UNIT,
   keys.LANGUAGE, keys.CLOCK_SCHEME, keys.DIST_UNIT, keys.TAP_MODE, keys.THEME,
   keys.GOAL_STEPS, keys.GOAL_KCAL
  ].forEach(function(key) { toInt(dict, key); });
  resolveDistanceGoal(dict);
  localStorage.setItem('TEMP_UNIT', String(dict[keys.TEMP_UNIT] || 0));
  // Remember the language so the settings page itself opens in it next time.
  localStorage.setItem('LANG', String(dict[keys.LANGUAGE]));

  // The custom-metric url is phone-only: save it locally and drop it from the
  // dict so it's never transmitted to the watch over AppMessage.
  var customUrl = dict[keys.CUSTOM_URL] || '';
  localStorage.setItem('customUrl', customUrl);
  delete dict[keys.CUSTOM_URL];

  Pebble.sendAppMessage(dict,
    function() {
      fetchWeather();  // re-fetch so unit change takes effect
      if (customUrl) { fetchCustom(); }  // re-fetch so a new/changed url takes effect
    },
    function() { console.log('settings send failed'); }
  );
});
