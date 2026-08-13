// Translations for the Clay settings page.
//
// config.json stays the single source of the page's structure, in Dutch. This
// module walks a copy of it and swaps the display strings, so a layout change
// only ever has to be made in one place. Keys are the Dutch strings exactly as
// they appear there; anything without a key is left alone, which is what keeps
// language names, colour values and select values untouched.
//
// Only 'heading', 'submit' and 'text' use defaultValue as display text -- for
// every other item type it is the actual stored value and must never be
// translated. ('text' was missing here, which left its paragraphs in Dutch on
// every non-Dutch page; those items carry no messageKey, so nothing is stored.)
//
// The watch speaks five languages; so does this page. A map that is missing a
// string simply leaves the Dutch through, so a half-finished translation
// degrades rather than breaks.

var EN = {
  'Toon icoon en waarde bij tik': 'Show icon and value on tap',
  'Als je hierboven "Niets" of "Alleen icoon" koos, laat een tik op de pols de verborgen details vijf seconden zien.':
    'If you chose "Nothing" or "Icon only" above, a tap on the wrist shows the hidden details for five seconds.',
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
  'Tik op het horloge toont': 'A tap on the watch shows',
  'Een tik op de pols vervangt de widgetrij vijf seconden lang.':
    'A tap on the wrist replaces the widget row for five seconds.',
  'Seconden': 'Seconds',
  'Volledige datum': 'Full date',
  'Progressbar': 'Progress bar',
  'Toont': 'Shows',
  'Stappen': 'Steps',
  'Batterij': 'Battery',
  'Calorieën': 'Calories',
  'Afstand': 'Distance',
  'Aangepast 1 (JSON-url)': 'Custom 1 (JSON url)',
  'Aangepast 2 (JSON-url)': 'Custom 2 (JSON url)',
  'Daglicht': 'Daylight',
  'Dag verstreken': 'Day elapsed',
  'Week verstreken': 'Week elapsed',
  'Maand verstreken': 'Month elapsed',
  'Jaar verstreken': 'Year elapsed',
  'Tweede bar (alleen bij twee bars)': 'Second bar (two-bar layout only)',
  'Naast de bar': 'Beside the bar',
  'Niets': 'Nothing',
  'Alleen icoon': 'Icon only',
  'Icoon en waarde': 'Icon and value',
  'Icoon en waarde omwisselen': 'Swap icon and value',
  'Toon mijn normale tempo': 'Show my usual pace',
  'Zet een streepje op de bar waar je op dit tijdstip meestal staat. Alleen bij stappen, calorieën en afstand.':
    'Puts a mark on the bar where you usually stand at this time of day. Steps, calories and distance only.',
  'Tweede bar eigen kleur': 'Second bar has its own colour',
  'Kleur tweede bar': 'Second bar colour',
  'Doelen': 'Goals',
  'Waar de bar 100% bereikt. Vul <code>0</code> in om je eigen daggemiddelde als doel te gebruiken.':
    'Where the bar reaches 100%. Enter <code>0</code> to use your own daily average as the goal.',
  'Stappendoel': 'Step goal',
  'Caloriedoel': 'Calorie goal',
  'Eenheid afstand': 'Distance unit',
  'Automatisch (horloge-instelling)': 'Automatic (watch setting)',
  'Kilometers': 'Kilometres',
  'Mijlen': 'Miles',
  'Afstandsdoel (km)': 'Distance goal (km)',
  'Afstandsdoel (mijl)': 'Distance goal (miles)',
  'Widgets onderaan': 'Bottom widgets',
  'Kies per positie welke widget verschijnt (max. 3).':
    'Choose which widget appears in each position (3 at most).',
  'Links': 'Left',
  'Geen': 'None',
  'Datum': 'Date',
  'Weer': 'Weather',
  'Hartslag': 'Heart rate',
  'Zon op/onder': 'Sunrise / sunset',
  'Midden': 'Middle',
  'Rechts': 'Right',
  'Toon batterijpercentage': 'Show battery percentage',
  'Taal': 'Language',
  'Taal datum': 'Date language',
  'Automatisch (horlogetaal)': 'Automatic (watch language)',
  'Aangepast (JSON-url)': 'Custom (JSON url)',
  'Voor de "Aangepast 1/2"-bars: een url die JSON teruggeeft in de vorm <code>{"items":[{"name":"...","value":42}]}</code> (value 0-100, item 0 → Aangepast 1, item 1 → Aangepast 2). De url zelf blijft op de telefoon en wordt nooit naar het horloge gestuurd — alleen de opgehaalde percentages.':
    'For the "Custom 1/2" bars: a url returning JSON shaped like <code>{"items":[{"name":"...","value":42}]}</code> (value 0-100, item 0 → Custom 1, item 1 → Custom 2). The url itself stays on the phone and is never sent to the watch — only the fetched percentages.',
  'Temperatuureenheid': 'Temperature unit',
  'Weericoon in accentkleur': 'Weather icon in accent colour',
  'Opslaan': 'Save'
  // 'Demi', 'URL', 'Celsius', 'Fahrenheit' and the language names are the same
  // in both, so they are deliberately absent.
};

