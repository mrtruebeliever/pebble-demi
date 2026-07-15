var Clay = require('pebble-clay');
var clayConfig = require('./config.json');
var clay = new Clay(clayConfig);
var keys = require('message_keys');

var WEATHER_REFRESH_MS = 30 * 60 * 1000;  // 30 minutes

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

// On launch: fetch weather and start the periodic refresh check.
Pebble.addEventListener('ready', function() {
  fetchWeather();
  setInterval(maybeFetchWeather, 5 * 60 * 1000);
});

// On Clay close: forward the settings to the watch (coercing selects to ints).
Pebble.addEventListener('webviewclosed', function(e) {
  if (!e || !e.response) { return; }
  var dict = clay.getSettings(e.response);
  dict[keys.LAYOUT_MODE] = parseInt(dict[keys.LAYOUT_MODE], 10) || 0;
  dict[keys.PROGRESS_TYPE] = parseInt(dict[keys.PROGRESS_TYPE], 10) || 0;
  dict[keys.WIDGET_LEFT] = parseInt(dict[keys.WIDGET_LEFT], 10) || 0;
  dict[keys.WIDGET_MID] = parseInt(dict[keys.WIDGET_MID], 10) || 0;
  dict[keys.WIDGET_RIGHT] = parseInt(dict[keys.WIDGET_RIGHT], 10) || 0;
  dict[keys.TEMP_UNIT] = parseInt(dict[keys.TEMP_UNIT], 10) || 0;
  dict[keys.LANGUAGE] = parseInt(dict[keys.LANGUAGE], 10) || 0;
  dict[keys.CLOCK_SCHEME] = parseInt(dict[keys.CLOCK_SCHEME], 10) || 0;
  localStorage.setItem('TEMP_UNIT', String(dict[keys.TEMP_UNIT]));
  Pebble.sendAppMessage(dict,
    function() { fetchWeather(); },  // re-fetch so unit change takes effect
    function() { console.log('settings send failed'); }
  );
});
