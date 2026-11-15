#!/usr/bin/env bash
# Builds docs/Eightfold-Manual.pdf from docs/MANUAL.md. Needs pandoc and Node with Playwright (Chromium).
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
tmp="$(mktemp -d)"
cp "$root/scripts/manual/style.css" "$tmp/style.css"
pandoc "$root/docs/MANUAL.md" -s --embed-resources --resource-path="$root/docs:$tmp" \
  --metadata title="Eightfold for VCV Rack: manual" -c style.css -o "$tmp/manual.html"
# drop pandoc's title block (the manual has its own heading)
perl -0pi -e 's/<header id="title-block-header">.*?<\/header>//s' "$tmp/manual.html"
node "$root/scripts/manual/topdf.js" "$tmp/manual.html" "$root/docs/Eightfold-Manual.pdf"
rm -rf "$tmp"
echo "wrote docs/Eightfold-Manual.pdf"
