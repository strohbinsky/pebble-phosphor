#!/usr/bin/env python3
"""Erzeugt src/pkjs/flughaefen.js aus der OurAirports-Liste (gemeinfrei, davidmegginson.github.io/ourairports-data).
Auswahl „international“: type = large_airport, scheduled_service = yes, IATA-Code vorhanden.
Aufruf: python3 tools/flughaefen_erzeugen.py <airports.csv>   — legt zusätzlich die gefilterte Liste
als Rohquelle neben den App-Ordner (dateien/ourairports-gross-<datum>.csv)."""
import csv, sys, datetime
from pathlib import Path
quelle = sys.argv[1]
zeilen = [r for r in csv.DictReader(open(quelle, encoding="utf-8"))
          if r["type"] == "large_airport" and r["scheduled_service"] == "yes" and len(r["iata_code"]) == 3]
app = Path(__file__).resolve().parent.parent
daten = ";".join(f'{r["iata_code"]}{float(r["latitude_deg"]):.2f},{float(r["longitude_deg"]):.2f}' for r in zeilen)
(app / "src" / "pkjs" / "flughaefen.js").write_text(
    "// Automatisch erzeugt von tools/flughaefen_erzeugen.py — nicht von Hand ändern.\n"
    f"// {len(zeilen)} große Linienflughäfen aus OurAirports, Stand {datetime.date.today()}. Format: IATA + Breite,Länge; …\n"
    f"module.exports = '{daten}';\n", encoding="utf-8")
roh = app.parent / f"ourairports-gross-{datetime.date.today()}.csv"
with open(roh, "w", encoding="utf-8", newline="") as f:
    w = csv.writer(f); w.writerow(["iata", "name", "breite", "laenge", "land"])
    for r in zeilen: w.writerow([r["iata_code"], r["name"], r["latitude_deg"], r["longitude_deg"], r["iso_country"]])
print(len(zeilen), "Flughäfen,", len(daten), "Zeichen")
