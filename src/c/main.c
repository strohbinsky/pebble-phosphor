// Phosphor — Watchface für die Pebble Time 2 (emery, 200 x 228). Entwurf: phosphor-entwuerfe-v17.svg.
// Radar-Grün auf Schwarz, eigene Pixelschriften (schrift.h). Jede Minute ein kompletter Neuaufbau.
// Uhr: Ortszeit, UTC, Datum, Zeitzone, Mond, Schritte, Akku, Pulsverlauf 6 h — alles ohne Handy.
// Handy (höchstens stündlich, auf Anfrage der Uhr): Wetter und Gezeiten 48 h, Sonnenzeiten, IATA-Code, Breitengrad.
// Zwei Ansichten, umschaltbar in der Einstellungsseite: Phosphor (hier) und Klar (klar.h, Systemschrift, mit Gezeiten).
#include <pebble.h>
#include "schrift.h"

// Farben je Ansicht: Phosphor (Grün auf Schwarz) oder seit 0.22 „Phosphor hell“ (Schwarz auf Weiß, P_HELL)
#define P_HELL     (s_skin == SKIN_PHOSPHOR_HELL)
#define PF(dunkel, hell) (P_HELL ? GColorFromHEX(hell) : GColorFromHEX(dunkel))
#define C_HG       PF(0x000000, 0xFFFFFF)   // Hintergrund
#define C_TEXT     PF(0x00FF00, 0x000000)   // seit 0.14 reines Grün: 55FF55 wirkte gräulich
#define C_GRAU     PF(0x00AA00, 0x555555)   // nur noch Linien und Rahmen, Text immer C_TEXT
#define C_GITTER   PF(0x005500, 0xAAAAAA)
#define C_REGEN    PF(0x00AA55, 0x00AA55)
#define C_TEMP     PF(0x00FF00, 0x005500)   // hell: Dunkelgrün, bleibt als Phosphor-Kurve erkennbar
#define C_ZIEL     PF(0xAAFFAA, 0x00AA00)   // Schrittziel erreicht
#define C_NACHT_GITTER PF(0x00AA00, 0x555555)   // Gitter im Nachtraster, sonst darin unsichtbar
#define C_DRUCK    PF(0x00FFFF, 0x0055AA)   // hell: Cyan auf Weiß unlesbar, daher Blau
#define C_JETZT    PF(0xAAAAAA, 0x555555)
#define C_WARN     PF(0xFFFF00, 0xFF5500)
#define C_WARN_AUS PF(0x555500, 0xFFAA55)

#define AKKU_WARN     25      // Prozent: darunter wird der Akkubalken gelb
#define SCHRITT_ZIEL  10000
#define ABRUF_S       3600    // Wetter höchstens stündlich
#define WIEDERHOLEN_S 60      // seit 0.24: überfällig und ohne Antwort/ohne Netz jede Minute neu fragen — Netz wieder da → binnen 1 min
#define SERVER_S      600     // Open-Meteo antwortet mit Fehler: erst nach 10 min wieder, den Dienst nicht belasten
#define PULS_MIN      360     // Pulsverlauf über 6 Stunden
#define PULS_ALLE_S   600     // Pulsverlauf alle 10 min neu aus der Minuten-Historie
#define PULS_SP       28      // Spalten der Pulsgrafik (2-px-Blöcke)
#define WETTER_LEN    144     // 48 h × Temperatur, Regen, Druck
#define SYNODISCH     2551443 // mittlerer Mondumlauf in Sekunden
#define NEUMOND_REF   947182440 // Neumond 06.01.2000 18:14 UTC

// Diagramm
#define D_Y0 142
#define D_H  54

enum { PK_WETTER = 1, PK_TAG, PK_STAND, PK_SONNE, PK_IATA, PK_BREITE, PK_FORMAT, PK_GEZEITEN, PK_SKIN, PK_PARK1, PK_PARK2, PK_SPRACHE };
enum { SKIN_PHOSPHOR = 0, SKIN_KLAR = 1, SKIN_PHOSPHOR_HELL = 2, SKIN_KLAR_DUNKEL = 3 };   // 2, 3 seit 0.22
#define SKIN_IST_KLAR (s_skin == SKIN_KLAR || s_skin == SKIN_KLAR_DUNKEL)
#define FORMAT 3          // Speicherformat; bei Änderung hochzählen, dann verwirft die Uhr alte Daten

static Window *s_window;
static Layer *s_layer;
static GContext *g;

static uint8_t s_wetter[WETTER_LEN];
static int32_t s_tag;                 // Datentag JJJJMMTT (Stunde 0 = 0 Uhr dieses Tages)
static int32_t s_stand;               // Abrufzeit (UTC)
static int32_t s_sonne[4] = { -1, -1, -1, -1 };  // Auf/Unter Datentag, Auf/Unter Folgetag, UTC-Sekunden
static char s_iata[4];
static int32_t s_breite = 5000;       // Breitengrad × 100 (Südhalbkugel: Mond gespiegelt)
static bool s_hat_wetter;
static int8_t s_gezeiten[48];          // Wasserstand in 5 cm, -128 = fehlt; gleiche Stunden wie s_wetter
static int32_t s_skin = SKIN_PHOSPHOR;
// Sprache (seit 0.23): Englisch Standard, Deutsch wählbar in den Einstellungen. Betrifft Wochentage, Mond, Zonenkürzel
static bool s_de;
#define TXT(en, de) (s_de ? (de) : (en))
static const char *const WT_EN[] = { "SU", "MO", "TU", "WE", "TH", "FR", "SA" };
static const char *const WT_DE[] = { "SO", "MO", "DI", "MI", "DO", "FR", "SA" };
#define WT (s_de ? WT_DE : WT_EN)
static char s_park[2][8];            // Parkplatz-Notiz (seit 0.22), zwei Zeilen, leer = keine Anzeige
// Sekunden (seit 0.20, Wunsch Seb 2026-10-09): erst nach einer deutlichen Handgelenkbewegung (Tap/Schütteln der Uhr),
// dann SEK_DAUER_MS lang, danach wieder nur Minutentakt — schont den Akku
#define SEK_DAUER_MS 15000                     // 0.20: 30 s, seit 0.21 15 s
static bool s_sekunden;
static AppTimer *s_sek_timer;
static time_t s_letzte_anfrage;
static int s_warte_s = WIEDERHOLEN_S;  // Abstand bis zur nächsten Anfrage, solange Wetter überfällig ist
static int s_fehler;                   // letzter Fehler vom Handy (1 Standort, 2 Netz, 3 Auswertung, 4 Server), 0 = keiner

