#!/bin/sh -e

mkdir client_distribution
mkdir client_distribution/assets
mkdir client_distribution/assets/user

./dist/asset_packer -c scripts/packing.config assets/asterbound client_distribution/assets/asterbound.pak
./dist/asset_packer -c scripts/packing.config assets/asterbound-compat client_distribution/assets/asterbound-compat.pak

mkdir client_distribution/mods
touch client_distribution/mods/mods_go_here

mkdir client_distribution/linux
cp \
  dist/OPENSOURCE.md \
  dist/asterbound \
  dist/btree_repacker \
  dist/asset_packer \
  dist/asset_unpacker \
  dist/dump_versioned_json \
  dist/make_versioned_json \
  lib/linux/libdiscord_game_sdk.so \
  lib/linux/libsteam_api.so \
  scripts/ci/linux/sbinit.config \
  scripts/ci/linux/run-client.sh \
  scripts/steam_appid.txt \
  client_distribution/linux/

  
mkdir client_distribution/linux/.icon
cp \
  source/client/asterbound.png \
  client_distribution/linux/.icon/

mkdir server_distribution
mkdir server_distribution/assets

mkdir server_distribution/mods
touch server_distribution/mods/mods_go_here

./dist/asset_packer -c scripts/packing.config -s assets/asterbound server_distribution/assets/asterbound.pak
./dist/asset_packer -c scripts/packing.config -s assets/asterbound-compat server_distribution/assets/asterbound-compat.pak

mkdir server_distribution/linux

cp \
  dist/OPENSOURCE.md \
  dist/asterbound_server \
  dist/btree_repacker \
  scripts/ci/linux/run-server.sh \
  scripts/ci/linux/sbinit.config \
  scripts/steam_appid.txt \
  server_distribution/linux/

tarlz -c3vf client.tar.lz client_distribution
tarlz -c3vf server.tar.lz server_distribution
