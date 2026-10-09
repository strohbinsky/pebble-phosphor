// Phosphor — Handy-Teil. Holt auf Anfrage der Uhr (höchstens stündlich) das Wetter und die Gezeiten für den aktuellen
// Standort und schickt beides zusammen mit Sonnenzeiten und Flughafen. Einstellungsseite: Sprache (Englisch/Deutsch), Ansicht (vier) und Parkplatz-Notiz. Sonne und Flughafen hängen an einem
// Anker-Standort, der nur bei einem Ortswechsel von mehr als 50 km neu gesetzt wird.
var logik = require('./logik');
var FLUGHAEFEN = require('./flughaefen');
var keys = require('message_keys');
var SCHWELLE_KM = 50;
var laeuft = false;

function log(t) { console.log('phosphor: ' + t); }

function senden(dict, versuch) {
  versuch = versuch || 0;
  Pebble.sendAppMessage(dict, function () {}, function () {
    if (versuch < 3) setTimeout(function () { senden(dict, versuch + 1); }, 2000 * (versuch + 1));
    else log('Senden gescheitert');
  });
}

function ankerLaden() {
  try { return JSON.parse(localStorage.getItem('anker')); } catch (e) { return null; }
}

function fehler(code, text) {
  laeuft = false;
  log(text);
  senden({ FEHLER: code });
}

function wetterHolen(lat, lon) {
  try {
    var a = logik.ankerPruefen(ankerLaden(), lat, lon, SCHWELLE_KM, FLUGHAEFEN);
    if (a.neu) { localStorage.setItem('anker', JSON.stringify(a.anker)); log('neuer Anker ' + a.anker.iata); }
    var anker = a.anker;
    var xhr = new XMLHttpRequest();
    xhr.open('GET', logik.wetterUrl(lat, lon));
    xhr.timeout = 30000;
    xhr.onload = function () {
      try {
        if (xhr.status !== 200) return fehler(4, 'HTTP ' + xhr.status);
        var p = logik.wetterPaket(JSON.parse(xhr.responseText));
        var s = logik.sonnenPaket(anker.lat, anker.lon, p.tag);
        var d = { WETTER: p.bytes, TAG: p.tag, STAND: Math.floor(Date.now() / 1000), IATA: anker.iata,
                  BREITE: Math.round(anker.lat * 100) };
        for (var i = 0; i < 4; i++) d[keys.SONNE + i] = s[i];
        gezeitenHolen(lat, lon, p.tag, function (g) {          // Gezeiten nachladen; Fehler = keine Gezeiten
          d.GEZEITEN = g;
          laeuft = false;
          senden(d);
          log('Wetter gesendet, Tag ' + p.tag + (logik.hatGezeiten(g) ? ', mit Gezeiten' : ', ohne Gezeiten'));
        });
      } catch (e) { fehler(3, 'Auswertung: ' + e); }
    };
    xhr.onerror = function () { fehler(2, 'Netz'); };
    xhr.ontimeout = function () { fehler(2, 'Zeitlimit'); };
    xhr.send();
  } catch (e) { fehler(3, 'Abruf: ' + e); }
}

function gezeitenHolen(lat, lon, tag, fertig) {
  var leer = logik.gezeitenPaket(null, tag), erledigt = false;
  function ende(g) { if (!erledigt) { erledigt = true; fertig(g); } }
  try {
    var xhr = new XMLHttpRequest();
    xhr.open('GET', logik.gezeitenUrl(lat, lon));
    xhr.timeout = 20000;
    xhr.onload = function () {
      try { ende(xhr.status === 200 ? logik.gezeitenPaket(JSON.parse(xhr.responseText), tag) : leer); } catch (e) { ende(leer); }
    };
    xhr.onerror = xhr.ontimeout = function () { ende(leer); };
    xhr.send();
  } catch (e) { ende(leer); }
}

function abrufen() {
  if (laeuft) return;
  laeuft = true;
  setTimeout(function () { laeuft = false; }, 90000);   // Sicherung, falls kein Rückruf kommt
  navigator.geolocation.getCurrentPosition(function (pos) {
    wetterHolen(pos.coords.latitude, pos.coords.longitude);
  }, function () {
    var a = ankerLaden();   // kein Standort: letzten Anker nehmen
    if (a) wetterHolen(a.lat, a.lon); else fehler(1, 'kein Standort');
  }, { enableHighAccuracy: false, maximumAge: 30 * 60 * 1000, timeout: 30000 });
}

