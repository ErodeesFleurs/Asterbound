#!/bin/sh -e

mkdir client_distribution
mkdir client_distribution/assets
mkdir client_distribution/assets/user

./dist/asset_packer -c scripts/packing.config assets/asterbound client_distribution/assets/asterbound.pak
./dist/asset_packer -c scripts/packing.config assets/asterbound-compat client_distribution/assets/asterbound-compat.pak

mkdir client_distribution/mods
touch client_distribution/mods/mods_go_here

mkdir client_distribution/osx
cp -LR scripts/ci/macos/Asterbound.app client_distribution/osx/
app=client_distribution/osx/Asterbound.app
versions=$(python3 -c '
import json
v = json.load(open("version.json"))["version"]
parts = v.split(".")
if len(parts) != 4 or any(not p.isdecimal() or int(p) > 99 for p in parts):
    raise SystemExit("macOS packaging requires four version components in 0..99")
a, b, c, d = map(int, parts)
if a * 100 + b == 0:
    raise SystemExit("macOS build version must have a positive first component")
print(v, f"{a}.{b}.{c}", f"{a * 100 + b}.{c}.{d}")
')
set -- $versions
/usr/bin/plutil -replace CFBundleGetInfoString -string "Asterbound $1" "$app/Contents/Info.plist"
/usr/bin/plutil -replace CFBundleShortVersionString -string "$2" "$app/Contents/Info.plist"
/usr/bin/plutil -replace CFBundleVersion -string "$3" "$app/Contents/Info.plist"
mkdir "$app/Contents/MacOS" "$app/Contents/Resources"
cp dist/asterbound "$app/Contents/MacOS/"
cp dist/*.dylib "$app/Contents/MacOS/"

# Build the macOS icon from the same Asterbound artwork used by the client.
icon_work=$(mktemp -d)
trap 'rm -rf "$icon_work"' EXIT HUP INT TERM
iconset="$icon_work/asterbound.iconset"
mkdir "$iconset"
for size in 16 32 128 256 512; do
  sips -z "$size" "$size" source/client/asterbound.png --out "$iconset/icon_${size}x${size}.png" >/dev/null
  retina=$((size * 2))
  sips -z "$retina" "$retina" source/client/asterbound.png --out "$iconset/icon_${size}x${size}@2x.png" >/dev/null
done
iconutil -c icns "$iconset" -o "$app/Contents/Resources/asterbound.icns"
cp \
  dist/OPENSOURCE.md \
  dist/asterbound_server \
  dist/btree_repacker \
  dist/asset_packer \
  dist/asset_unpacker \
  dist/dump_versioned_json \
  dist/make_versioned_json \
  scripts/ci/macos/sbinit.config \
  scripts/ci/macos/run-server.sh \
  scripts/steam_appid.txt \
  client_distribution/osx/

tarlz -c3vf client.tar.lz client_distribution
