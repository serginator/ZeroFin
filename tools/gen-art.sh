#!/bin/sh
# Make the app images from the two big source pictures. Needs ImageMagick (magick).
# Usage: tools/gen-art.sh <banner-source> <logo-source>
#   banner-source: wide picture (any size). Gives assets/images/banner.jpg (README) and banner-splash.jpg (320x170, the start banner).
#   logo-source:   square picture. Gives assets/images/zerofin.png (100 px launcher icon) and zerofin-256.png (Store icon) and zerofin-512.png.
# The start banner must stay a baseline JPEG: LVGL decodes only that kind.
set -eu
[ $# -eq 2 ] || { sed -n 2,7p "$0"; exit 1; }
OUT="$(cd "$(dirname "$0")/.." && pwd)/assets/images"
magick "$1" -filter Lanczos -resize 1280x -strip -interlace none -sampling-factor 4:2:0 -quality 85 "$OUT/banner.jpg"
magick "$1" -filter Lanczos -resize 320x170! -strip -interlace none -sampling-factor 4:2:0 -quality 88 "$OUT/banner-splash.jpg"
magick "$2" -filter Lanczos -resize 512x512 -strip -colors 192 -define png:compression-level=9 "$OUT/zerofin-512.png"
magick "$2" -filter Lanczos -resize 256x256 -strip -colors 192 -define png:compression-level=9 "$OUT/zerofin-256.png"
magick "$2" -filter Lanczos -resize 100x100 -strip -colors 128 -define png:compression-level=9 "$OUT/zerofin.png"
ls -l "$OUT"
