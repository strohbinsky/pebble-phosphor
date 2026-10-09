// Phosphor — reine Rechenlogik des Handy-Skripts (ohne Pebble-API), damit sie in Node testbar ist.
var RAD = Math.PI / 180;

// Großkreis-Entfernung in km
function entfernungKm(la1, lo1, la2, lo2) {
  var a = Math.sin((la2 - la1) * RAD / 2), b = Math.sin((lo2 - lo1) * RAD / 2);
  var h = a * a + Math.cos(la1 * RAD) * Math.cos(la2 * RAD) * b * b;
  return 6371 * 2 * Math.asin(Math.min(1, Math.sqrt(h)));
}

// Nächster große Linienflughafen aus der Liste "FRA50.03,8.57;…" → IATA-Code
function naechsterFlughafen(lat, lon, liste) {
  var best = '', bestD = 1e9, teile = liste.split(';');
  for (var i = 0; i < teile.length; i++) {
    var t = teile[i], k = t.indexOf(',');
    var d = entfernungKm(lat, lon, parseFloat(t.substring(3, k)), parseFloat(t.substring(k + 1)));
    if (d < bestD) { bestD = d; best = t.substring(0, 3); }
  }
  return best;
}

// Sonnenauf- und -untergang (Sunrise equation, Refraktion -0,833°) für ein Kalenderdatum am Ort.
// Rückgabe: { auf, unter } als UTC-Millisekunden; null bei Polartag/-nacht.
function sonne(lat, lon, jahr, monat, tag) {
  var jd = Date.UTC(jahr, monat - 1, tag, 12) / 86400000 + 2440587.5;
  var n = Math.round(jd - 2451545.0 + 0.0008);
  var js = n - lon / 360;
  var m = (357.5291 + 0.98560028 * js) % 360;
  var c = 1.9148 * Math.sin(m * RAD) + 0.02 * Math.sin(2 * m * RAD) + 0.0003 * Math.sin(3 * m * RAD);
  var l = (m + c + 180 + 102.9372) % 360;
  var jt = 2451545.0 + js + 0.0053 * Math.sin(m * RAD) - 0.0069 * Math.sin(2 * l * RAD);
  var sd = Math.sin(l * RAD) * Math.sin(23.4397 * RAD), cd = Math.cos(Math.asin(sd));
  var cw = (Math.sin(-0.833 * RAD) - Math.sin(lat * RAD) * sd) / (Math.cos(lat * RAD) * cd);
  if (cw > 1 || cw < -1) return null;
  var w = Math.acos(cw) / RAD / 360;
  var ms = function (j) { return Math.round((j - 2440587.5) * 86400000); };
  return { auf: ms(jt - w), unter: ms(jt + w) };
}

// Minuten nach Mitternacht in der Zeitzone des Handys (= der Uhr); -1 wenn unbekannt
function ortsMinuten(ms) {
  if (ms === null || ms === undefined) return -1;
  var d = new Date(ms);
  return d.getHours() * 60 + d.getMinutes();
}

// Open-Meteo-Antwort → 144 Bytes: 48 h Temperatur (°C·2, int8), Regen (mm·10, uint8), Druck (hPa·2−1900, uint8)
// Fehlwerte: Temperatur -128, Regen und Druck 255
function wetterPaket(j) {
  var h = j.hourly, b = [];
  var i, v;
  for (i = 0; i < 48; i++) {
    v = h.temperature_2m[i];
    v = (v === null || v === undefined) ? -128 : Math.max(-127, Math.min(127, Math.round(v * 2)));
    b.push(v & 255);
  }
  for (i = 0; i < 48; i++) {
    v = h.precipitation[i];
    b.push((v === null || v === undefined) ? 255 : Math.max(0, Math.min(254, Math.round(v * 10))));
  }
  for (i = 0; i < 48; i++) {
    v = h.pressure_msl[i];
    b.push((v === null || v === undefined) ? 255 : Math.max(0, Math.min(254, Math.round(v * 2 - 1900))));
  }
  var t = h.time[0];   // "2026-10-07T00:00", Ortszeit des Standorts
  var tag = parseInt(t.substring(0, 4), 10) * 10000 + parseInt(t.substring(5, 7), 10) * 100 + parseInt(t.substring(8, 10), 10);
  return { bytes: b, tag: tag };
}

