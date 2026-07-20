var Clay = require('pebble-clay');
var clayConfig = require('./config.json');
var clay = new Clay(clayConfig);
var keys = require('message_keys');

var WEATHER_REFRESH_MS = 30 * 60 * 1000;  // 30 minutes
var CUSTOM_REFRESH_MS = 3 * 60 * 1000;    // 3 minutes — below the gist CDN's own 5-min cache floor, so shorter gains little

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

// Fetches current weather by geolocation and sends it to the watch.
function fetchWeather() {
  navigator.geolocation.getCurrentPosition(
    function(pos) {
      var url = 'https://api.open-meteo.com/v1/forecast?latitude='
        + pos.coords.latitude + '&longitude=' + pos.coords.longitude
        + '&current_weather=true&temperature_unit=' + tempUnit();
      var xhr = new XMLHttpRequest();
      xhr.open('GET', url);
      xhr.onload = function() {
        try {
          var cw = JSON.parse(xhr.responseText).current_weather;
          console.log('weather: ' + cw.temperature + ' code ' + cw.weathercode);
          var msg = {};
          msg[keys.WEATHER_TEMP] = Math.round(cw.temperature);
          msg[keys.WEATHER_COND] = condFromWMO(cw.weathercode);
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

// On Clay close: forward the settings to the watch (coercing selects to ints).
Pebble.addEventListener('webviewclosed', function(e) {
  if (!e || !e.response) { return; }
  var dict = clay.getSettings(e.response);
  [keys.LAYOUT_MODE, keys.PROGRESS_TYPE, keys.PROGRESS_TYPE_2, keys.PROGRESS_INFO,
   keys.WIDGET_LEFT, keys.WIDGET_MID, keys.WIDGET_RIGHT, keys.TEMP_UNIT,
   keys.LANGUAGE, keys.CLOCK_SCHEME
  ].forEach(function(key) { toInt(dict, key); });
  localStorage.setItem('TEMP_UNIT', String(dict[keys.TEMP_UNIT] || 0));

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
