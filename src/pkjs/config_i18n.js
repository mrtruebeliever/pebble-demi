// English translations for the Clay settings page.
//
// config.json stays the single source of the page's structure, in Dutch. This
// module walks a copy of it and swaps the display strings, so a layout change
// only ever has to be made in one place. Keys are the Dutch strings exactly as
// they appear there; anything without a key is left alone, which is what keeps
// language names, colour values and select values untouched.
//
// Only 'heading' and 'submit' use defaultValue as display text -- for every
// other item type it is the actual stored value and must never be translated.

var EN = {
  'Uiterlijk': 'Appearance',
  'Accentkleur': 'Accent colour',
  'Kleuren uren/minuten': 'Hour / minute colours',
  'Wit / donkergrijs': 'White / dark grey',
  'Wit / wit': 'White / white',
  'Wit / lichtgrijs (e-paper)': 'White / light grey (e-paper)',
  'Lichtgrijs / wit (e-paper)': 'Light grey / white (e-paper)',
  'Accent / wit': 'Accent / white',
  'Wit / accent': 'White / accent',
  'Accent / donkergrijs': 'Accent / dark grey',
  'Accent / lichtgrijs': 'Accent / light grey',
  'Indeling': 'Layout',
  'Verticaal (uren boven minuten)': 'Vertical (hours above minutes)',
  'Horizontaal (verticale bar)': 'Horizontal (vertical bar)',
  'Horizontaal (twee bars)': 'Horizontal (two bars)',
  '24-uurs klok': '24-hour clock',
  'Progressbar': 'Progress bar',
  'Toont': 'Shows',
  'Stappen': 'Steps',
  'Batterij': 'Battery',
  'Calorieën': 'Calories',
  'Afstand': 'Distance',
  'Aangepast 1 (JSON-url)': 'Custom 1 (JSON url)',
  'Aangepast 2 (JSON-url)': 'Custom 2 (JSON url)',
  'Tweede bar (alleen bij twee bars)': 'Second bar (two-bar layout only)',
  'Naast de bar': 'Beside the bar',
  'Niets': 'Nothing',
  'Alleen icoon': 'Icon only',
  'Icoon en waarde': 'Icon and value',
  'Icoon en waarde omwisselen': 'Swap icon and value',
  'Tweede bar eigen kleur': 'Second bar has its own colour',
  'Kleur tweede bar': 'Second bar colour',
  'Widgets onderaan': 'Bottom widgets',
  'Links': 'Left',
  'Geen': 'None',
  'Datum': 'Date',
  'Weer': 'Weather',
  'Hartslag': 'Heart rate',
  'Midden': 'Middle',
  'Rechts': 'Right',
  'Toon batterijpercentage': 'Show battery percentage',
  'Taal': 'Language',
  'Taal datum / Date language': 'Date language',
  'Automatisch (horlogetaal) / Automatic': 'Automatic (watch language)',
  'Aangepast (JSON-url)': 'Custom (JSON url)',
  'Temperatuureenheid': 'Temperature unit',
  'Weericoon in accentkleur': 'Weather icon in accent colour',
  'Opslaan': 'Save'
  // 'Demi', 'URL', 'Celsius', 'Fahrenheit' and the language names are the same
  // in both, so they are deliberately absent.
};

// Deep copy, translating display strings as it goes.
function translate(node, map) {
  if (Array.isArray(node)) {
    return node.map(function (n) { return translate(n, map); });
  }
  if (node === null || typeof node !== 'object') { return node; }

  var out = {};
  var displayDefault = (node.type === 'heading' || node.type === 'submit');
  Object.keys(node).forEach(function (k) {
    var v = node[k];
    var translatable = (k === 'label' || k === 'description' ||
                        (k === 'defaultValue' && displayDefault));
    if (translatable && typeof v === 'string' && map[v]) {
      out[k] = map[v];
    } else {
      out[k] = translate(v, map);
    }
  });
  return out;
}

// lang is the watch-side index (see src/c/config.h): 1 = Nederlands keeps the
// page as authored; everything else, including LANG_AUTO, gets English.
function buildConfig(clayConfig, lang) {
  if (lang === 1) { return clayConfig; }
  return translate(clayConfig, EN);
}

module.exports = { buildConfig: buildConfig, EN: EN };
