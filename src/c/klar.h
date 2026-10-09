// Ansicht „Klar“ — Stil der Ansicht Klar aus Linie 8: Schwarz auf Weiß, Systemschriften, Kurven pixelgenau.
// Positionen wie Phosphor. Dazu Gezeiten: Kurve im Diagramm, Hoch- und Niedrigwasser in der Fußzeile.
// Wird in main.c eingebunden und nutzt dessen Zustand und Hilfsfunktionen (flaeche, heute_versatz, dx …).
#pragma once

// Seit 0.22 „Klar dunkel“ (K_DUNKEL): Schwarz und Weiß, Hell- und Dunkelgrau getauscht; Kurvenfarben bleiben
#define K_DUNKEL  (s_skin == SKIN_KLAR_DUNKEL)
#define K_SCHWARZ (K_DUNKEL ? GColorWhite : GColorBlack)
#define K_WEISS   (K_DUNKEL ? GColorBlack : GColorWhite)
#define K_DGRAU   GColorFromHEX(K_DUNKEL ? 0xAAAAAA : 0x555555)
#define K_HGRAU   GColorFromHEX(K_DUNKEL ? 0x555555 : 0xAAAAAA)
#define K_ROT     GColorFromHEX(0xFF0000)   // Temperatur
#define K_DRUCK   GColorFromHEX(0xFFAA00)   // Luftdruck: Gelb in der dunkelsten Stufe, die auf Weiß lesbar bleibt
#define K_REGEN   GColorFromHEX(0x55AAFF)
#define K_TIDE    GColorFromHEX(K_DUNKEL ? 0x55AAFF : 0x0055AA)   // Gezeiten; auf Schwarz heller
#define K_WARN    GColorFromHEX(0xFF5500)   // Akku unter AKKU_WARN
#define K_GRUEN   GColorFromHEX(0x00AA00)   // Schrittziel erreicht
#define K_ZEIT_X  6
// Uhrzeit in LECO wie Linie 8 Klar (dort 32 fett), kantiger als Roboto. Werte im Emulator gemessen:
// LECO 42: Ziffern 29 px hoch, Rahmen → Oberkante 13 px · LECO 38 fett: 27 px, 11 px. Linker Seitenabstand je 3 px
#ifdef K_UHR_38
#define K_UHR_FONT FONT_KEY_LECO_38_BOLD_NUMBERS
#define K_UHR_OBEN 11
#define K_UHR_Y    75                       // Oberkante der Ziffern: mittig zwischen Zeitzone und UTC-Zeile
#define K_UHR_H    27                       // Ziffernhöhe LECO 38 fett
#else
#define K_UHR_FONT FONT_KEY_LECO_42_NUMBERS
#define K_UHR_OBEN 13
#define K_UHR_Y    74
#define K_UHR_H    29                       // Ziffernhöhe LECO 42
#endif
#define K_ZEIT_L  3
#define K_SEK_H   11                       // Gothic 18 fett: Ziffernhöhe (Emulator 2026-10-09)
// Kopf (seit 0.14): Wochentag in Bitham 30 Black, im Emulator gemessen: Großbuchstaben 21 px hoch,
// 9 px unter dem Textrahmen, 2 px Seitenabstand. Datum und Zone daneben in Gothic 14, zusammen so breit wie die Uhrzeit
#define K_WT_FONT FONT_KEY_BITHAM_30_BLACK
#define K_WT_H    21
#define K_WT_OBEN 9
#define K_WT_L    2
#define K_ZEILE_H 10                        // Gothic 14 fett: Höhe der Großbuchstaben und Ziffern
#define K_G28_OBEN 10                       // Gothic 28 fett: Textrahmen -> Oberkante der Großbuchstaben (Emulator 2026-10-09)
#define K_G28_H    18                       // Gothic 28 fett: Höhe der Großbuchstaben und Ziffern (Emulator 2026-10-09)
#define K_FEHLT   (-128)