static HealthMinuteData s_minuten[PULS_MIN];   // 12 B je Minute → gut 4 KB
static int16_t s_puls[PULS_SP];               // Mittel je Spalte, 0 = keine Daten
static int s_puls_min, s_puls_max;
static time_t s_puls_stand;

// ---------- Zeichnen: Grundlagen ----------
static void flaeche(int x, int y, int w, int h, GColor c) {
  graphics_context_set_fill_color(g, c);
  graphics_fill_rect(g, GRect(x, y, w, h), 0, GCornerNone);
}

// Ohne Wetterdaten für heute (seit 0.24): leeres Diagramm mit Gitter und Jetzt-Strich, darin ein Hinweis, warum
static const char *hinweis_ohne_wetter(void) {
  if (!connection_service_peek_pebble_app_connection()) return TXT("NO PHONE", "KEIN HANDY");
  if (s_fehler == 2) return TXT("NO INTERNET", "KEIN INTERNET");
  if (s_fehler == 1) return TXT("NO LOCATION", "KEIN STANDORT");
  return TXT("WAITING FOR WEATHER", "WARTE AUF WETTER");
}

static const Glyph *glyph(const Glyph *f, int n, char c) {
  for (int i = 0; i < n; i++) if (f[i].c == c) return &f[i];
  return NULL;
}

static int text_w(const Glyph *f, int n, const char *s, int sc) {
  int w = 0;
  for (; *s; s++) { const Glyph *gl = glyph(f, n, *s); w += ((gl ? gl->w : 2) + 1) * sc; }
  return w > 0 ? w - sc : 0;
}

// Pixeltext; zusammenhängende Punkte einer Zeile werden als ein Rechteck gezeichnet
static void text(const Glyph *f, int n, int h, const char *s, int x, int y, int sc, GColor c) {
  graphics_context_set_fill_color(g, c);
  for (; *s; s++) {
    const Glyph *gl = glyph(f, n, *s);
    if (!gl) { x += 3 * sc; continue; }
    for (int gy = 0; gy < h; gy++) {
      int lauf = -1;
      for (int gx = 0; gx <= gl->w; gx++) {
        const bool an = gx < gl->w && (gl->rows[gy] & (1 << (gl->w - 1 - gx)));
        if (an && lauf < 0) lauf = gx;
        if (!an && lauf >= 0) {
          graphics_fill_rect(g, GRect(x + lauf * sc, y + gy * sc, (gx - lauf) * sc, sc), 0, GCornerNone);
          lauf = -1;
        }
      }
    }
    x += (gl->w + 1) * sc;
  }
}

enum { LINKS, MITTE, RECHTS };

// Pixeltext schmal: Punkt sx breit, sy hoch, „luecke“ px zwischen den Zeichen (Kopf seit 0.18: 2 × 3, Lücke 2)
static void text_xy(const Glyph *f, int n, int h, const char *s, int x, int y, int sx, int sy, int luecke, GColor c) {
  graphics_context_set_fill_color(g, c);
  for (; *s; s++) {
    const Glyph *gl = glyph(f, n, *s);
    if (!gl) { x += 3 * sx; continue; }
    for (int gy = 0; gy < h; gy++) {
      int lauf = -1;
      for (int gx = 0; gx <= gl->w; gx++) {
        const bool an = gx < gl->w && (gl->rows[gy] & (1 << (gl->w - 1 - gx)));
        if (an && lauf < 0) lauf = gx;
        if (!an && lauf >= 0) { graphics_fill_rect(g, GRect(x + lauf * sx, y + gy * sy, (gx - lauf) * sx, sy), 0, GCornerNone); lauf = -1; }
      }
    }
    x += gl->w * sx + luecke;
  }
}
static int text_xy_w(const Glyph *f, int n, const char *s, int sx, int luecke) {
  int w = 0;
  for (; *s; s++) { const Glyph *gl = glyph(f, n, *s); w += (gl ? gl->w : 2) * sx + luecke; }
  return w > 0 ? w - luecke : 0;
}
static void klein(const char *s, int x, int y, int ausr, GColor c) {   // 3x5, doppelt
  const int w = text_w(F35, F35_N, s, 2);
  text(F35, F35_N, 5, s, ausr == MITTE ? x - w / 2 : ausr == RECHTS ? x - w : x, y, 2, c);
}
static void gross(const char *s, int x, int y, int sc, int ausr, GColor c) {   // 5x7
  const int w = text_w(F57, F57_N, s, sc);
  text(F57, F57_N, 7, s, ausr == MITTE ? x - w / 2 : ausr == RECHTS ? x - w : x, y, sc, c);
}

// Pixeltext 3 × 5 (doppelt) auf genau „breite“ px gesperrt; mehr als 3 px je Lücke oder zu breit: zentriert
static void klein_breit(const char *s, int x0, int breite, int y, GColor c) {
  const int n = strlen(s), luecken = n > 1 ? n - 1 : 1, rest = breite - text_w(F35, F35_N, s, 2);
  if (rest < 0 || rest > 3 * luecken) { klein(s, x0 + breite / 2, y, MITTE, c); return; }
  char z[2] = { 0, 0 };
  int x = x0;
  for (int i = 0; i < n; i++) {
    z[0] = s[i];
    klein(z, x, y, LINKS, c);
    x += text_w(F35, F35_N, z, 2) + 2 + rest * (i + 1) / luecken - rest * i / luecken;
  }
}

// „UTC+2“, „UTC+5:45“, „UTC-3“
static void utc_versatz(int off, char *aus, int n) {
  const int ab = off < 0 ? -off : off;
  if (ab % 3600) snprintf(aus, n, "UTC%c%d:%02d", off < 0 ? '-' : '+', ab / 3600, ab % 3600 / 60);
  else snprintf(aus, n, "UTC%c%d", off < 0 ? '-' : '+', ab / 3600);
}

