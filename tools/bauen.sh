#!/bin/zsh
# Baut das Watchface außerhalb des Quellordners (~/.local/share/phosphor-build) und installiert es.
#   tools/bauen.sh            nur bauen
#   tools/bauen.sh emu        bauen + Emulator emery + Logs
#   tools/bauen.sh uhr <IP>   bauen + über das Handy im WLAN (Entwicklerverbindung in der Pebble-App)
# Die fertige App liegt danach als phosphor.pbw neben dem Quellordner (Ordner "dateien", per Sync aufs Handy).
set -e
export PATH="$HOME/.local/bin:$PATH"
QUELLE="${0:A:h:h}/"
ZIEL="$HOME/.local/share/phosphor-build"
mkdir -p "$ZIEL"
if [ "${QUELLE:h:t}" = "dateien" ]; then AUSGABE="${QUELLE}../phosphor.pbw"; else AUSGABE="${QUELLE}dist/phosphor.pbw"; fi
python3 "${QUELLE}tools/schrift_erzeugen.py"
rsync -a --delete --exclude build --exclude tools --exclude dist --exclude .git "$QUELLE" "$ZIEL/"
cd "$ZIEL"
rm -rf build   # pebble clean reicht nicht: rsync --delete entfernt die waf-Statusdatei, dann bleibt build/ veraltet
pebble build 2>&1 | grep -v "PebblePacket\|Represents" | tail -3
# Quellkarte (Debug) raus: enthält Build-Pfade mit dem Benutzernamen des Rechners, das Watchface braucht sie nicht
zip -q -d build/*.pbw pebble-js-app.js.map >/dev/null 2>&1 || true
# Sperre gegen persönliche Daten (Begriffsliste außerhalb des Repos), falls auf diesem Rechner eingerichtet
if [ -x "$HOME/.local/bin/github-sperre" ] && ! "$HOME/.local/bin/github-sperre" build/*.pbw >/dev/null; then
  echo "ABBRUCH: github-sperre schlägt bei der gebauten App an — nicht kopiert"
  "$HOME/.local/bin/github-sperre" build/*.pbw
  exit 1
fi
mkdir -p "${AUSGABE:h}"
cp build/*.pbw "$AUSGABE"
case "$1" in
  emu) pebble install --emulator emery --logs ;;
  uhr) pebble install --phone "$2" --logs ;;
esac