// ---------- Einstellungen: Ansicht und Parkplatz-Notiz ----------
// Ansichten (Index = SKIN an der Uhr): Phosphor, Klar, seit 0.22 Phosphor hell (invers) und Klar dunkel (invers).
// Parkplatz (seit 0.22): zwei kurze Zeilen neben einem P-Symbol, „Parkplatz löschen“ leert beide und speichert sofort.
var SKINS = ['phosphor', 'klar', 'phosphor-hell', 'klar-dunkel'];
function skin() { var v = localStorage.getItem('skin'); return SKINS.indexOf(v) >= 0 ? v : 'phosphor'; }
function park(i) { return localStorage.getItem('park' + i) || ''; }
var PARK_MAX = 5;                                       // Zeichen je Zeile; die Uhr kennt Großbuchstaben, Ziffern, - . : +
function parkText(v) { return String(v || '').toUpperCase().replace(/[ÄÖÜ]/g, function (c) { return { 'Ä': 'AE', 'Ö': 'OE', 'Ü': 'UE' }[c]; }).replace(/[^A-Z0-9 .,:+\-%]/g, '').trim().substring(0, PARK_MAX); }

// Sprache (seit 0.23): Englisch Standard, Deutsch wählbar. Die Seite rendert sich selbst aus TEXTE neu, wenn man umschaltet.
var SPRACHEN = ['en', 'de'];                                 // Index = SPRACHE an der Uhr
function sprache() { var v = localStorage.getItem('sprache'); return SPRACHEN.indexOf(v) >= 0 ? v : 'en'; }

var TEXTE = {
  en: { titel: 'Language', ansicht: 'View', park: 'Parking note',
        parkHilfe: 'Two short lines next to a P at the top right, e.g. B3 / 42 or L2 / 117. Empty = nothing shown.',
        loeschen: 'Clear parking note', ok: 'Save',
        a: [['Phosphor', 'Radar green on black, pixel font'], ['Phosphor light', 'Black on white, pixel font — for daylight'],
            ['Clear', 'Black on white, system font, with tides on the coast'], ['Clear dark', 'White on black, curves in colour like Clear']] },
  de: { titel: 'Sprache', ansicht: 'Ansicht', park: 'Parkplatz',
        parkHilfe: 'Zwei kurze Zeilen neben einem P oben rechts, z. B. B3 / 42 oder L2 / 117. Leer = nichts sichtbar.',
        loeschen: 'Parkplatz löschen', ok: 'Speichern',
        a: [['Phosphor', 'Radar-Grün auf Schwarz, Pixelschrift'], ['Phosphor hell', 'Schwarz auf Weiß, Pixelschrift — für Tageslicht'],
            ['Klar', 'Schwarz auf Weiß, Systemschrift, mit Gezeiten an der Küste'], ['Klar dunkel', 'Weiß auf Schwarz, Kurven in Farbe wie Klar']] }
};
var FARBEN = { 'phosphor': ['#000', '#00FF00'], 'phosphor-hell': ['#fff', '#111'], 'klar': ['#fff', '#111'], 'klar-dunkel': ['#000', '#888'] };
var REIHE = ['phosphor', 'phosphor-hell', 'klar', 'klar-dunkel'];   // Reihenfolge auf der Seite, Texte in TEXTE.a gleich