// ---------- Schrift ----------
enum { KG14, KG18, KG24, KUHR, KG14R, KG18R, KG24R, KG28 };
static GFont k_font(int f) {
  static const char *KEY[] = { FONT_KEY_GOTHIC_14_BOLD, FONT_KEY_GOTHIC_18_BOLD, FONT_KEY_GOTHIC_24_BOLD,
                               K_UHR_FONT, FONT_KEY_GOTHIC_14, FONT_KEY_GOTHIC_18, FONT_KEY_GOTHIC_24,
                               FONT_KEY_GOTHIC_28_BOLD };
  return fonts_get_system_font(KEY[f]);
}
static const int K_OBEN[] = { 5, 7, 10, K_UHR_OBEN, 5, 7, 10, K_G28_OBEN };   // Textrahmen → Oberkante der Ziffern, im Emulator gemessen

static int k_text_w(int f, const char *s) {
  return graphics_text_layout_get_content_size(s, k_font(f), GRect(0, 0, 400, 80), GTextOverflowModeFill, GTextAlignmentLeft).w;
}
// y = Oberkante der Großbuchstaben
static void k_text(int f, const char *s, int x, int y, int ausr, GColor c) {
  const int w = k_text_w(f, s);
  const int x0 = ausr == MITTE ? x - w / 2 : ausr == RECHTS ? x - w : x;
  graphics_context_set_text_color(g, c);
  graphics_draw_text(g, s, k_font(f), GRect(x0, y - K_OBEN[f], w + 4, 80), GTextOverflowModeFill, GTextAlignmentLeft, NULL);
}
// Text auf genau „breite“ px gesperrt (höchstens 3 px mehr je Lücke, sonst zentriert); passt er nicht, kleinere Schrift
static void k_text_breit(int f, int f_klein, const char *s, int x0, int breite, int y, GColor c) {
  if (k_text_w(f, s) > breite) { if (K_OBEN[f] != K_OBEN[f_klein]) y += K_OBEN[f] - K_OBEN[f_klein] + 2; f = f_klein; }
  int n = 0, summe = 0, w[32];
  char z[2] = { 0, 0 };
  for (const char *p = s; *p && n < 32; p++, n++) { z[0] = *p; w[n] = k_text_w(f, z); summe += w[n]; }
  const int rest = breite - summe, luecken = n > 1 ? n - 1 : 1;
  if (rest < 0 || rest > 3 * luecken) { k_text(f, s, x0 + breite / 2, y, MITTE, c); return; }   // kurz: zentrieren statt zerren; zu breit: nicht stauchen
  int x = x0;
  graphics_context_set_text_color(g, c);
  for (int i = 0; i < n; i++) {
    z[0] = s[i];
    graphics_draw_text(g, z, k_font(f), GRect(x, y - K_OBEN[f], w[i] + 4, 80), GTextOverflowModeFill, GTextAlignmentLeft, NULL);
    x += w[i] + rest * (i + 1) / luecken - rest * i / luecken;
  }
}

static void k_linie(int x1, int y1, int x2, int y2, int b, GColor c) {
  graphics_context_set_antialiased(g, true);
  graphics_context_set_stroke_color(g, c);
  graphics_context_set_stroke_width(g, b);
  graphics_draw_line(g, GPoint(x1, y1), GPoint(x2, y2));
  graphics_context_set_stroke_width(g, 1);
}

// ---------- Kopf: Schritte ----------
static void k_kopf(void) {
  char buf[16];
  int st = (int)health_service_sum_today(HealthMetricStepCount);
  if (st < 0) st = 0;
  if (st >= 1000) snprintf(buf, sizeof buf, "%d.%03d", st / 1000, st % 1000);
  else snprintf(buf, sizeof buf, "%d", st);
  flaeche(0, 18, 200, 1, K_HGRAU);
  k_text(KG18, buf, 6, 3, LINKS, K_SCHWARZ);
  const GColor an = st >= SCHRITT_ZIEL ? K_GRUEN : K_SCHWARZ;
  for (int i = 0; i < 10; i++) flaeche(68 + i * 13, 3, 11, 8, i < st / 1000 ? an : K_HGRAU);
}