// Deutsches Zonenkürzel aus tm_zone der Uhr (die PT2 meldet CEDT statt CEST, der Emulator nichts). Sonst das Kürzel
// der Uhr (nur Großbuchstaben, bis 5 Zeichen), sonst leer
static void zonen_kuerzel(const struct tm *t, char *aus, int n) {
  // Spalten: Kürzel der Uhr, Englisch, Deutsch. Die PT2 meldet die Sommerzeit als „…DT“ statt „…ST“
  static const char *TAB[][3] = { {"CET", "CET", "MEZ"}, {"CEST", "CEST", "MESZ"}, {"CEDT", "CEST", "MESZ"}, {"WET", "WET", "WEZ"},
                                  {"WEST", "WEST", "WESZ"}, {"WEDT", "WEST", "WESZ"}, {"EET", "EET", "OEZ"}, {"EEST", "EEST", "OESZ"},
                                  {"EEDT", "EEST", "OESZ"} };
  const char *z = t->tm_zone;
  aus[0] = '\0';
  if (!z || !z[0]) return;
  for (unsigned i = 0; i < sizeof TAB / sizeof TAB[0]; i++) if (!strcmp(z, TAB[i][0])) { snprintf(aus, n, "%s", TAB[i][s_de ? 2 : 1]); return; }
  for (const char *p = z; *p; p++) if (*p < 'A' || *p > 'Z') return;
  if (strlen(z) <= 5) snprintf(aus, n, "%s", z);
}

