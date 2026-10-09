#!/bin/zsh
# Übersetzt main.c gegen den API-Nachbau und rendert eine Vorschau als PNG (doppelte Größe).
#   tools/vorschau/rendern.sh <ausgabe.png> [unixzeit]   — Umgebung wie in vorschau.c beschrieben
set -e
DIR="${0:A:h}"
cc -O1 -Wall -Wno-unused-function -I "$DIR" -o /tmp/phosphor-vorschau "$DIR/vorschau.c" -lm
/tmp/phosphor-vorschau "${2:-$(date +%s)}" /tmp/phosphor-vorschau.raw
python3 "$DIR/png.py" /tmp/phosphor-vorschau.raw "$1"