// ---------- Akku ----------
static void k_akku(int x, int y0) {
  const BatteryChargeState b = battery_state_service_peek();
  const bool warn = b.charge_percent < AKKU_WARN;
  const GColor an = warn ? K_WARN : K_SCHWARZ;
  char buf[8];
  snprintf(buf, sizeof buf, "%d", b.charge_percent);
  const int f = b.charge_percent >= 100 ? KG14 : KG18;             // „100“ in 18 px stößt an den Mondring
  const int w = k_text_w(f, buf);
  int zx = x + 5 - w / 2;
  if (zx + w > 198) zx = 198 - w;
  k_text(f, buf, zx, f == KG14 ? y0 + 1 : y0, LINKS, an);
  const int voll = (b.charge_percent + 10) / 20;
  flaeche(x + 3, y0 + 13, 4, 2, voll >= 5 ? an : K_HGRAU);
  for (int i = 0; i < 5; i++) flaeche(x, y0 + 36 - i * 5, 10, 4, i < voll ? an : K_HGRAU);
}

// ---------- Mond: schematische Kugel (mond_kugel in main.c) — Tagseite weiß, Nachtseite dunkelgrau, Rand schwarz ----------
static void k_mond(int cx, int cy, int r) {
  // dunkel: Licht weiß, Schatten dunkelgrau, Ring hellgrau — Neumond = dunkle Scheibe mit Ring, Vollmond = weiße Scheibe
  if (K_DUNKEL) mond_kugel(cx, cy, r, GColorWhite, GColorFromHEX(0x555555), true, GColorFromHEX(0xAAAAAA));
  else mond_kugel(cx, cy, r, K_WEISS, K_DGRAU, true, K_SCHWARZ);
  k_text(KG14R, mond_zunehmend() ? TXT("WAX", "ZUN") : TXT("WAN", "ABN"), cx, cy + r + 5, MITTE, K_DGRAU);
}

// ---------- Puls: 28 Spalten im Abstand 2 px als Linie ----------
static void k_puls(void) {
  const int x1 = 76, x2 = 131, y0 = 113, h = 18;
  graphics_context_set_stroke_color(g, K_HGRAU);
  graphics_draw_rect(g, GRect(x1 - 3, y0 - 3, x2 - x1 + 7, h + 7));
  int lo = 999, hi = 0;
  for (int i = 0; i < PULS_SP; i++) if (s_puls[i]) { if (s_puls[i] < lo) lo = s_puls[i]; if (s_puls[i] > hi) hi = s_puls[i]; }
  char buf[16];
  if (hi == 0) {
    k_text(KG18, "MAX --", 196, 111, RECHTS, K_SCHWARZ);
    k_text(KG18R, "MIN --", 196, 124, RECHTS, K_DGRAU);
    return;
  }
  lo -= 3;
  if (hi < lo + 40) hi = lo + 40;
  int vx = -1, vy = 0;
  for (int i = 0; i < PULS_SP; i++) {
    if (!s_puls[i]) { vx = -1; continue; }
    const int x = x1 + i * 2, y = y0 + h - 1 - (s_puls[i] - lo) * (h - 1) / (hi - lo);
    if (vx >= 0) k_linie(vx, vy, x, y, 1, K_SCHWARZ); else flaeche(x, y, 1, 1, K_SCHWARZ);
    vx = x; vy = y;
  }
  snprintf(buf, sizeof buf, "MAX %d", s_puls_max);
  k_text(KG18, buf, 196, 111, RECHTS, K_SCHWARZ);
  snprintf(buf, sizeof buf, "MIN %d", s_puls_min);
  k_text(KG18R, buf, 196, 124, RECHTS, K_DGRAU);
}

// ---------- Gezeiten ----------
typedef struct { int16_t min; bool hoch; } Tide;   // min: Minuten ab Datentag-Stunde „versatz“
static int s_jetzt;                                // Minuten nach Mitternacht, für die Fußzeile

