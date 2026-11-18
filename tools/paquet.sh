#!/bin/bash
# Build the installer inside the project. Does not install or modify Library.
# The current panel is embedded in the compiled binaries.
set -euo pipefail
cd "$(dirname "$0")/.."
PROJECT_DIR="$(pwd -P)"
case "$PROJECT_DIR" in /|"$HOME") echo "Refusing packaging outside a project."; exit 1;; esac
for directory in build .local-cache; do
  [ ! -L "$directory" ] || { echo "Refusing symlink: $directory"; exit 1; }
done
mkdir -p build .local-cache
export TMPDIR="$PROJECT_DIR/.local-cache"
VERSION=$(sed -n 's/^project(Opera VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt)
[ -n "$VERSION" ] || { echo "Missing project version"; exit 1; }
OUT="build/Opera_artefacts/Release"
PKG="$PROJECT_DIR/build/Opera-$VERSION.pkg"
[ ! -L "$PKG" ] && [ ! -L "$PKG.payload.json" ] || { echo "Refusing output symlink"; exit 1; }
for item in "$OUT/AU/Opera.component" "$OUT/VST3/Opera.vst3" "$OUT/Standalone/Opera.app"; do
  [ -d "$item" ] || { echo "Missing: $item; build first."; exit 1; }
done
# Refuse stale or host-only builds before producing a distributable package.
for bundle in "$OUT/AU/Opera.component" "$OUT/VST3/Opera.vst3" "$OUT/Standalone/Opera.app"; do
  for arch in arm64 x86_64; do lipo "$bundle/Contents/MacOS/Opera" -verify_arch "$arch"; done
  actual=$(/usr/libexec/PlistBuddy -c 'Print :CFBundleShortVersionString' "$bundle/Contents/Info.plist")
  [ "$actual" = "$VERSION" ] || { echo "Stale bundle version: $bundle ($actual)"; exit 1; }
done
python3 - "$OUT" <<'PYTARGET'
import re, subprocess, sys
from pathlib import Path
root = Path(sys.argv[1])
for kind, name in (("AU", "Opera.component"), ("VST3", "Opera.vst3"), ("Standalone", "Opera.app")):
    binary = root / kind / name / "Contents/MacOS/Opera"
    headers = subprocess.check_output(["otool", "-l", str(binary)], text=True)
    minimums = re.findall(r"\bminos ([0-9.]+)", headers)
    if len(minimums) != 2 or any(tuple(map(int, value.split(".")[:2])) > (11, 0) for value in minimums):
        raise SystemExit(f"Unexpected deployment targets in {binary}: {minimums}")
PYTARGET
WORK=$(mktemp -d "$PROJECT_DIR/build/pkg-stage.XXXXXX")
trap 'rm -rf "$WORK"' EXIT
STAGE="$WORK/root"
mkdir -p "$STAGE/Library/Audio/Plug-Ins/Components" "$STAGE/Library/Audio/Plug-Ins/VST3" "$STAGE/Applications"
ditto "$OUT/AU/Opera.component" "$STAGE/Library/Audio/Plug-Ins/Components/Opera.component"
ditto "$OUT/VST3/Opera.vst3" "$STAGE/Library/Audio/Plug-Ins/VST3/Opera.vst3"
ditto "$OUT/Standalone/Opera.app" "$STAGE/Applications/Opera.app"
SOUNDS="$STAGE/Library/Application Support/SF Instruments/Opera/Presets/Sounds"
mkdir -p "$SOUNDS"
count=0
for sound in Presets/Sounds/*.orbitpreset; do
  [ -f "$sound" ] || continue
  cp "$sound" "$SOUNDS/"
  count=$((count + 1))
done
echo "Sons inclus : $count"
find "$STAGE" -name ".DS_Store" -delete
xattr -cr "$STAGE"
chmod -R u+w,go+rX "$STAGE"
# Re-sign staged bundles after the VST3 manifest/resource generation has finished.
for bundle in "$STAGE/Applications/Opera.app" "$STAGE/Library/Audio/Plug-Ins/Components/Opera.component" "$STAGE/Library/Audio/Plug-Ins/VST3/Opera.vst3"; do
  codesign --force --deep --sign - "$bundle"
  codesign --verify --deep --strict "$bundle"
done
python3 - "$STAGE" "$PKG.payload.json" <<'PYMANIFEST'
import hashlib, json, sys
from pathlib import Path
root = Path(sys.argv[1])
manifest = {str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in root.rglob('*') if p.is_file()}
Path(sys.argv[2]).write_text(json.dumps(manifest, indent=2))
PYMANIFEST
# Keep the installation at its intended destination, rather than relocating to a build copy.
PLIST="$WORK/components.plist"
pkgbuild --analyze --root "$STAGE" "$PLIST" >/dev/null
python3 - "$PLIST" <<'PYCOMPONENTS'
import plistlib, sys
from pathlib import Path
path = Path(sys.argv[1])
components = plistlib.loads(path.read_bytes())
for component in components:
    component["BundleIsRelocatable"] = False
    component["BundleIsVersionChecked"] = False
path.write_bytes(plistlib.dumps(components))
PYCOMPONENTS
pkgbuild --root "$STAGE" --component-plist "$PLIST" --identifier com.sfinstruments.opera.pkg --version "$VERSION" --install-location / "$WORK/Opera.pkg"
mv -f "$WORK/Opera.pkg" "$PKG"
echo "Installeur prêt : $PKG"