static int begrenzen(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

static int isqrt(int v) {
  if (v <= 0) return 0;
  int r = 0;
  while ((r + 1) * (r + 1) <= v) r++;
  return r;
}

// ---------- Tage ----------
static int32_t tag_von(const struct tm *t) { return (t->tm_year + 1900) * 10000 + (t->tm_mon + 1) * 100 + t->tm_mday; }

static int32_t folgetag(int32_t tag) {
  struct tm t = { 0 };
  t.tm_year = tag / 10000 - 1900; t.tm_mon = tag / 100 % 100 - 1; t.tm_mday = tag % 100 + 1; t.tm_hour = 12;
  time_t z = mktime(&t);
  return tag_von(localtime(&z));
}

// Stundenversatz von heute im Wetterpaket: 0 (Datentag), 24 (Folgetag) oder -1 (keine Daten für heute)
static int heute_versatz(void) {
  if (!s_hat_wetter) return -1;
  time_t now = time(NULL);
  const int32_t heute = tag_von(localtime(&now));
  if (heute == s_tag) return 0;
  if (heute == folgetag(s_tag)) return 24;
  return -1;
}

// ---------- Kopfzeile: Schritte ----------
static void kopf(void) {
  char buf[16];
  int st = (int)health_service_sum_today(HealthMetricStepCount);
  if (st < 0) st = 0;
  if (st >= 1000) snprintf(buf, sizeof buf, "%d.%03d", st / 1000, st % 1000);
  else snprintf(buf, sizeof buf, "%d", st);
  klein(buf, 6, 2, LINKS, C_TEXT);
  const GColor an = st >= SCHRITT_ZIEL ? C_ZIEL : C_TEXT;   // Ziel erreicht: ganzer Balken blassgrün
  for (int i = 0; i < 10; i++) flaeche(68 + i * 13, 3, 11, 8, i < st / 1000 ? an : C_GITTER);
}

// ---------- Akku: Prozentzahl über senkrechtem Symbol, 5 Segmente, Pol oben ----------
// Höhe y0..y0+39 = Mondspitze bis Unterkante ABN. Zahl 10 px, Symbol 27 px (Pol 2, Segmente 5 × 4)
static void akku_zeichnen(int x, int y0) {
  const BatteryChargeState b = battery_state_service_peek();
  const bool warn = b.charge_percent < AKKU_WARN;
  const GColor an = warn ? C_WARN : C_TEXT, aus = warn ? C_WARN_AUS : C_GITTER;
  char buf[8];
  snprintf(buf, sizeof buf, "%d", b.charge_percent);
  const int w = text_w(F35, F35_N, buf, 2);
  int zx = x + 5 - w / 2;                                           // mittig über dem Symbol,
  if (zx + w > 198) zx = 198 - w;                                   // „100“ rechtsbündig
  klein(buf, zx, y0, LINKS, an);
  const int voll = (b.charge_percent + 10) / 20;
  flaeche(x + 3, y0 + 13, 4, 2, voll >= 5 ? an : aus);              // Pol
  for (int i = 0; i < 5; i++)                                       // i = 0 unten
    flaeche(x, y0 + 36 - i * 5, 10, 4, i < voll ? an : aus);
}

// ---------- Mond: schematische Kugel, Phase pixelweise (seit 0.14; vorher Fototextur) ----------
// Für beide Ansichten: Tagseite „licht“, Nachtseite „schatten“, Rand 1 px in „rand“ (nur wenn mit_rand).
static void mond_kugel(int cx, int cy, int r, GColor licht, GColor schatten, bool mit_rand, GColor rand) {
  const time_t now = time(NULL);
  int64_t pos = ((int64_t)now - NEUMOND_REF) % SYNODISCH;
  if (pos < 0) pos += SYNODISCH;
  const int32_t k = cos_lookup((int32_t)(pos * TRIG_MAX_ANGLE / SYNODISCH));
  const bool zun = pos < SYNODISCH / 2;
  const bool sued = s_breite < 0;                       // Südhalbkugel: Mond steht auf dem Kopf
  const int d = 2 * r + 1, R = d * d;
  for (int j = 0; j < d; j++) {
    const int dy2 = 2 * j - (d - 1);
    const int w2 = isqrt(R - dy2 * dy2);                // halbe Zeilenbreite × 2
    for (int i = 0; i < d; i++) {
      const int dx2 = 2 * i - (d - 1), q = dx2 * dx2 + dy2 * dy2;
      if (q > R) continue;
      if (mit_rand && q > (d - 3) * (d - 3)) { flaeche(cx - r + i, cy - r + j, 1, 1, rand); continue; }
      int32_t xn = w2 > 0 ? dx2 * TRIG_MAX_RATIO / w2 : 0;   // -1 … 1 in der Zeile
      if (sued) xn = -xn;
      const bool hell = zun ? xn >= k : xn <= -k;
      flaeche(cx - r + i, cy - r + j, 1, 1, hell ? licht : schatten);
    }
  }
}
static bool mond_zunehmend(void) {
  int64_t pos = ((int64_t)time(NULL) - NEUMOND_REF) % SYNODISCH;
  if (pos < 0) pos += SYNODISCH;
  return pos < SYNODISCH / 2;
}
static void mond(int cx, int cy, int r) {
  if (P_HELL) mond_kugel(cx, cy, r, GColorWhite, GColorBlack, true, GColorBlack);   // hell: Vollmond weiß mit Ring, Neumond schwarz
  else mond_kugel(cx, cy, r, C_TEXT, C_GITTER, false, C_GITTER);
  klein(mond_zunehmend() ? TXT("WAX", "ZUN") : TXT("WAN", "ABN"), cx, cy + r + 4, MITTE, C_TEXT);
}

// ---------- Puls ----------
static void puls_laden(void) {
  time_t ende = time(NULL), start = ende - PULS_MIN * 60;
  const time_t von = start;
  for (int i = 0; i < PULS_SP; i++) s_puls[i] = 0;
  s_puls_min = 999; s_puls_max = 0;
  s_puls_stand = ende;
  const uint32_t n = health_service_get_minute_history(s_minuten, PULS_MIN, &start, &ende);
  int32_t summe[PULS_SP] = { 0 };
  int16_t anz[PULS_SP] = { 0 };
  for (uint32_t i = 0; i < n; i++) {
    const HealthMinuteData *m = &s_minuten[i];
    if (m->is_invalid || m->heart_rate_bpm == 0) continue;
    const int32_t sek = (int32_t)(start - von) + (int32_t)i * 60;
    const int sp = begrenzen(sek * PULS_SP / (PULS_MIN * 60), 0, PULS_SP - 1);
    summe[sp] += m->heart_rate_bpm; anz[sp]++;
    if (m->heart_rate_bpm < s_puls_min) s_puls_min = m->heart_rate_bpm;
    if (m->heart_rate_bpm > s_puls_max) s_puls_max = m->heart_rate_bpm;
  }
  for (int i = 0; i < PULS_SP; i++) if (anz[i]) s_puls[i] = (int16_t)((summe[i] + anz[i] / 2) / anz[i]);
  // Die Uhr misst im Systemtakt etwa alle 10 min, eine Spalte umfasst knapp 13 min: einzelne leere Spalten
  // mit dem Vorwert füllen, damit die Linie nicht abreißt. Längere Lücken (Uhr abgelegt) bleiben sichtbar.
  for (int i = 1, leer = 0; i < PULS_SP; i++) {
    if (s_puls[i]) { leer = 0; continue; }
    if (s_puls[i - 1] && ++leer <= 2) s_puls[i] = s_puls[i - 1];
  }
}

static void puls_zeichnen(void) {
  const int x1 = 76, x2 = 131, y0 = 113, h = 18, B = 2, stufen = h / B;
  flaeche(x1 - 3, y0 - 3, x2 - x1 + 7, 1, C_GRAU);
  flaeche(x1 - 3, y0 + h + 3, x2 - x1 + 7, 1, C_GRAU);
  flaeche(x1 - 3, y0 - 3, 1, h + 7, C_GRAU);
  flaeche(x2 + 3, y0 - 3, 1, h + 7, C_GRAU);
  int lo = 999, hi = 0;
  for (int i = 0; i < PULS_SP; i++) if (s_puls[i]) { if (s_puls[i] < lo) lo = s_puls[i]; if (s_puls[i] > hi) hi = s_puls[i]; }
  char buf[16];
  if (hi == 0) {
    klein("MAX --", 196, 112, RECHTS, C_TEXT);
    klein("MIN --", 196, 124, RECHTS, C_TEXT);
    return;
  }
  lo -= 3;
  if (hi < lo + 40) hi = lo + 40;
  int vor = -1;
  for (int i = 0; i < PULS_SP; i++) {
    if (!s_puls[i]) { vor = -1; continue; }
    const int st = begrenzen(((s_puls[i] - lo) * (stufen - 1) * 2 + (hi - lo)) / (2 * (hi - lo)), 0, stufen - 1);
    const int a = vor < 0 ? st : (vor < st ? vor : st), e = vor < 0 ? st : (vor > st ? vor : st);
    for (int k = a; k <= e; k++) flaeche(x1 + i * B, y0 + h - (k + 1) * B, B, B, C_TEXT);
    vor = st;
  }
  snprintf(buf, sizeof buf, "MAX %d", s_puls_max);
  klein(buf, 196, 112, RECHTS, C_TEXT);
  snprintf(buf, sizeof buf, "MIN %d", s_puls_min);
  klein(buf, 196, 124, RECHTS, C_TEXT);
}

// ---------- Diagramm ----------
static int dx(int minuten) { return minuten * 200 / 1440; }          // Minuten nach Mitternacht → x

static int ortsminuten(int32_t utc) {                               // UTC-Sekunden → Minuten nach Mitternacht, -1 = keine
  if (utc < 0) return -1;
  const time_t t = utc;
  const struct tm *l = localtime(&t);
  return l->tm_hour * 60 + l->tm_min;
}

static int temp_wert(int v) { return (int8_t)v; }   // Byte → halbe Grad; -128 = fehlt

// Parkplatz-Notiz (seit 0.22, Wunsch Seb 2026-10-09): rechts oben im freien Feld über Mond und Akku, neben dem Datum.
// Kästchen mit „P“, daneben zwei kurze Zeilen (z. B. „P43“ / „2-3“), zusammen so hoch wie das Kästchen (21 px).
// Rechtsbündig an x = 197. Beide Zeilen leer: nichts. Eingabe und Löschen über die Einstellungsseite am Handy
static int park_text_w(bool klar, const char *s);
static void park_text(bool klar, const char *s, int x, int y, GColor c);
static void park_zeichnen(bool klar) {
  if (!s_park[0][0] && !s_park[1][0]) return;
  const int oben = klar ? 24 : 22, seite = 21, rechts = 197;
  const int w = park_text_w(klar, s_park[0]) > park_text_w(klar, s_park[1]) ? park_text_w(klar, s_park[0]) : park_text_w(klar, s_park[1]);
  const int x0 = rechts - w - 4 - seite;
  const GColor c = klar ? (s_skin == SKIN_KLAR_DUNKEL ? GColorWhite : GColorBlack) : C_TEXT;
  graphics_context_set_stroke_color(g, c);
  graphics_draw_round_rect(g, GRect(x0, oben, seite, seite), 3);
  park_text(klar, "P", -(x0 + seite / 2), oben + (klar ? 4 : 3), c);       // negatives x = mittig um -x
  park_text(klar, s_park[0], x0 + seite + 4, oben, c);
  park_text(klar, s_park[1], x0 + seite + 4, oben + 11, c);
}

#include "klar.h"   // Ansicht Klar; liefert auch die Gezeiten-Rechnung (k_tiden) und das Dreieck für beide Ansichten
static int park_text_w(bool klar, const char *s) { return klar ? k_text_w(KG14, s) : text_w(F35, F35_N, s, 2); }
static void park_text(bool klar, const char *s, int x, int y, GColor c) {
  const bool mitte = x < 0;
  if (mitte) x = -x;
  if (klar) k_text(s[0] == 'P' && !s[1] && mitte ? KG18 : KG14, s, x, y, mitte ? MITTE : LINKS, c);
  else if (mitte) text(F35, F35_N, 5, s, x - text_w(F35, F35_N, s, 3) / 2, y, 3, c);   // „P“ dreifach: 15 px hoch
  else klein(s, x, y, LINKS, c);
}

#define C_TIDE PF(0x55AAFF, 0x0055FF)

// Gezeiten: 1-px-Linie ohne Kantenglättung, Dreiecke an Hoch- und Niedrigwasser
static void tide_kurve(int versatz, int y0, int h) {
  int lo = 999, hi = -999;
  for (int st = 0; st <= 24; st++) {
    const int v = s_gezeiten[versatz + st > 47 ? 47 : versatz + st];
    if (v == K_FEHLT) continue;
    if (v < lo) lo = v;
    if (v > hi) hi = v;
  }
  if (hi < lo) return;
  if (hi - lo < 4) hi = lo + 4;
  const int oben = y0 + 8, unten = y0 + h - 5;
  #define TIY(v100) (unten - ((v100) - lo * 100) * (unten - oben) / ((hi - lo) * 100))
  graphics_context_set_antialiased(g, false);
  graphics_context_set_stroke_color(g, C_TIDE);
  graphics_context_set_stroke_width(g, 2);
  int vy = -1;
  for (int x = 0; x < 200; x++) {
    const int st = x * 24 / 200, rest = x * 24 % 200;
    const int a = s_gezeiten[versatz + st], b0 = s_gezeiten[versatz + st + 1 > 47 ? 47 : versatz + st + 1];
    if (a == K_FEHLT) { vy = -1; continue; }
    const int y = TIY(a * 100 + ((b0 == K_FEHLT ? a : b0) - a) * rest / 2);
    if (vy >= 0) graphics_draw_line(g, GPoint(x - 1, vy), GPoint(x, y));
    vy = y;
  }
  graphics_context_set_stroke_width(g, 1);
  Tide t[6];
  const int n = k_tiden(versatz, 25, t, 6);
  for (int i = 0; i < n; i++) {
    const int x = begrenzen(dx(t[i].min), 2, 197), y = TIY(t[i].hoch ? hi * 100 : lo * 100);
    k_dreieck(x, t[i].hoch ? y - 6 : y + 4, t[i].hoch, C_TIDE);
  }
  #undef TIY
}

// Fußzeile ohne Gezeiten: Sonnenzeiten an ihrer Stelle auf der Achse. Mit Gezeiten (seit 0.13) wie Klar: links die nächsten
// Hoch-/Niedrigwasser mit Pfeil, Trennstrich, rechts Sonnenauf- und -untergang. Mit der Pixelschrift passen 2 Gezeiten.
static void fuss(int versatz) {
  char buf[16];
  if (!k_hat_gezeiten(versatz)) {                                     // Binnenland: Sonnenzeiten an ihrer Stelle auf der Achse
    for (int i = 0; i < 2; i++) {
      const int m = ortsminuten(s_sonne[(versatz ? 2 : 0) + i]);
      if (m < 0) continue;
      snprintf(buf, sizeof buf, "%02d:%02d", m / 60, m % 60);
      klein(buf, begrenzen(dx(m), 20, 179), 204, MITTE, C_TEXT);
    }
    return;
  }
  Tide t[3];
  int nt = k_naechste_tiden(versatz, s_jetzt, t, 3);
  char txt[3][8];
  int w[3], summe;
  for (;;) {
    summe = 0;
    for (int i = 0; i < nt; i++) {
      const int m = t[i].min % 1440;
      snprintf(txt[i], sizeof txt[i], "%d:%02d", m / 60, m % 60);
      w[i] = 8 + text_w(F35, F35_N, txt[i], 2);
      summe += w[i];
    }
    if (nt <= 1 || summe + 3 * (nt + 1) <= 97) break;
    nt--;
  }
  int x = 1, rest = 97 - summe;
  for (int i = 0; i < nt; i++) {
    x += rest / (nt + 1);
    k_symbol(t[i].hoch ? SYM_HOCH : SYM_TIEF, x - 1, 208, C_TIDE);
    klein(txt[i], x + 7, 206, LINKS, C_TIDE);
    x += w[i];
  }
  flaeche(99, 205, 1, 14, C_GRAU);
  char st[2][8];
  int sw[2], ssum = 0, ns = 0;
  for (int i = 0; i < 2; i++) {
    const int m = ortsminuten(s_sonne[(versatz ? 2 : 0) + i]);
    if (m < 0) { sw[i] = -1; continue; }
    snprintf(st[i], sizeof st[i], "%d:%02d", m / 60, m % 60);
    sw[i] = 9 + text_w(F35, F35_N, st[i], 2);
    ssum += sw[i]; ns++;
  }
  x = 101; rest = 98 - ssum;
  for (int i = 0; i < 2; i++) {
    if (sw[i] < 0) continue;
    x += rest / (ns + 1);
    k_symbol(i ? SYM_UNTER : SYM_AUF, x, 208, C_TEXT);
    klein(st[i], x + 9, 206, LINKS, C_TEXT);
    x += sw[i];
  }
}

static void diagramm(const struct tm *lokal) {
  const int y0 = D_Y0, h = D_H, versatz = heute_versatz();
  const int jetzt = lokal->tm_hour * 60 + lokal->tm_min;
  int n_auf = -1, n_unter = 999;                      // Nacht: x < n_auf oder x >= n_unter
  s_jetzt = jetzt;
  if (versatz >= 0) {
    const int auf = ortsminuten(s_sonne[versatz ? 2 : 0]), unter = ortsminuten(s_sonne[versatz ? 3 : 1]);
    // Nacht als Schachbrettraster (seit 0.14; vorher jedes zweite Pixel jeder zweiten Zeile — kaum zu sehen)
    if (auf >= 0 && unter >= 0) {
      n_auf = dx(auf); n_unter = dx(unter);
      graphics_context_set_stroke_color(g, C_GITTER);
      for (int y = y0; y < y0 + h; y++)
        for (int x = y & 1; x < 200; x += 2)
          if (x < n_auf || x >= n_unter) graphics_draw_pixel(g, GPoint(x, y));
    }
  }
  // Gitter; im Nachtraster heller, sonst geht es darin unter
  #define GITTER(x) ((x) < n_auf || (x) >= n_unter ? C_NACHT_GITTER : C_GITTER)
  for (int x = 0; x < 200; x += 3) { flaeche(x, y0 + h / 3, 1, 1, GITTER(x)); flaeche(x, y0 + 2 * h / 3, 1, 1, GITTER(x)); }
  for (int st = 6; st <= 18; st += 6) for (int y = y0; y < y0 + h; y += 3) flaeche(dx(st * 60), y, 1, 1, GITTER(dx(st * 60)));
  #undef GITTER

  if (versatz >= 0) {                                  // ohne Daten: Hinweis unten nach dem Jetzt-Strich
    const uint8_t *temp = s_wetter, *regen = s_wetter + 48, *druck = s_wetter + 96;
    // Regen: Balken je Stunde, 1,5 mm = 60 % der Höhe
    for (int st = 0; st < 24; st++) {
      const int mm10 = regen[versatz + st];
      if (mm10 == 255 || mm10 == 0) continue;
      int bh = mm10 >= 15 ? h * 6 / 10 : mm10 * h * 6 / 150;
      if (bh < 1) bh = 1;
      flaeche(st * 200 / 24 + 1, y0 + h - bh, 6, bh, C_REGEN);
    }
    // Luftdruck: nur Trend, eigene Skala mit mindestens 10 hPa Spanne, gepunktet
    int plo = 999, phi = -1;
    for (int st = 0; st < 24; st++) { const int v = druck[versatz + st]; if (v == 255) continue; if (v < plo) plo = v; if (v > phi) phi = v; }
    if (phi >= 0) {
      const int mitte2 = plo + phi, spanne = phi - plo > 20 ? phi - plo : 20;   // Einheit hPa/2
      for (int x = 0; x < 200; x += 5) {
        const int st = x * 24 / 200, rest = x * 24 % 200;
        const int a = druck[versatz + st], b = (versatz + st + 1 < 48 && druck[versatz + st + 1] != 255) ? druck[versatz + st + 1] : a;
        if (a == 255) continue;
        const int v2 = a * 2 + (b - a) * 2 * rest / 200;                         // Wert ×2
        const int y = y0 + 8 + (h - 14) - (v2 - mitte2 + spanne) * (h - 14) / (2 * spanne);
        flaeche(x, begrenzen(y - 1, y0, y0 + h - 3), 3, 3, C_DRUCK);
      }
    }
    tide_kurve(versatz, y0, h);                                   // über dem Luftdruck, unter der Temperatur
    // Temperatur
    int tlo = 999, thi = -999, ilo = 0, ihi = 0;
    for (int st = 0; st < 24; st++) {
      const int v = temp_wert(temp[versatz + st]);
      if (v == -128) continue;
      if (v < tlo) { tlo = v; ilo = st; }
      if (v > thi) { thi = v; ihi = st; }
    }
    if (thi > -999) {
      const int oben = y0 + 13, unten = y0 + h - 4;
      const int spanne = thi - tlo > 2 ? thi - tlo : 2;
      #define TY(v) (unten - ((v) - tlo) * (unten - oben) / spanne)
      graphics_context_set_antialiased(g, false);
      graphics_context_set_stroke_color(g, C_TEMP);
      graphics_context_set_stroke_width(g, 2);
      for (int st = 0; st < 24; st++) {
        const int a = temp_wert(temp[versatz + st]);
        const int b = versatz + st + 1 < 48 ? temp_wert(temp[versatz + st + 1]) : a;
        if (a == -128 || b == -128) continue;
        graphics_draw_line(g, GPoint(st * 200 / 24, TY(a)), GPoint(st == 23 ? 199 : (st + 1) * 200 / 24, TY(b)));
      }
      graphics_context_set_stroke_width(g, 1);
      char buf[32];
      const int idx[2] = { ihi, ilo }, wert[2] = { thi, tlo };
      for (int i = 0; i < 2; i++) {
        const int v = wert[i], grad = v >= 0 ? (v + 1) / 2 : -((-v + 1) / 2);
        snprintf(buf, sizeof buf, "%d\xB0", grad);
        const int w = text_w(F35, F35_N, buf, 2);
        const int x = begrenzen(idx[i] * 200 / 24, w / 2 + 1, 199 - w / 2);
        const int ly = begrenzen(TY(v) - 14, y0 + 1, y0 + h - 11);
        const int nah = x - dx(jetzt) < 0 ? dx(jetzt) - x : x - dx(jetzt);
        if (!(versatz >= 0 && ly < y0 + 14 && nah < w / 2 + 16)) klein(buf, x, ly, MITTE, C_TEMP);   // sonst deckt die Jetzt-Temperatur
        if (thi == tlo) break;
      }
      #undef TY
    }
    fuss(versatz);
  }
  // Jetzt-Strich; darüber die aktuelle Temperatur aus der Kurve (zwischen den Stundenwerten linear)
  int strich_y = y0;
  if (versatz >= 0) {
    const int st = jetzt / 60, a = temp_wert(s_wetter[versatz + st]);
    const int b = versatz + st + 1 < 48 ? temp_wert(s_wetter[versatz + st + 1]) : a;
    if (a != -128) {
      const int v2 = a * 60 + ((b == -128 ? a : b) - a) * (jetzt % 60);
      const int grad = v2 >= 0 ? (v2 + 60) / 120 : -((-v2 + 60) / 120);
      char buf[8];
      snprintf(buf, sizeof buf, "%d\xB0", grad);
      const int w = text_w(F35, F35_N, buf, 2), x = begrenzen(dx(jetzt) + 1, w / 2 + 2, 197 - w / 2);
      flaeche(x - w / 2 - 2, y0, w + 4, 13, C_HG);
      klein(buf, x, y0 + 1, MITTE, C_TEMP);
      strich_y = y0 + 13;
    }
  }
  flaeche(dx(jetzt), strich_y, 2, y0 + h - strich_y, C_JETZT);
  // Achse, Striche bei 0/6/12/18/24 Uhr
  flaeche(0, y0 + h, 200, 1, C_GRAU);
  for (int st = 0; st <= 24; st += 6) flaeche(st == 24 ? 199 : dx(st * 60), y0 + h, 1, st % 12 ? 4 : 6, C_GRAU);
  if (versatz < 0) {                                   // Hinweis mittig, über Gitter und Jetzt-Strich freigestellt
    const char *t = hinweis_ohne_wetter();
    const int w = text_w(F35, F35_N, t, 2);
    flaeche(100 - w / 2 - 3, y0 + 20, w + 6, 14, C_HG);
    klein(t, 100, y0 + 22, MITTE, C_TEXT);
  }
}

// ---------- Gesamtbild ----------
static void zeichnen(Layer *layer, GContext *ctx) {
  g = ctx;
  if (SKIN_IST_KLAR) { k_zeichnen(); return; }
  flaeche(0, 0, 200, 228, C_HG);
  const time_t now = time(NULL);
  struct tm lokal = *localtime(&now);
  struct tm utc = *gmtime(&now);
  kopf();

  char buf[48], zeit[16], versatz[32];
  snprintf(zeit, sizeof zeit, "%02d:%02d", lokal.tm_hour, lokal.tm_min);
  gross(zeit, 6, 67, 5, LINKS, C_TEXT);
  if (s_sekunden) {                                    // klein, unten bündig und dicht an der Uhrzeit (seit 0.21; 0.20 hochgestellt)
    snprintf(buf, sizeof buf, "%02d", lokal.tm_sec);
    klein(buf, 6 + text_w(F57, F57_N, "00:00", 5) + 3, 67 + 7 * 5 - 10, LINKS, C_TEXT);
  }

  // Kopf (seit 0.16, Wunsch Seb 2026-10-09): Wochentag und Datum in einer Zeile, nie breiter als die Uhrzeit. Seit 0.18 mit Jahr zweistellig und schmal gezeichnet (5 × 7, Punkt 2 breit
  // und 3 hoch, 21 px): dreifach bräuchte „MO 30.12.26“ 156 px, schmal 22 + 78 von 125.
  // Darunter klein und mittig über der Uhrzeit Zone und UTC-Versatz („MESZ  UTC+2“). Kopf dafür 7 px höher als in 0.14
  const int x0 = 6, rechts = 6 + text_w(F57, F57_N, "00:00", 5), oben = 22, zeile2 = 51;
  char zone[8];
  // Seit 0.19: Wochentag und Datum als ein Block mit Wortabstand, mittig über der Uhrzeit (Lücke bis 0.18 wirkte komisch)
  snprintf(buf, sizeof buf, "%s %02d.%02d.%02d", WT[lokal.tm_wday], lokal.tm_mday, lokal.tm_mon + 1, lokal.tm_year % 100);
  text_xy(F57, F57_N, 7, buf, (x0 + rechts - text_xy_w(F57, F57_N, buf, 2, 2)) / 2, oben, 2, 3, 2, C_TEXT);
  zonen_kuerzel(&lokal, zone, sizeof zone);
  utc_versatz(lokal.tm_gmtoff, versatz, sizeof versatz);
  snprintf(buf, sizeof buf, "%s%s%s", zone, zone[0] ? "  " : "", versatz);
  if (text_w(F35, F35_N, buf, 2) > rechts - x0) snprintf(buf, sizeof buf, "%s", versatz);   // Kürzel + Versatz zu breit: nur Versatz
  klein(buf, (x0 + rechts) / 2, zeile2, MITTE, C_TEXT);

  mond(162, 79, 13);
  akku_zeichnen(186, 66);
  park_zeichnen(false);

  snprintf(buf, sizeof buf, "%02d%02dZ", utc.tm_hour, utc.tm_min);
  gross(buf, 6, 115, 2, LINKS, C_TEXT);
  puls_zeichnen();
  diagramm(&lokal);
}

// ---------- Handy ----------
static bool braucht_wetter(time_t now) {
  return !s_hat_wetter || now - s_stand >= ABRUF_S || heute_versatz() < 0;
}

static void anfragen(time_t now) {
  if (now - s_letzte_anfrage < s_warte_s) return;
  DictionaryIterator *it;
  if (app_message_outbox_begin(&it) != APP_MSG_OK) return;
  dict_write_int32(it, MESSAGE_KEY_ANFRAGE, 1);
  if (app_message_outbox_send() == APP_MSG_OK) s_letzte_anfrage = now;
}

// Handy wieder in Reichweite (seit 0.24): sofort fragen, falls überfällig. Weg: Hinweis „KEIN HANDY“ zeigen
static void handy_verbindung(bool da) {
  if (da) { s_letzte_anfrage = 0; s_warte_s = WIEDERHOLEN_S; if (braucht_wetter(time(NULL))) anfragen(time(NULL)); }
  layer_mark_dirty(s_layer);
}

static void empfangen(DictionaryIterator *it, void *ctx) {
  Tuple *t;
  if (dict_find(it, MESSAGE_KEY_BEREIT)) {         // Handy-Skript gestartet: bei Bedarf sofort fragen
    s_letzte_anfrage = 0;
    if (braucht_wetter(time(NULL))) anfragen(time(NULL));
  }
  if ((t = dict_find(it, MESSAGE_KEY_FEHLER))) {   // Netz/Standort fehlt: weiter jede Minute; Server- oder Datenfehler: 10 min
    s_fehler = t->value->int32;
    s_warte_s = s_fehler >= 3 ? SERVER_S : WIEDERHOLEN_S;
    APP_LOG(APP_LOG_LEVEL_WARNING, "Handy meldet Fehler %d", (int)s_fehler);
  }
  if ((t = dict_find(it, MESSAGE_KEY_WETTER)) && t->length == WETTER_LEN) {
    memcpy(s_wetter, t->value->data, WETTER_LEN);
    s_hat_wetter = true;
    s_fehler = 0; s_warte_s = WIEDERHOLEN_S;
    persist_write_data(PK_WETTER, s_wetter, WETTER_LEN);
  }
  if ((t = dict_find(it, MESSAGE_KEY_GEZEITEN)) && t->length == 48) {
    memcpy(s_gezeiten, t->value->data, 48);
    persist_write_data(PK_GEZEITEN, s_gezeiten, 48);
  }
  if ((t = dict_find(it, MESSAGE_KEY_SKIN))) { s_skin = t->value->int32 >= SKIN_PHOSPHOR && t->value->int32 <= SKIN_KLAR_DUNKEL ? t->value->int32 : SKIN_PHOSPHOR; persist_write_int(PK_SKIN, s_skin); }
  if ((t = dict_find(it, MESSAGE_KEY_SPRACHE))) { s_de = t->value->int32 == 1; persist_write_int(PK_SPRACHE, s_de ? 1 : 0); }
  for (int i = 0; i < 2; i++) if ((t = dict_find(it, MESSAGE_KEY_PARK1 + i))) {   // leerer Text = Notiz gelöscht
    snprintf(s_park[i], sizeof s_park[i], "%s", t->value->cstring);
    if (s_park[i][0]) persist_write_string(PK_PARK1 + i, s_park[i]); else persist_delete(PK_PARK1 + i);
  }
  if ((t = dict_find(it, MESSAGE_KEY_TAG))) { s_tag = t->value->int32; persist_write_int(PK_TAG, s_tag); }
  if ((t = dict_find(it, MESSAGE_KEY_STAND))) { s_stand = t->value->int32; persist_write_int(PK_STAND, s_stand); }
  bool sonne = false;
  for (int i = 0; i < 4; i++) if ((t = dict_find(it, MESSAGE_KEY_SONNE + i))) { s_sonne[i] = t->value->int32; sonne = true; }
  if (sonne) persist_write_data(PK_SONNE, s_sonne, sizeof s_sonne);
  if ((t = dict_find(it, MESSAGE_KEY_IATA))) { snprintf(s_iata, sizeof s_iata, "%s", t->value->cstring); persist_write_string(PK_IATA, s_iata); }
  if ((t = dict_find(it, MESSAGE_KEY_BREITE))) { s_breite = t->value->int32; persist_write_int(PK_BREITE, s_breite); }
  layer_mark_dirty(s_layer);
}

static void laden(void) {
  for (int i = 0; i < 48; i++) s_gezeiten[i] = -128;
  if (persist_exists(PK_SKIN)) s_skin = persist_read_int(PK_SKIN);
  if (persist_exists(PK_SPRACHE)) s_de = persist_read_int(PK_SPRACHE) == 1;
  for (int i = 0; i < 2; i++) if (persist_exists(PK_PARK1 + i)) persist_read_string(PK_PARK1 + i, s_park[i], sizeof s_park[i]);
  if (!persist_exists(PK_FORMAT) || persist_read_int(PK_FORMAT) != FORMAT) {
    for (int k = PK_WETTER; k <= PK_BREITE; k++) persist_delete(k);
    persist_delete(PK_GEZEITEN);
    persist_write_int(PK_FORMAT, FORMAT);
    return;
  }
  if (persist_exists(PK_WETTER) && persist_read_data(PK_WETTER, s_wetter, WETTER_LEN) == WETTER_LEN) s_hat_wetter = true;
  if (persist_exists(PK_TAG)) s_tag = persist_read_int(PK_TAG);
  if (persist_exists(PK_STAND)) s_stand = persist_read_int(PK_STAND);
  if (persist_exists(PK_SONNE)) persist_read_data(PK_SONNE, s_sonne, sizeof s_sonne);
  if (persist_exists(PK_IATA)) persist_read_string(PK_IATA, s_iata, sizeof s_iata);
  if (persist_exists(PK_BREITE)) s_breite = persist_read_int(PK_BREITE);
  if (persist_exists(PK_GEZEITEN)) persist_read_data(PK_GEZEITEN, s_gezeiten, 48);
}

static void tick(struct tm *t, TimeUnits u) {
  if (u & MINUTE_UNIT) {                               // im Sekundentakt nur zeichnen, Puls und Wetter weiter minütlich
    const time_t now = time(NULL);
    if (now - s_puls_stand >= PULS_ALLE_S) puls_laden();
    if (braucht_wetter(now)) anfragen(now);
  }
  layer_mark_dirty(s_layer);
}

static void sekunden_aus(void *d) {
  s_sek_timer = NULL;
  s_sekunden = false;
  tick_timer_service_subscribe(MINUTE_UNIT, tick);
  layer_mark_dirty(s_layer);
}

static void handgelenk(AccelAxisType achse, int32_t richtung) {   // jede weitere Bewegung verlängert
  if (!s_sekunden) {
    s_sekunden = true;
    tick_timer_service_subscribe(SECOND_UNIT, tick);
    layer_mark_dirty(s_layer);
  }
  if (!s_sek_timer || !app_timer_reschedule(s_sek_timer, SEK_DAUER_MS)) s_sek_timer = app_timer_register(SEK_DAUER_MS, sekunden_aus, NULL);
}

static void akku(BatteryChargeState b) { layer_mark_dirty(s_layer); }

static void fenster_laden(Window *w) {
  Layer *root = window_get_root_layer(w);
  s_layer = layer_create(layer_get_bounds(root));
  layer_set_update_proc(s_layer, zeichnen);
  layer_add_child(root, s_layer);
}

static void fenster_entladen(Window *w) { layer_destroy(s_layer); }

static void init(void) {
  laden();
  puls_laden();
  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
  window_set_window_handlers(s_window, (WindowHandlers){ .load = fenster_laden, .unload = fenster_entladen });
  window_stack_push(s_window, false);
  app_message_register_inbox_received(empfangen);
  app_message_open(512, 64);
  tick_timer_service_subscribe(MINUTE_UNIT, tick);
  battery_state_service_subscribe(akku);
  accel_tap_service_subscribe(handgelenk);
  connection_service_subscribe((ConnectionHandlers){ .pebble_app_connection_handler = handy_verbindung });
}

static void deinit(void) {
  if (s_sek_timer) app_timer_cancel(s_sek_timer);
  accel_tap_service_unsubscribe();
  connection_service_unsubscribe();
  tick_timer_service_unsubscribe();
  battery_state_service_unsubscribe();
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
  return 0;
}