static bool k_hat_gezeiten(int versatz) {
  for (int i = versatz; i < versatz + 24 && i < 48; i++) if (s_gezeiten[i] != K_FEHLT) return true;
  return false;
}

// Extremum der Stundenwerte bei Index i: 0 keins, 1 Hochwasser, 2 Niedrigwasser; *korr = Versatz in Minuten (Parabel)
static int k_extrem(int i, int *korr) {
  if (i < 1 || i > 46) return 0;
  const int a = s_gezeiten[i - 1], b = s_gezeiten[i], c = s_gezeiten[i + 1];
  if (a == K_FEHLT || b == K_FEHLT || c == K_FEHLT) return 0;
  const bool hoch = b > a && b >= c, tief = b < a && b <= c;
  if (!hoch && !tief) return 0;
  const int nenner = a - 2 * b + c;
  *korr = begrenzen(nenner ? 30 * (a - c) / nenner : 0, -30, 30);
  return hoch ? 1 : 2;
}

// Hoch- und Niedrigwasser des Tages: lokale Extrema der Stundenwerte, Parabel durch drei Punkte → Minuten.
// „stunden“ = 25, damit ein Hochwasser kurz vor Mitternacht (Stundenwert um 0 Uhr am höchsten) mitkommt
static int k_tiden(int versatz, int stunden, Tide *aus, int max) {
  int n = 0;
  for (int st = 0; st < stunden && n < max; st++) {
    int korr = 0;
    const int art = k_extrem(versatz + st, &korr);
    if (!art) continue;
    aus[n].min = (int16_t)(st * 60 + korr);
    aus[n].hoch = art == 1;
    if (aus[n].min >= 0 && aus[n].min < 1440) n++;                 // nur Ereignisse dieses Tages
  }
  return n;
}

// Die nächsten Hoch- und Niedrigwasser ab jetzt, auch über Mitternacht hinaus, soweit die 48 h reichen.
// min: Minuten ab 0 Uhr heute (kann ≥ 1440 sein)
static int k_naechste_tiden(int versatz, int jetzt, Tide *aus, int max) {
  int n = 0;
  for (int st = jetzt / 60 - 1; versatz + st <= 46 && n < max; st++) {
    int korr = 0;
    const int art = k_extrem(versatz + st, &korr);
    if (!art || st * 60 + korr <= jetzt) continue;
    aus[n].min = (int16_t)(st * 60 + korr);
    aus[n].hoch = art == 1;
    n++;
  }
  return n;
}

// Mini-Symbole für die Fußzeile, Zeile für Zeile, Bit 0x40 = linke Spalte bei 7 px Breite
static const uint8_t SYM_HOCH[8]  = { 0x08, 0x1C, 0x2A, 0x08, 0x08, 0x08, 0x08, 0x08 };   // Pfeil hoch, 5 breit (in 7er-Raster mittig)
static const uint8_t SYM_TIEF[8]  = { 0x08, 0x08, 0x08, 0x08, 0x08, 0x2A, 0x1C, 0x08 };
static const uint8_t SYM_AUF[8]   = { 0x08, 0x1C, 0x2A, 0x08, 0x00, 0x1C, 0x3E, 0x7F };   // Pfeil hoch über halber Sonne
static const uint8_t SYM_UNTER[8] = { 0x08, 0x2A, 0x1C, 0x08, 0x00, 0x1C, 0x3E, 0x7F };   // Pfeil runter über halber Sonne
static void k_symbol(const uint8_t *z, int x, int y, GColor c) {      // 7 × 8, x = linke Kante des 7er-Rasters
  for (int j = 0; j < 8; j++) for (int i = 0; i < 7; i++) if (z[j] & (0x40 >> i)) flaeche(x + i, y + j, 1, 1, c);
}

// Dreieck, 5 px breit; Spitze oben = Hochwasser
static void k_dreieck(int x, int y, bool hoch, GColor c) {
  for (int k = 0; k < 3; k++) flaeche(x - k, hoch ? y + k : y + 2 - k, 2 * k + 1, 1, c);
}

