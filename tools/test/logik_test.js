// Prüft logik.js gegen Open-Meteo: Sonnenzeiten (Gegenprobe), Wetterpaket, Anker und Flughafen.
// Aufruf: node tools/test/logik_test.js   (braucht Netz)
var L = require('../../src/pkjs/logik');
var FLUG = require('../../src/pkjs/flughaefen');
var fehler = 0;
function pruef(ok, text) { console.log((ok ? 'ok     ' : 'FEHLER ') + text); if (!ok) fehler++; }

var orte = [['Frankfurt', 50.03, 8.57], ['Sydney', -33.87, 151.2], ['Reykjavik', 64.13, -21.9],
            ['Kathmandu', 27.7, 85.3], ['Honolulu', 21.3, -157.86], ['Tromsø', 69.65, 18.96]];
(async function () {
  for (var o of orte) {
    var r = await fetch('https://api.open-meteo.com/v1/forecast?latitude=' + o[1] + '&longitude=' + o[2] +
      '&daily=sunrise,sunset&timezone=GMT&start_date=2026-10-09&end_date=2026-10-09').then(x => x.json())
      .catch(() => null);
    if (!r || !r.daily) { pruef(false, o[0] + ': keine Antwort'); continue; }
    var s = L.sonne(o[1], o[2], 2026, 10, 9);
    var auf = Date.parse(r.daily.sunrise[0] + 'Z'), unter = Date.parse(r.daily.sunset[0] + 'Z');
    if (!s) { pruef(isNaN(auf) || r.daily.sunrise[0] === r.daily.sunset[0], o[0] + ': Polarnacht, Open-Meteo ' + r.daily.sunrise[0]); continue; }
    var da = Math.abs(s.auf - auf) / 60000, du = Math.abs(s.unter - unter) / 60000;
    var tol = Math.abs(o[1]) > 60 ? 5 : 2;   // flacher Sonnenstand: kleine Rechenunterschiede = Minuten
    pruef(da <= tol && du <= tol, o[0] + ' 09.10.: Abweichung auf ' + da.toFixed(1) + ' min, unter ' + du.toFixed(1) + ' min');
  }
  var w = await fetch(L.wetterUrl(50.03, 8.57)).then(x => x.json());
  var p = L.wetterPaket(w);
  pruef(p.bytes.length === 144 && p.bytes.every(b => b >= 0 && b <= 255), 'Wetterpaket 144 Bytes, Tag ' + p.tag);
  var t0 = p.bytes[12] > 127 ? p.bytes[12] - 256 : p.bytes[12];
  pruef(Math.abs(t0 / 2 - w.hourly.temperature_2m[12]) <= 0.25, 'Temperatur 12 Uhr ' + t0 / 2 + ' = ' + w.hourly.temperature_2m[12]);
  pruef(Math.abs((p.bytes[96 + 12] + 1900) / 2 - w.hourly.pressure_msl[12]) <= 0.25, 'Druck 12 Uhr ' + (p.bytes[108] + 1900) / 2);
  // Gezeiten: Küste mit Werten, Binnenland ohne, Tag muss zum Wetter passen
  var gk = await fetch(L.gezeitenUrl(53.87, 8.70)).then(x => x.json());
  var wk = L.wetterPaket(await fetch(L.wetterUrl(53.87, 8.70)).then(x => x.json()));
  var pk = L.gezeitenPaket(gk, wk.tag);
  var v12 = pk[12] > 127 ? pk[12] - 256 : pk[12];
  pruef(pk.length === 48 && L.hatGezeiten(pk), 'Gezeiten Cuxhaven 48 Bytes, mit Werten');
  pruef(Math.abs(v12 / 20 - gk.hourly.sea_level_height_msl[12]) <= 0.026, 'Wasserstand 12 Uhr ' + v12 / 20 + ' m = ' + gk.hourly.sea_level_height_msl[12]);
  var gf = await fetch(L.gezeitenUrl(50.03, 8.57)).then(x => x.json());
  pruef(!L.hatGezeiten(L.gezeitenPaket(gf, p.tag)), 'Gezeiten Frankfurt: keine (Binnenland)');
  pruef(!L.hatGezeiten(L.gezeitenPaket(gk, wk.tag + 1)), 'Gezeiten mit falschem Tag verworfen');
  pruef(!L.hatGezeiten(L.gezeitenPaket(null, wk.tag)), 'Gezeiten ohne Antwort: leer');
  var a1 = L.ankerPruefen(null, 50.0, 8.27, 50, FLUG);
  pruef(a1.neu && a1.anker.iata === 'FRA', 'Anker Mainz → ' + a1.anker.iata);
  var a2 = L.ankerPruefen(a1.anker, 50.1, 8.6, 50, FLUG);
  pruef(!a2.neu, 'Nachbarort (25 km) behält Anker');
  var a3 = L.ankerPruefen(a1.anker, 47.56, 7.59, 50, FLUG);
  pruef(a3.neu && a3.anker.iata === 'BSL', 'Basel → ' + a3.anker.iata);
  var sp = L.sonnenPaket(50.03, 8.57, 20261007);
  pruef(sp.length === 4 && sp[1] - sp[0] > 36000 && Math.abs(sp[2] - sp[0] - 86400) < 300, 'Sonnenpaket UTC ' + sp.map(t => new Date(t * 1000).toISOString().substring(11, 16)).join(' '));
  console.log(fehler ? fehler + ' Fehler' : 'alles ok');
  process.exit(fehler ? 1 : 0);
})();