var FR = {
  'Toon icoon en waarde bij tik': 'Afficher l\'icône et la valeur en tapant',
  'Als je hierboven "Niets" of "Alleen icoon" koos, laat een tik op de pols de verborgen details vijf seconden zien.':
    'Si vous avez choisi Â« Rien Â» ou Â« Icône seule Â» ci-dessus, une tape sur le poignet affiche les détails masqués pendant cinq secondes.',
  'Uiterlijk': 'Apparence',
  'Accentkleur': 'Couleur d\'accent',
  'Kleuren uren/minuten': 'Couleurs heures / minutes',
  'Wit / donkergrijs': 'Blanc / gris foncé',
  'Wit / wit': 'Blanc / blanc',
  'Wit / lichtgrijs (e-paper)': 'Blanc / gris clair (e-paper)',
  'Lichtgrijs / wit (e-paper)': 'Gris clair / blanc (e-paper)',
  'Accent / wit': 'Accent / blanc',
  'Wit / accent': 'Blanc / accent',
  'Accent / donkergrijs': 'Accent / gris foncé',
  'Accent / lichtgrijs': 'Accent / gris clair',
  'Indeling': 'Disposition',
  'Verticaal (uren boven minuten)': 'Vertical (heures au-dessus des minutes)',
  'Horizontaal (verticale bar)': 'Horizontal (barre verticale)',
  'Horizontaal (twee bars)': 'Horizontal (deux barres)',
  '24-uurs klok': 'Horloge 24 heures',
  'Tik op het horloge toont': 'Une tape sur la montre affiche',
  'Een tik op de pols vervangt de widgetrij vijf seconden lang.':
    'Une tape sur le poignet remplace la rangée de widgets pendant cinq secondes.',
  'Seconden': 'Secondes',
  'Volledige datum': 'Date complète',
  'Progressbar': 'Barre de progression',
  'Toont': 'Affiche',
  'Stappen': 'Pas',
  'Batterij': 'Batterie',
  'Calorieën': 'Calories',
  'Afstand': 'Distance',
  'Aangepast 1 (JSON-url)': 'Personnalisé 1 (url JSON)',
  'Aangepast 2 (JSON-url)': 'Personnalisé 2 (url JSON)',
  'Daglicht': 'Lumière du jour',
  'Dag verstreken': 'Jour écoulé',
  'Week verstreken': 'Semaine écoulée',
  'Maand verstreken': 'Mois écoulé',
  'Jaar verstreken': 'Année écoulée',
  'Tweede bar (alleen bij twee bars)': 'Deuxième barre (disposition à deux barres)',
  'Naast de bar': 'À côté de la barre',
  'Niets': 'Rien',
  'Alleen icoon': 'Icône seule',
  'Icoon en waarde': 'Icône et valeur',
  'Icoon en waarde omwisselen': 'Permuter l\'icône et la valeur',
  'Toon mijn normale tempo': 'Afficher mon rythme habituel',
  'Zet een streepje op de bar waar je op dit tijdstip meestal staat. Alleen bij stappen, calorieën en afstand.':
    'Place un repère sur la barre là où vous en êtes d\'habitude à cette heure. Pas, calories et distance uniquement.',
  'Tweede bar eigen kleur': 'Couleur propre pour la deuxième barre',
  'Kleur tweede bar': 'Couleur de la deuxième barre',
  'Doelen': 'Objectifs',
  'Waar de bar 100% bereikt. Vul <code>0</code> in om je eigen daggemiddelde als doel te gebruiken.':
    'Où la barre atteint 100 %. Saisissez <code>0</code> pour utiliser votre propre moyenne quotidienne comme objectif.',
  'Stappendoel': 'Objectif de pas',
  'Caloriedoel': 'Objectif de calories',
  'Eenheid afstand': 'Unité de distance',
  'Automatisch (horloge-instelling)': 'Automatique (réglage de la montre)',
  'Kilometers': 'Kilomètres',
  'Mijlen': 'Miles',
  'Afstandsdoel (km)': 'Objectif de distance (km)',
  'Afstandsdoel (mijl)': 'Objectif de distance (miles)',
  'Widgets onderaan': 'Widgets en bas',
  'Kies per positie welke widget verschijnt (max. 3).':
    'Choisissez le widget affiché à chaque position (3 au maximum).',
  'Links': 'Gauche',
  'Geen': 'Aucun',
  'Datum': 'Date',
  'Weer': 'Météo',
  'Hartslag': 'Fréquence cardiaque',
  'Zon op/onder': 'Lever / coucher du soleil',
  'Midden': 'Milieu',
  'Rechts': 'Droite',
  'Toon batterijpercentage': 'Afficher le pourcentage de batterie',
  'Taal': 'Langue',
  'Taal datum': 'Langue de la date',
  'Automatisch (horlogetaal)': 'Automatique (langue de la montre)',
  'Aangepast (JSON-url)': 'Personnalisé (url JSON)',
  'Voor de "Aangepast 1/2"-bars: een url die JSON teruggeeft in de vorm <code>{"items":[{"name":"...","value":42}]}</code> (value 0-100, item 0 → Aangepast 1, item 1 → Aangepast 2). De url zelf blijft op de telefoon en wordt nooit naar het horloge gestuurd — alleen de opgehaalde percentages.':
    'Pour les barres « Personnalisé 1/2 » : une url renvoyant du JSON de la forme <code>{"items":[{"name":"...","value":42}]}</code> (value 0-100, item 0 → Personnalisé 1, item 1 → Personnalisé 2). L\'url elle-même reste sur le téléphone et n\'est jamais envoyée à la montre — seuls les pourcentages récupérés le sont.',
  'Temperatuureenheid': 'Unité de température',
  'Weericoon in accentkleur': 'Icône météo en couleur d\'accent',
  'Opslaan': 'Enregistrer'
};