static void k_tide_kurve(int versatz, int y0, int h) {
  int lo = 999, hi = -999;
  for (int st = 0; st <= 24; st++) {
    const int v = s_gezeiten[versatz + st > 47 ? 47 : versatz + st];
    if (v == K_FEHLT) continue;
    if (v < lo) lo = v;
    if (v > hi) hi = v;
  }
  if (hi < lo) return;
  if (hi - lo < 4) hi = lo + 4;                                    // mindestens 20 cm Spanne
  const int oben = y0 + 8, unten = y0 + h - 5;
  #define TIY(v100) (unten - ((v100) - lo * 100) * (unten - oben) / ((hi - lo) * 100))
  int vy = -1;
  for (int x = 0; x < 200; x++) {
    const int st = x * 24 / 200, rest = x * 24 % 200;
    const int a = s_gezeiten[versatz + st], b0 = s_gezeiten[versatz + st + 1 > 47 ? 47 : versatz + st + 1];
    if (a == K_FEHLT) { vy = -1; continue; }
    const int b = b0 == K_FEHLT ? a : b0;
    const int y = TIY(a * 100 + (b - a) * rest / 2);
    if (vy >= 0) k_linie(x - 1, vy, x, y, 2, K_TIDE);
    vy = y;
  }
  Tide t[6];
  const int n = k_tiden(versatz, 25, t, 6);                       // 25 h: Hochwasser kurz vor Mitternacht liegt im Wert von 0 Uhr
  for (int i = 0; i < n; i++) {
    const int x = begrenzen(dx(t[i].min), 2, 197), y = TIY(t[i].hoch ? hi * 100 : lo * 100);
    k_dreieck(x, t[i].hoch ? y - 5 : y + 3, t[i].hoch, K_TIDE);
  }
  #undef TIY
}

// ---------- Fußzeile ----------
// Ohne Gezeiten: Sonnenzeiten wie in Phosphor an ihrer Stelle auf der Achse (Gothic 18 fett).
// Mit Gezeiten (seit 0.13): links die nächsten Hoch-/Niedrigwasser mit Pfeil, zuerst das nächste, auch über Mitternacht;
// Trennstrich in der Mitte; rechts Sonnenauf- und -untergang mit Symbol. Gothic 14 normal. Es passen 2 Gezeiten.
static void k_fuss(int versatz) {
  char buf[16];
  if (!k_hat_gezeiten(versatz)) {
    for (int i = 0; i < 2; i++) {
      const int m = ortsminuten(s_sonne[(versatz ? 2 : 0) + i]);
      if (m < 0) continue;
      snprintf(buf, sizeof buf, "%02d:%02d", m / 60, m % 60);
      k_text(KG18, buf, begrenzen(dx(m), 20, 179), 205, MITTE, K_SCHWARZ);
    }
    return;
  }
  // Symbole 7 × 8 an der Grundlinie
  Tide t[3];
  int nt = k_naechste_tiden(versatz, s_jetzt, t, 3);
  char txt[3][8];
  int w[3], summe;
  for (;;) {                                                          // so viele, wie in die linke Hälfte passen
    summe = 0;
    for (int i = 0; i < nt; i++) {
      const int m = t[i].min % 1440;
      snprintf(txt[i], sizeof txt[i], "%d:%02d", m / 60, m % 60);
      w[i] = 8 + k_text_w(KG14R, txt[i]);
      summe += w[i];
    }
    if (nt <= 1 || summe + 3 * (nt + 1) <= 97) break;
    nt--;
  }
  int x = 1, rest = 97 - summe;
  for (int i = 0; i < nt; i++) {
    x += rest / (nt + 1);
    k_symbol(t[i].hoch ? SYM_HOCH : SYM_TIEF, x - 1, 208, K_TIDE);
    k_text(KG14R, txt[i], x + 7, 207, LINKS, K_TIDE);
    x += w[i];
  }
  flaeche(99, 205, 1, 14, K_HGRAU);
  char st[2][8];
  int sw[2], ssum = 0, ns = 0;
  for (int i = 0; i < 2; i++) {
    const int m = ortsminuten(s_sonne[(versatz ? 2 : 0) + i]);
    if (m < 0) { sw[i] = -1; continue; }
    snprintf(st[i], sizeof st[i], "%d:%02d", m / 60, m % 60);
    sw[i] = 9 + k_text_w(KG14R, st[i]);
    ssum += sw[i]; ns++;
  }
  x = 101; rest = 98 - ssum;
  for (int i = 0; i < 2; i++) {
    if (sw[i] < 0) continue;
    x += rest / (ns + 1);
    k_symbol(i ? SYM_UNTER : SYM_AUF, x, 208, K_SCHWARZ);
    k_text(KG14R, st[i], x + 9, 207, LINKS, K_SCHWARZ);
    x += sw[i];
  }
}

