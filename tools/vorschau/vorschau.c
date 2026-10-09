// Rendert das Display von main.c mit Testwerten in eine Rohdatei (200x228 RGB); png.py macht daraus ein PNG.
// Aufruf: TZ=Europe/Berlin vorschau <unixzeit> <ausgabe.raw>
// Umgebung: SPRACHE=de (Standard Englisch), SKIN=0..3, PARK1=P43 PARK2=2-3, SEKUNDEN=1 (Sekundenanzeige an), PULS_TAKT (z. B. 10 = nur jede 10. Minute ein Wert), AKKU (70), SCHRITTE (8432), IATA (FRA), BREITE (5003), OHNE_WETTER=1, FEHLER=1/2 (Hinweis ohne Wetter), OHNE_HANDY=1, OHNE_PULS=1, WETTER=<datei 144 Bytes>, GEZEITEN=<datei 48 Bytes>,
//           TAG (JJJJMMTT, Standard: heute), SONNE="<auf>,<unter>,<auf2>,<unter2>" (UTC-Sekunden, Standard: Frankfurt 07.10.2026)
#define main pebble_main
#include "../../src/c/main.c"
#undef main
uint8_t FB[228][200][3];
time_t FAKE_NOW;
int FAKE_AKKU = 70, FAKE_SCHRITTE = 8432, FAKE_PULS[360];
static int env(const char *k, int std) { return getenv(k) ? atoi(getenv(k)) : std; }
int main(int argc, char **argv) {
  FAKE_NOW = atol(argv[1]);
  FAKE_AKKU = env("AKKU", 70);
  FAKE_SCHRITTE = env("SCHRITTE", 8432);
  for (int i = 0; i < 360; i++) {          // Ruhe ~62, Spaziergang 2,5 h vor jetzt
    const double h = i / 60.0;
    FAKE_PULS[i] = getenv("OHNE_PULS") ? 0 : (int)(62 + 45 * exp(-pow((h - 3.3) / 0.6, 2)) + 4 * sin(h * 7));
    if (getenv("PULS_TAKT") && i % atoi(getenv("PULS_TAKT"))) FAKE_PULS[i] = 0;   // nur jede n-te Minute gemessen
  }
  if (!getenv("OHNE_WETTER")) {
    FILE *f = fopen(getenv("WETTER") ? getenv("WETTER") : "/dev/null", "rb");
    if (f && fread(s_wetter, 1, WETTER_LEN, f) == WETTER_LEN) s_hat_wetter = true;
    if (f) fclose(f);
    s_tag = env("TAG", tag_von(localtime(&FAKE_NOW)));
    s_stand = FAKE_NOW - 600;
    sscanf(getenv("SONNE") ? getenv("SONNE") : "1791351240,1791391920,1791437700,1791478200", "%d,%d,%d,%d", &s_sonne[0], &s_sonne[1], &s_sonne[2], &s_sonne[3]);
  }
  for (int i = 0; i < 48; i++) s_gezeiten[i] = -128;
  if (getenv("GEZEITEN")) { FILE *gz = fopen(getenv("GEZEITEN"), "rb"); if (gz) { if (fread(s_gezeiten, 1, 48, gz) != 48) s_gezeiten[0] = -128; fclose(gz); } }
  if (getenv("KLAR")) s_skin = SKIN_KLAR;
  if (getenv("SKIN")) s_skin = atoi(getenv("SKIN"));
  s_de = getenv("SPRACHE") && !strcmp(getenv("SPRACHE"), "de");
  if (getenv("PARK1")) snprintf(s_park[0], sizeof s_park[0], "%s", getenv("PARK1"));
  if (getenv("PARK2")) snprintf(s_park[1], sizeof s_park[1], "%s", getenv("PARK2"));
  if (getenv("SEKUNDEN")) s_sekunden = true;
  if (getenv("FEHLER")) s_fehler = atoi(getenv("FEHLER"));   // 1 Standort, 2 Netz — Hinweis ohne Wetter
  snprintf(s_iata, sizeof s_iata, "%s", getenv("IATA") ? getenv("IATA") : "FRA");
  s_breite = env("BREITE", 5003);
  puls_laden();
  zeichnen(NULL, NULL);
  FILE *f = fopen(argv[2], "wb"); fwrite(FB, 1, sizeof FB, f); fclose(f);
  return 0;
}