var SEITE = '<!DOCTYPE html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">' +
  '<title>Phosphor</title><style>*{box-sizing:border-box}body{font-family:sans-serif;margin:0;padding:16px;background:#f4f4f4;color:#111}' +
  'h1{font-size:20px;margin:0 0 4px}h2{font-size:14px;text-transform:uppercase;letter-spacing:.05em;color:#666;margin:22px 0 8px}p{color:#555;font-size:14px;margin:0 0 12px}' +
  'label{display:flex;align-items:center;gap:12px;background:#fff;border-radius:10px;padding:14px;margin-bottom:10px;font-size:17px}' +
  'label small{display:block;color:#666;font-size:13px}input[type=radio]{width:22px;height:22px}.zwei{display:flex;gap:10px}.zwei label{flex:1}' +
  '.k{width:34px;height:34px;border-radius:6px;flex:none}.p{display:flex;gap:10px}.p input{flex:1;min-width:0;font-size:20px;padding:12px;border:1px solid #ccc;border-radius:10px;text-transform:uppercase}' +
  'button{width:100%;padding:14px;font-size:17px;border:0;border-radius:10px;background:#111;color:#fff;margin-top:10px}button.weg{background:#fff;color:#b00;border:1px solid #b00}</style></head>' +
  '<body><h1>Phosphor</h1><div id="seite"></div><script>var z=/*ZUSTAND*/null,T=/*TEXTE*/null,F=/*FARBEN*/null,R=/*REIHE*/null;' +
  'function wert(id){var e=document.getElementById(id);return e?e.value:"";}' +
  'function zeigen(){var t=T[z.lang],h="<h2>"+t.titel+"</h2><div class=zwei>";' +
  '[["en","English"],["de","Deutsch"]].forEach(function(l){h+="<label><input type=radio name=l value="+l[0]+(z.lang==l[0]?" checked":"")+">"+l[1]+"</label>";});' +
  'h+="</div><h2>"+t.ansicht+"</h2>";R.forEach(function(k,i){h+="<label><input type=radio name=s value="+k+(z.skin==k?" checked":"")+"><span class=k style=\'background:"+F[k][0]+";border:3px solid "+F[k][1]+"\'></span><span>"+t.a[i][0]+"<small>"+t.a[i][1]+"</small></span></label>";});' +
  'h+="<h2>"+t.park+"</h2><p>"+t.parkHilfe+"</p><div class=p><input id=p1 maxlength=5 placeholder=B3><input id=p2 maxlength=5 placeholder=42></div>"+' +
  '"<button class=weg id=weg>"+t.loeschen+"</button><button id=ok>"+t.ok+"</button>";' +
  'document.getElementById("seite").innerHTML=h;document.getElementById("p1").value=z.p1;document.getElementById("p2").value=z.p2;' +
  'document.querySelectorAll("input[name=l]").forEach(function(e){e.onchange=function(){z.lang=e.value;z.p1=wert("p1");z.p2=wert("p2");z.skin=document.querySelector("input[name=s]:checked").value;zeigen();};});' +
  'document.getElementById("ok").onclick=function(){zu(wert("p1"),wert("p2"));};document.getElementById("weg").onclick=function(){zu("","");};document.documentElement.lang=z.lang;}' +
  'function zu(p1,p2){var v=document.querySelector("input[name=s]:checked").value;' +
  'document.location="pebblejs://close#"+encodeURIComponent(JSON.stringify({skin:v,park1:p1,park2:p2,lang:z.lang}));}zeigen();</script></body></html>';

Pebble.addEventListener('showConfiguration', function () {
  var ers = { '/*ZUSTAND*/null': JSON.stringify({ skin: skin(), p1: park(1), p2: park(2), lang: sprache() }), '/*TEXTE*/null': JSON.stringify(TEXTE),
              '/*FARBEN*/null': JSON.stringify(FARBEN), '/*REIHE*/null': JSON.stringify(REIHE) };
  var html = SEITE;
  Object.keys(ers).forEach(function (k) { html = html.replace(k, function () { return ers[k]; }); });
  Pebble.openURL('data:text/html;charset=utf-8,' + encodeURIComponent(html));
});

Pebble.addEventListener('webviewclosed', function (e) {
  if (!e || !e.response) return;
  try {
    var d = JSON.parse(decodeURIComponent(e.response)), msg = {};
    if (SKINS.indexOf(d.skin) >= 0) { localStorage.setItem('skin', d.skin); msg.SKIN = SKINS.indexOf(d.skin); }
    if (SPRACHEN.indexOf(d.lang) >= 0) { localStorage.setItem('sprache', d.lang); msg.SPRACHE = SPRACHEN.indexOf(d.lang); }
    if (d.park1 !== undefined) {
      var p1 = parkText(d.park1), p2 = parkText(d.park2);
      localStorage.setItem('park1', p1); localStorage.setItem('park2', p2);
      msg.PARK1 = p1; msg.PARK2 = p2;                    // leer = Notiz auf der Uhr löschen
    }
    senden(msg);
    log('Einstellungen: Ansicht ' + d.skin + ', Parkplatz ' + (msg.PARK1 || msg.PARK2 ? 'gesetzt' : 'leer'));
  } catch (err) { log('Einstellungen: ' + err); }
});

Pebble.addEventListener('ready', function () { senden({ BEREIT: 1, SPRACHE: SPRACHEN.indexOf(sprache()) }); });   // Sprache gleich mitschicken: Standard Englisch auch ohne Einstellungsseite
Pebble.addEventListener('appmessage', function (e) { if (e.payload.ANFRAGE !== undefined) abrufen(); });
