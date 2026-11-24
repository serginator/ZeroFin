#!/bin/sh
# Build the ZeroFin .deb for the Cardputer Zero in an arm64 Debian 13 container.
# Needs Docker and an arm64 host (Apple Silicon, or an arm64 CI runner). Never build on the device.
# Usage: tools/build-deb.sh [output-dir]      (default: dist)
set -eu
cd "$(dirname "$0")/.."
OUT="${1:-dist}"
mkdir -p "$OUT"
OUT="$(cd "$OUT" && pwd)"

docker build --platform linux/arm64 -t zerofin-cp0-build -f packaging/Dockerfile packaging
# The named volume keeps the build directory, so a second build only compiles what changed.
docker run --rm --platform linux/arm64 \
  -v "$PWD":/src:ro -v zerofin-cp0-build:/work -v "$OUT":/out \
  -e HOST_UID="$(id -u)" -e HOST_GID="$(id -g)" \
  zerofin-cp0-build sh -c '
    set -e
    cmake -S /src -B /work/build -G Ninja -DCMAKE_BUILD_TYPE=Release -UZEROFIN_MAINTAINER -UZEROFIN_DEB_REVISION \
      -DUSE_DESKTOP=OFF -DZEROFIN_BUILD_TESTS=OFF -DZEROFIN_WITH_CURL=OFF
    cmake --build /work/build -j"$(nproc)"
    cd /work/build && cpack -G DEB -B /out
    rm -rf /out/_CPack_Packages
    chown "$HOST_UID:$HOST_GID" /out/zerofin_*.deb
  '
ls -l "$OUT"/zerofin_*.deb
