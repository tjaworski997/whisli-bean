#!/bin/bash
# Rebuilds docs/screens/{en,pl}: the real panel build, in headless Chrome, against scripts/panel-mock.mjs.
# Touches no device. Run after changing a page that the READMEs show.
set -euo pipefail
cd "$(dirname "$0")/.."
chrome="/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"
profile=$(mktemp -d)
(cd ui && npx vite build >/dev/null)

shot() { # port lang url out width height
  perl -e 'alarm 30; exec @ARGV' "$chrome" --headless=new --no-first-run --disable-gpu --hide-scrollbars --lang="$2" \
    --user-data-dir="$profile/$1" --force-device-scale-factor=1.5 --window-size="$5,$6" --virtual-time-budget=4000 \
    --screenshot="$PWD/$4" "http://localhost:$1/$3" >/dev/null 2>&1 || true
}

for entry in en:8101 pl:8102; do
  lang=${entry%:*}; port=${entry#*:}
  node scripts/panel-mock.mjs "$lang" "$port" ui/dist & mock=$!
  mkdir -p "docs/screens/$lang"
  while read -r name url height width; do
    shot "$port" "$lang" "$url" "docs/screens/$lang/$name.png" "${width:-1280}" "$height"
  done <<'LIST'
buttons #/buttons 720
button-edit #/buttons/NEC-4BBA-A0 1250
button-new #/buttons/new 760
webhooks ?expand#/webhooks 1000
wake-on-lan ?expand#/servers 720
receiver #/receiver 700
mqtt #/system/mqtt 1450
led #/system/led 720
network #/system/network 860
settings #/system 860
diagnostics #/system/diagnostics 1300
logs #/system/logs 620
dark ?dark#/buttons 720
phone #/buttons 1100 500
LIST
  kill "$mock"
done
rm -rf "$profile"