var DE = {
  'Toon icoon en waarde bij tik': 'Symbol und Wert beim Tippen zeigen',
  'Als je hierboven "Niets" of "Alleen icoon" koos, laat een tik op de pols de verborgen details vijf seconden zien.':
    'Wenn du oben „Nichts“ oder „Nur Symbol“ gewählt hast, zeigt ein Tippen aufs Handgelenk die verborgenen Angaben fünf Sekunden lang.',
  'Uiterlijk': 'Erscheinungsbild',
  'Accentkleur': 'Akzentfarbe',
  'Kleuren uren/minuten': 'Farben Stunden / Minuten',
  'Wit / donkergrijs': 'Weiß / Dunkelgrau',
  'Wit / wit': 'Weiß / Weiß',
  'Wit / lichtgrijs (e-paper)': 'Weiß / Hellgrau (E-Paper)',
  'Lichtgrijs / wit (e-paper)': 'Hellgrau / Weiß (E-Paper)',
  'Accent / wit': 'Akzent / Weiß',
  'Wit / accent': 'Weiß / Akzent',
  'Accent / donkergrijs': 'Akzent / Dunkelgrau',
  'Accent / lichtgrijs': 'Akzent / Hellgrau',
  'Indeling': 'Layout',
  'Verticaal (uren boven minuten)': 'Vertikal (Stunden über Minuten)',
  'Horizontaal (verticale bar)': 'Horizontal (vertikaler Balken)',
  'Horizontaal (twee bars)': 'Horizontal (zwei Balken)',
  '24-uurs klok': '24-Stunden-Uhr',
  'Tik op het horloge toont': 'Tippen auf die Uhr zeigt',
  'Een tik op de pols vervangt de widgetrij vijf seconden lang.':
    'Ein Tippen aufs Handgelenk ersetzt die Widget-Zeile für fünf Sekunden.',
  'Seconden': 'Sekunden',
  'Volledige datum': 'Vollständiges Datum',
  'Progressbar': 'Fortschrittsbalken',
  'Toont': 'Zeigt',
  'Stappen': 'Schritte',
  'Batterij': 'Akku',
  'Calorieën': 'Kalorien',
  'Afstand': 'Entfernung',
  'Aangepast 1 (JSON-url)': 'Eigene 1 (JSON-URL)',
  'Aangepast 2 (JSON-url)': 'Eigene 2 (JSON-URL)',
  'Daglicht': 'Tageslicht',
  'Dag verstreken': 'Tag vergangen',
  'Week verstreken': 'Woche vergangen',
  'Maand verstreken': 'Monat vergangen',
  'Jaar verstreken': 'Jahr vergangen',
  'Tweede bar (alleen bij twee bars)': 'Zweiter Balken (nur bei zwei Balken)',
  'Naast de bar': 'Neben dem Balken',
  'Niets': 'Nichts',
  'Alleen icoon': 'Nur Symbol',
  'Icoon en waarde': 'Symbol und Wert',
  'Icoon en waarde omwisselen': 'Symbol und Wert tauschen',
  'Toon mijn normale tempo': 'Mein übliches Tempo anzeigen',
  'Zet een streepje op de bar waar je op dit tijdstip meestal staat. Alleen bij stappen, calorieën en afstand.':
    'Setzt eine Markierung auf den Balken, wo du um diese Uhrzeit normalerweise stehst. Nur bei Schritten, Kalorien und Entfernung.',
  'Tweede bar eigen kleur': 'Eigene Farbe für zweiten Balken',
  'Kleur tweede bar': 'Farbe des zweiten Balkens',
  'Doelen': 'Ziele',
  'Waar de bar 100% bereikt. Vul <code>0</code> in om je eigen daggemiddelde als doel te gebruiken.':
    'Wo der Balken 100 % erreicht. Gib <code>0</code> ein, um deinen eigenen Tagesdurchschnitt als Ziel zu verwenden.',
  'Stappendoel': 'Schrittziel',
  'Caloriedoel': 'Kalorienziel',
  'Eenheid afstand': 'Entfernungseinheit',
  'Automatisch (horloge-instelling)': 'Automatisch (Uhreinstellung)',
  'Kilometers': 'Kilometer',
  'Mijlen': 'Meilen',
  'Afstandsdoel (km)': 'Entfernungsziel (km)',
  'Afstandsdoel (mijl)': 'Entfernungsziel (Meilen)',
  'Widgets onderaan': 'Widgets unten',
  'Kies per positie welke widget verschijnt (max. 3).':
    'Wähle pro Position, welches Widget erscheint (höchstens 3).',
  'Links': 'Links',
  'Geen': 'Keins',
  'Datum': 'Datum',
  'Weer': 'Wetter',
  'Hartslag': 'Herzfrequenz',
  'Zon op/onder': 'Sonnenauf-/untergang',
  'Midden': 'Mitte',
  'Rechts': 'Rechts',
  'Toon batterijpercentage': 'Akkuprozent anzeigen',
  'Taal': 'Sprache',
  'Taal datum': 'Sprache des Datums',
  'Automatisch (horlogetaal)': 'Automatisch (Uhrensprache)',
  'Aangepast (JSON-url)': 'Eigene (JSON-URL)',
  'Voor de "Aangepast 1/2"-bars: een url die JSON teruggeeft in de vorm <code>{"items":[{"name":"...","value":42}]}</code> (value 0-100, item 0 → Aangepast 1, item 1 → Aangepast 2). De url zelf blijft op de telefoon en wordt nooit naar het horloge gestuurd — alleen de opgehaalde percentages.':
    'Für die Balken „Eigene 1/2“: eine URL, die JSON der Form <code>{"items":[{"name":"...","value":42}]}</code> zurückgibt (value 0-100, item 0 → Eigene 1, item 1 → Eigene 2). Die URL selbst bleibt auf dem Telefon und wird nie an die Uhr gesendet — nur die abgerufenen Prozentwerte.',
  'Temperatuureenheid': 'Temperatureinheit',
  'Weericoon in accentkleur': 'Wettersymbol in Akzentfarbe',
  'Opslaan': 'Speichern'
};

