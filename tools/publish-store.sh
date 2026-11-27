#!/bin/sh
# Publish a ZeroFin release to the CardputerZero Store with one command.
# It downloads the .deb that CI attached to the GitHub release, takes the Store listing (app-builder.json,
# screenshots, icon) from the same git tag, and runs `czdev publish`. That opens a PR in CardputerZero/packages.
# Later versions of an app merge there automatically.
#
# Usage: tools/publish-store.sh [tag]      (default: the latest release, for example v1.1.0)
# Needs: gh (logged in), git, czdev (logged in with `czdev login`) and dpkg-deb (macOS: brew install dpkg).
set -eu
cd "$(dirname "$0")/.."

for tool in gh git czdev dpkg-deb; do
  command -v "$tool" >/dev/null 2>&1 || { echo "publish-store: '$tool' not found. See the header of this script." >&2; exit 1; }
done
[ -f "$HOME/.czdev/credentials" ] || { echo "publish-store: not logged in. Run 'czdev login' first." >&2; exit 1; }

TAG="${1:-$(gh release view --json tagName -q .tagName)}"
echo "Publishing $TAG to the CardputerZero Store"

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

# czdev reads app-builder.json, the screenshots and the icon from the current folder. Take them from the
# tag, so the listing always matches the release and a dirty working tree cannot change it.
git fetch -q origin tag "$TAG" 2>/dev/null || true
git archive "$TAG" app-builder.json screenshots assets/images | tar -x -C "$WORK"

mkdir "$WORK/build"
gh release download "$TAG" --pattern 'zerofin_*.deb' --dir "$WORK/build"

cd "$WORK"
czdev publish