// Sonnenzeiten für den Datentag und den Folgetag → [auf1, unter1, auf2, unter2] als UTC-Sekunden, -1 = Polartag/-nacht.
// Die Uhr rechnet selbst in Ortszeit um (die Zeitzone der JS-Umgebung ist nicht verlässlich, im Emulator falsch).
function sonnenPaket(lat, lon, tag) {
  var j = Math.floor(tag / 10000), m = Math.floor(tag / 100) % 100, d = tag % 100;
  var erg = [];
  for (var k = 0; k < 2; k++) {
    var dt = new Date(Date.UTC(j, m - 1, d + k));
    var s = sonne(lat, lon, dt.getUTCFullYear(), dt.getUTCMonth() + 1, dt.getUTCDate());
    erg.push(s ? Math.round(s.auf / 1000) : -1, s ? Math.round(s.unter / 1000) : -1);
  }
  return erg;
}

// Anker für Sonne und Flughafen: nur bei großem Ortswechsel neu setzen
function ankerPruefen(anker, lat, lon, schwelleKm, liste) {
  if (anker && entfernungKm(anker.lat, anker.lon, lat, lon) <= schwelleKm) return { anker: anker, neu: false };
  return { anker: { lat: lat, lon: lon, iata: naechsterFlughafen(lat, lon, liste) }, neu: true };
}

function wetterUrl(lat, lon) {
  return 'https://api.open-meteo.com/v1/forecast?latitude=' + lat.toFixed(2) + '&longitude=' + lon.toFixed(2) +
    '&hourly=temperature_2m,precipitation,pressure_msl&timezone=auto&forecast_days=2';
}

// Gezeiten (Open-Meteo Marine, Modellwert, nicht für Navigation) → 48 Bytes: Wasserstand in 5 cm (int8), -128 = fehlt.
// Nur wenn die Antwort mit demselben Tag beginnt wie das Wetter; im Binnenland liefert die API nur null → alles -128.
function gezeitenPaket(j, tag) {
  var b = [], i, v, h = j && j.hourly;
  var gleich = h && h.time && h.time[0] && gezeitenTag(h.time[0]) === tag;
  for (i = 0; i < 48; i++) {
    v = gleich ? h.sea_level_height_msl[i] : null;
    v = (v === null || v === undefined) ? -128 : Math.max(-127, Math.min(127, Math.round(v * 20)));
    b.push(v & 255);
  }
  return b;
}
function gezeitenTag(t) { return parseInt(t.substring(0, 4), 10) * 10000 + parseInt(t.substring(5, 7), 10) * 100 + parseInt(t.substring(8, 10), 10); }
function hatGezeiten(b) { for (var i = 0; i < b.length; i++) if (b[i] !== 128) return true; return false; }

function gezeitenUrl(lat, lon) {
  return 'https://marine-api.open-meteo.com/v1/marine?latitude=' + lat.toFixed(2) + '&longitude=' + lon.toFixed(2) +
    '&hourly=sea_level_height_msl&timezone=auto&forecast_days=2&cell_selection=sea';
}

module.exports = { gezeitenPaket: gezeitenPaket, hatGezeiten: hatGezeiten, gezeitenUrl: gezeitenUrl, entfernungKm: entfernungKm, naechsterFlughafen: naechsterFlughafen, sonne: sonne,
  ortsMinuten: ortsMinuten, wetterPaket: wetterPaket, sonnenPaket: sonnenPaket, ankerPruefen: ankerPruefen,
  wetterUrl: wetterUrl };
