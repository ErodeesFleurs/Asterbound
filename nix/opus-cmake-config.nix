{
  lib,
  stdenv,
  libopus,
}:

# nixpkgs' opus has no CMake package config; see nix/opus/OpusConfig.cmake.
stdenv.mkDerivation {
  pname = "opus-cmake-config";
  version = lib.getVersion libopus;

  dontUnpack = true;
  dontConfigure = true;
  dontBuild = true;
  strictDeps = true;

  installPhase = ''
    runHook preInstall
    install -Dm644 ${./opus/OpusConfig.cmake} "$out/lib/cmake/Opus/OpusConfig.cmake"
    runHook postInstall
  '';

  meta = {
    description = "CMake package config for nixpkgs' opus, for Asterbound's find_package(Opus CONFIG)";
    license = libopus.meta.license;
    platforms = libopus.meta.platforms;
  };
}