var ES = {
  'Toon icoon en waarde bij tik': 'Mostrar icono y valor al tocar',
  'Als je hierboven "Niets" of "Alleen icoon" koos, laat een tik op de pols de verborgen details vijf seconden zien.':
    'Si arriba elegiste «Nada» o «Solo icono», un toque en la muñeca muestra los detalles ocultos durante cinco segundos.',
  'Uiterlijk': 'Apariencia',
  'Accentkleur': 'Color de acento',
  'Kleuren uren/minuten': 'Colores horas / minutos',
  'Wit / donkergrijs': 'Blanco / gris oscuro',
  'Wit / wit': 'Blanco / blanco',
  'Wit / lichtgrijs (e-paper)': 'Blanco / gris claro (e-paper)',
  'Lichtgrijs / wit (e-paper)': 'Gris claro / blanco (e-paper)',
  'Accent / wit': 'Acento / blanco',
  'Wit / accent': 'Blanco / acento',
  'Accent / donkergrijs': 'Acento / gris oscuro',
  'Accent / lichtgrijs': 'Acento / gris claro',
  'Indeling': 'Disposición',
  'Verticaal (uren boven minuten)': 'Vertical (horas sobre minutos)',
  'Horizontaal (verticale bar)': 'Horizontal (barra vertical)',
  'Horizontaal (twee bars)': 'Horizontal (dos barras)',
  '24-uurs klok': 'Reloj de 24 horas',
  'Tik op het horloge toont': 'Un toque en el reloj muestra',
  'Een tik op de pols vervangt de widgetrij vijf seconden lang.':
    'Un toque en la muñeca sustituye la fila de widgets durante cinco segundos.',
  'Seconden': 'Segundos',
  'Volledige datum': 'Fecha completa',
  'Progressbar': 'Barra de progreso',
  'Toont': 'Muestra',
  'Stappen': 'Pasos',
  'Batterij': 'Batería',
  'Calorieën': 'Calorías',
  'Afstand': 'Distancia',
  'Aangepast 1 (JSON-url)': 'Personalizado 1 (url JSON)',
  'Aangepast 2 (JSON-url)': 'Personalizado 2 (url JSON)',
  'Daglicht': 'Luz diurna',
  'Dag verstreken': 'Día transcurrido',
  'Week verstreken': 'Semana transcurrida',
  'Maand verstreken': 'Mes transcurrido',
  'Jaar verstreken': 'Año transcurrido',
  'Tweede bar (alleen bij twee bars)': 'Segunda barra (solo con dos barras)',
  'Naast de bar': 'Junto a la barra',
  'Niets': 'Nada',
  'Alleen icoon': 'Solo icono',
  'Icoon en waarde': 'Icono y valor',
  'Icoon en waarde omwisselen': 'Intercambiar icono y valor',
  'Toon mijn normale tempo': 'Mostrar mi ritmo habitual',
  'Zet een streepje op de bar waar je op dit tijdstip meestal staat. Alleen bij stappen, calorieën en afstand.':
    'Pone una marca en la barra donde sueles estar a esta hora. Solo para pasos, calorías y distancia.',
  'Tweede bar eigen kleur': 'Color propio para la segunda barra',
  'Kleur tweede bar': 'Color de la segunda barra',
  'Doelen': 'Objetivos',
  'Waar de bar 100% bereikt. Vul <code>0</code> in om je eigen daggemiddelde als doel te gebruiken.':
    'Donde la barra llega al 100 %. Introduce <code>0</code> para usar tu propia media diaria como objetivo.',
  'Stappendoel': 'Objetivo de pasos',
  'Caloriedoel': 'Objetivo de calorías',
  'Eenheid afstand': 'Unidad de distancia',
  'Automatisch (horloge-instelling)': 'Automático (ajuste del reloj)',
  'Kilometers': 'Kilómetros',
  'Mijlen': 'Millas',
  'Afstandsdoel (km)': 'Objetivo de distancia (km)',
  'Afstandsdoel (mijl)': 'Objetivo de distancia (millas)',
  'Widgets onderaan': 'Widgets inferiores',
  'Kies per positie welke widget verschijnt (max. 3).':
    'Elige qué widget aparece en cada posición (3 como máximo).',
  'Links': 'Izquierda',
  'Geen': 'Ninguno',
  'Datum': 'Fecha',
  'Weer': 'Tiempo',
  'Hartslag': 'Frecuencia cardíaca',
  'Zon op/onder': 'Amanecer / atardecer',
  'Midden': 'Centro',
  'Rechts': 'Derecha',
  'Toon batterijpercentage': 'Mostrar porcentaje de batería',
  'Taal': 'Idioma',
  'Taal datum': 'Idioma de la fecha',
  'Automatisch (horlogetaal)': 'Automático (idioma del reloj)',
  'Aangepast (JSON-url)': 'Personalizado (url JSON)',
  'Voor de "Aangepast 1/2"-bars: een url die JSON teruggeeft in de vorm <code>{"items":[{"name":"...","value":42}]}</code> (value 0-100, item 0 → Aangepast 1, item 1 → Aangepast 2). De url zelf blijft op de telefoon en wordt nooit naar het horloge gestuurd — alleen de opgehaalde percentages.':
    'Para las barras «Personalizado 1/2»: una url que devuelva JSON con la forma <code>{"items":[{"name":"...","value":42}]}</code> (value 0-100, item 0 → Personalizado 1, item 1 → Personalizado 2). La url en sí permanece en el teléfono y nunca se envía al reloj — solo los porcentajes obtenidos.',
  'Temperatuureenheid': 'Unidad de temperatura',
  'Weericoon in accentkleur': 'Icono del tiempo en color de acento',
  'Opslaan': 'Guardar'
};

// Deep copy, translating display strings as it goes.
function translate(node, map) {
  if (Array.isArray(node)) {
    return node.map(function (n) { return translate(n, map); });
  }
  if (node === null || typeof node !== 'object') { return node; }

  var out = {};
  var displayDefault = (node.type === 'heading' || node.type === 'submit' ||
                        node.type === 'text');
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

// Keyed by the watch-side language index (see src/c/config.h). 1 = Nederlands
// is absent on purpose: that is the page as authored, so it needs no pass.
var MAPS = {
  0: EN,
  2: FR,
  3: DE,
  4: ES
};

// lang is the watch-side index. Nederlands keeps the page as authored;
// anything without a map of its own -- including LANG_AUTO -- gets English.
function buildConfig(clayConfig, lang) {
  if (lang === 1) { return clayConfig; }
  return translate(clayConfig, MAPS[lang] || EN);
}

module.exports = { buildConfig: buildConfig, EN: EN, FR: FR, DE: DE, ES: ES };
