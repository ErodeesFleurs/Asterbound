{
  lib,
  stdenvNoCC,
  fetchurl,
  patchelf,
  bintools,
  wrapCCWith,
  sanitizers ? false,
  sanitizerRuntime ? null,
}:
let
  version = "23.1.3.6";
  hosts = {
    x86_64-linux = {
      triple = "x86_64-unknown-linux-gnu";
      hash = "sha256-zcsJdb/HZ2+dBDwPzuZ7oA5wDm7ti/LZ9hU972LBMW0=";
    };
    aarch64-linux = {
      triple = "aarch64-unknown-linux-gnu";
      hash = "sha256-/h8WhJvr2VY0ci88KW8AF/VvwfFIeGiXY0JMxUpmEYw=";
    };
  };
  host = hosts.${stdenvNoCC.hostPlatform.system};
  toolchain = stdenvNoCC.mkDerivation {
    pname = "xclang";
    inherit version;
    src = fetchurl {
      url = "https://github.com/clice-io/xclang/releases/download/${version}/xclang-${version}-${host.triple}.tar.xz";
      inherit (host) hash;
    };
    nativeBuildInputs = [ patchelf ];
    dontConfigure = true;
    dontBuild = true;
    # Keep the compiler's resource directory and target archives intact.
    dontStrip = true;
    dontPatchELF = true;
    installPhase = ''
      runHook preInstall
      mkdir -p "$out"
      cp -a . "$out/"
      # Standalone helpers such as FileCheck are not llvm multicall symlinks.
      # Normalize every dynamic host executable, without touching sysroots.
      for tool in "$out"/bin/*; do
        if [[ -f "$tool" && ! -L "$tool" ]] &&
            patchelf --print-interpreter "$tool" >/dev/null 2>&1; then
          patchelf --set-interpreter ${lib.escapeShellArg bintools.dynamicLinker} "$tool"
        fi
      done
      runHook postInstall
    '';
    passthru = {
      isClang = true;
      langC = true;
      langCC = true;
    };
    meta = {
      description = "PGO and ThinLTO optimized Clang toolchain";
      homepage = "https://github.com/clice-io/xclang";
      license = lib.licenses.asl20;
      platforms = builtins.attrNames hosts;
    };
  };
  sanitizerInclude =
    if sanitizerRuntime != null then
      "${sanitizerRuntime}/include"
    else
      "${toolchain}/${host.triple}/usr/lib/asan/include";
  sanitizerLibraryDir =
    if sanitizerRuntime != null then
      "${sanitizerRuntime}/lib"
    else
      "${toolchain}/${host.triple}/usr/lib/asan";
in
(wrapCCWith {
  cc = toolchain;
  inherit bintools;
  useCcForLibs = false;
  gccForLibs = null;
  libcxx = toolchain;
  # Keep Nix's glibc, but use the archive's headers and hermetic static
  # libc++/libc++abi/libunwind. C++ dependencies must also be linked statically.
  extraBuildCommands = ''
    echo '--no-default-config -resource-dir=${toolchain}/lib/clang/23' >> "$out/nix-support/cc-cflags"
    # cc-wrapper also supplies C++ defaults when clang is used as a C driver.
    # Driver configuration keeps these includes C++-only, as in xclang's own
    # target configs, without unused C++ command-line arguments in C probes.
    cat > "$out/nix-support/libcxx-headers.cfg" <<'EOF'
    ${lib.optionalString sanitizers "-stdlib++-isystem ${sanitizerInclude}"}
    -stdlib++-isystem ${toolchain}/libc++/include/${host.triple}/c++/v1
    -stdlib++-isystem ${toolchain}/libc++/include/c++/v1
    EOF
    echo "--config=$out/nix-support/libcxx-headers.cfg" >> "$out/nix-support/libcxx-cxxflags"
    echo '-stdlib=libc++ ${
      if sanitizers then
        "-nostdlib++ ${sanitizerLibraryDir}/libc++.a"
      else
        "-static-libstdc++"
    }' >> "$out/nix-support/libcxx-ldflags"
    ${lib.optionalString (sanitizers && sanitizerRuntime != null) ''
      echo '-L${sanitizerLibraryDir}' >> "$out/nix-support/cc-ldflags"
    ''}
    echo '-L${toolchain}/${host.triple}/usr/lib' >> "$out/nix-support/cc-ldflags"
    cat > "$out/nix-support/cc-wrapper-hook" <<'EOF'
    if [[ "$dontLink" != 1 ]]; then
      extraAfter+=(-rtlib=compiler-rt -unwindlib=libunwind -static-libgcc -fuse-ld=lld)
    fi
    EOF
    # clang-scan-deps parses the command after "--" itself, without executing
    # the CC wrapper. Give that command the same toolchain/header flags.
    cat > "$out/bin/clang-scan-deps" <<'EOF'
    #!${stdenvNoCC.shell}
    set -euf
    scan_args=()
    while (( $# )) && [[ "$1" != "--" ]]; do
      scan_args+=("$1")
      shift
    done
    if (( $# == 0 )); then
      exec ${toolchain}/bin/clang-scan-deps "''${scan_args[@]}"
    fi
    shift
    compiler="$1"
    shift
    # Reuse Nix's flag and hardening loaders, including their before/after
    # ordering. PCH scanning must agree with compilation about PIC and PIE.
    source @wrapper@/nix-support/utils.bash
    path_backup="$PATH"
    cInclude=1
    source ${bintools}/nix-support/add-flags.sh
    source @wrapper@/nix-support/add-flags.sh
    source @wrapper@/nix-support/add-hardening.sh
    suffix=${lib.replaceStrings [ "-" "." ] [ "_" "_" ] stdenvNoCC.hostPlatform.config}
    compile_flags="NIX_CFLAGS_COMPILE_$suffix"
    before_flags="NIX_CFLAGS_COMPILE_BEFORE_$suffix"
    stdlib_flags="NIX_CXXSTDLIB_COMPILE_$suffix"
    extraBefore=("''${hardeningCFlagsBefore[@]}" ''${!before_flags})
    extraAfter=("''${hardeningCFlagsAfter[@]}" ''${!compile_flags} ''${!stdlib_flags})
    params=("$@")
    source @wrapper@/nix-support/add-local-cc-cflags-before.sh
    exec ${toolchain}/bin/clang-scan-deps "''${scan_args[@]}" -- "$compiler" \
      "''${extraBefore[@]}" "$@" "''${extraAfter[@]}"
    EOF
    substituteInPlace "$out/bin/clang-scan-deps" --replace-fail '@wrapper@' "$out"
    chmod +x "$out/bin/clang-scan-deps"
  '';
}).overrideAttrs
  (old: {
    passthru = (old.passthru or { }) // {
      bundledRuntime = toolchain;
      xclangVersion = version;
      inherit sanitizers;
      inherit sanitizerRuntime;
    };
  })