// ---------- Diagramm ----------
static void k_diagramm(const struct tm *lokal) {
  const int y0 = D_Y0, h = D_H, versatz = heute_versatz();
  const int jetzt = lokal->tm_hour * 60 + lokal->tm_min;
  s_jetzt = jetzt;
  int n_auf = -1, n_unter = 999;                      // Nacht: x < n_auf oder x >= n_unter
  if (versatz >= 0) {
    const int auf = ortsminuten(s_sonne[versatz ? 2 : 0]), unter = ortsminuten(s_sonne[versatz ? 3 : 1]);
    if (auf >= 0 && unter >= 0) {                     // Schachbrett hellgrau (seit 0.14; vorher jedes vierte Pixel — kaum zu sehen)
      n_auf = dx(auf); n_unter = dx(unter);
      for (int y = y0; y < y0 + h; y++)
        for (int x = y & 1; x < 200; x += 2)
          if (x < n_auf || x >= n_unter) flaeche(x, y, 1, 1, K_HGRAU);
    }
  }
  #define K_GITTER(x) ((x) < n_auf || (x) >= n_unter ? K_DGRAU : K_HGRAU)   // im Nachtraster dunkler, sonst unsichtbar
  for (int x = 0; x < 200; x += 3) { flaeche(x, y0 + h / 3, 1, 1, K_GITTER(x)); flaeche(x, y0 + 2 * h / 3, 1, 1, K_GITTER(x)); }
  for (int st = 6; st <= 18; st += 6) for (int y = y0; y < y0 + h; y += 3) flaeche(dx(st * 60), y, 1, 1, K_GITTER(dx(st * 60)));
  #undef K_GITTER

  if (versatz >= 0) {                                  // ohne Daten: Hinweis unten nach dem Jetzt-Strich
    const uint8_t *temp = s_wetter, *regen = s_wetter + 48, *druck = s_wetter + 96;
    for (int st = 0; st < 24; st++) {
      const int mm10 = regen[versatz + st];
      if (mm10 == 255 || mm10 == 0) continue;
      int bh = mm10 >= 15 ? h * 6 / 10 : mm10 * h * 6 / 150;
      if (bh < 1) bh = 1;
      flaeche(st * 200 / 24 + 1, y0 + h - bh, 6, bh, K_REGEN);
    }
    // Luftdruck: Trend, 2 px gestrichelt (6 an, 4 aus) wie die Temperatur dick, Skala mindestens 10 hPa. Unterste Kurve
    int plo = 999, phi = -1;
    for (int st = 0; st < 24; st++) { const int v = druck[versatz + st]; if (v == 255) continue; if (v < plo) plo = v; if (v > phi) phi = v; }
    if (phi >= 0) {
      const int mitte2 = plo + phi, spanne = phi - plo > 20 ? phi - plo : 20;
      int vy = -1;
      for (int x = 0; x < 200; x++) {
        const int st = x * 24 / 200, rest = x * 24 % 200;
        const int a = druck[versatz + st], b = (versatz + st + 1 < 48 && druck[versatz + st + 1] != 255) ? druck[versatz + st + 1] : a;
        if (a == 255) { vy = -1; continue; }
        const int v200 = a * 200 + (b - a) * rest;
        const int y = begrenzen(y0 + 8 + (h - 14) - (v200 - (mitte2 - spanne) * 100) * (h - 14) / (2 * spanne * 100), y0 + 1, y0 + h - 2);
        if (vy >= 0 && x % 10 < 6) k_linie(x - 1, vy, x, y, 2, K_DRUCK);
        vy = y;
      }
    }
    k_tide_kurve(versatz, y0, h);                                   // Gezeiten über dem Luftdruck, unter der Temperatur
    // Temperatur
    int tlo = 999, thi = -999, ilo = 0, ihi = 0;
    for (int st = 0; st < 24; st++) {
      const int v = temp_wert(temp[versatz + st]);
      if (v == -128) continue;
      if (v < tlo) { tlo = v; ilo = st; }
      if (v > thi) { thi = v; ihi = st; }
    }
    if (thi > -999) {
      const int oben = y0 + 15, unten = y0 + h - 4;
      const int spanne = thi - tlo > 2 ? thi - tlo : 2;
      #define TY(v) (unten - ((v) - tlo) * (unten - oben) / spanne)
      for (int st = 0; st < 24; st++) {
        const int a = temp_wert(temp[versatz + st]);
        const int b = versatz + st + 1 < 48 ? temp_wert(temp[versatz + st + 1]) : a;
        if (a == -128 || b == -128) continue;
        k_linie(st * 200 / 24, TY(a), st == 23 ? 199 : (st + 1) * 200 / 24, TY(b), 2, K_ROT);
      }
      char buf[16];
      const int idx[2] = { ihi, ilo }, wert[2] = { thi, tlo };
      for (int i = 0; i < 2; i++) {
        const int v = wert[i], grad = v >= 0 ? (v + 1) / 2 : -((-v + 1) / 2);
        snprintf(buf, sizeof buf, "%d°", grad);
        const int w = k_text_w(KG18, buf);
        const int x = begrenzen(idx[i] * 200 / 24, w / 2 + 1, 199 - w / 2);
        const int ly = begrenzen(TY(v) - 16, y0 + 1, y0 + h - 13);
        const int nah = x - dx(jetzt) < 0 ? dx(jetzt) - x : x - dx(jetzt);
        if (!(ly < y0 + 16 && nah < w / 2 + 16)) k_text(KG18, buf, x, ly, MITTE, K_ROT);   // sonst deckt die Jetzt-Temperatur
        if (thi == tlo) break;
      }
      #undef TY
    }
    k_fuss(versatz);
  }
  // Jetzt-Strich; darüber die aktuelle Temperatur, aus der Kurve gelesen (zwischen den Stundenwerten linear)
  int strich_y = y0;
  if (versatz >= 0) {
    const int st = jetzt / 60, a = temp_wert(s_wetter[versatz + st]);
    const int b = versatz + st + 1 < 48 ? temp_wert(s_wetter[versatz + st + 1]) : a;
    if (a != -128) {
      const int v2 = a * 60 + ((b == -128 ? a : b) - a) * (jetzt % 60);            // halbe Grad × 60
      const int grad = v2 >= 0 ? (v2 + 60) / 120 : -((-v2 + 60) / 120);
      char buf[8];
      snprintf(buf, sizeof buf, "%d°", grad);
      const int w = k_text_w(KG18, buf), x = begrenzen(dx(jetzt) + 1, w / 2 + 2, 197 - w / 2);
      flaeche(x - w / 2 - 2, y0, w + 4, 15, K_WEISS);                              // freistellen über Gitter und Kurven
      k_text(KG18, buf, x, y0 + 1, MITTE, K_ROT);                                  // wie Tiefst- und Höchstwert
      strich_y = y0 + 16;
    }
  }
  flaeche(dx(jetzt), strich_y, 2, y0 + h - strich_y, K_DGRAU);
  flaeche(0, y0 + h, 200, 1, K_DGRAU);
  for (int st = 0; st <= 24; st += 6) flaeche(st == 24 ? 199 : dx(st * 60), y0 + h, 1, st % 12 ? 4 : 6, K_DGRAU);
  if (versatz < 0) {                                   // ohne Daten (seit 0.24): Hinweis mittig, freigestellt
    const char *t = hinweis_ohne_wetter();
    const int w = k_text_w(KG18, t);
    flaeche(100 - w / 2 - 4, y0 + 17, w + 8, 20, K_WEISS);
    k_text(KG18, t, 100, y0 + 17, MITTE, K_SCHWARZ);
  }
}

// ---------- Gesamtbild ----------
static void k_zeichnen(void) {
  flaeche(0, 0, 200, 228, K_WEISS);
  const time_t now = time(NULL);
  struct tm lokal = *localtime(&now);
  struct tm utc = *gmtime(&now);
  k_kopf();

  char buf[48], zeit[16], versatz[32];
  snprintf(zeit, sizeof zeit, "%02d:%02d", lokal.tm_hour, lokal.tm_min);
  k_text(KUHR, zeit, K_ZEIT_X, K_UHR_Y, LINKS, K_SCHWARZ);
  if (s_sekunden) {                                    // Sekunden (seit 0.20): Gothic 18 fett; seit 0.21 unten bündig, dicht an der Uhrzeit
    char sek[4];
    snprintf(sek, sizeof sek, "%02d", lokal.tm_sec);
    k_text(KG18, sek, K_ZEIT_X + k_text_w(KUHR, "00:00") - K_ZEIT_L + 1, K_UHR_Y + K_UHR_H - K_SEK_H, LINKS, K_SCHWARZ);
  }

  // Kopf (seit 0.16): Wochentag und Datum (seit 0.17 mit Jahr zweistellig, „MO 30.12.26“ 109 px) in Gothic 28 fett, Wochentag links, Datum rechtsbündig an der
  // Uhrzeit. Bitham 30 (Wochentag bis 0.14) wäre für „MO 30.12.“ 150 px breit, Platz sind 110, Gothic 28 fett braucht 87.
  // Darunter klein und mittig über der Uhrzeit Zone und UTC-Versatz
  const int x0 = K_ZEIT_X + K_ZEIT_L, rechts = K_ZEIT_X + k_text_w(KUHR, "00:00") - K_ZEIT_L;
  const int oben = 25, zeile2 = oben + K_G28_H + 10;   // Abstände je ~10 px: Schritte | Zeile 1 | Zeile 2 | Uhrzeit
  char zone[8];
  // Seit 0.19 ein Block mit normalem Wortabstand, mittig über der Uhrzeit (bis 0.18 links/rechtsbündig mit Lücke)
  snprintf(buf, sizeof buf, "%s %02d.%02d.%02d", WT[lokal.tm_wday], lokal.tm_mday, lokal.tm_mon + 1, lokal.tm_year % 100);   // Jahr seit 0.17
  k_text(KG28, buf, (x0 + rechts) / 2, oben, MITTE, K_SCHWARZ);
  zonen_kuerzel(&lokal, zone, sizeof zone);
  utc_versatz(lokal.tm_gmtoff, versatz, sizeof versatz);
  snprintf(buf, sizeof buf, "%s%s%s", zone, zone[0] ? "  " : "", versatz);
  if (k_text_w(KG14, buf) > rechts - x0) snprintf(buf, sizeof buf, "%s", versatz);
  k_text(KG14, buf, (x0 + rechts) / 2, zeile2, MITTE, K_SCHWARZ);

  k_mond(162, 79, 13);
  k_akku(186, 66);
  park_zeichnen(true);

  snprintf(buf, sizeof buf, "%02d%02dZ", utc.tm_hour, utc.tm_min);
  k_text(KG24, buf, 6, 114, LINKS, K_SCHWARZ);
  k_puls();
  k_diagramm(&lokal);
}
