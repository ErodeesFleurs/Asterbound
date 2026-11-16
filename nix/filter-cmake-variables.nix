# Keep the standard Nix CMake phases, but do not pass known unsupported -D
# variables. CMake still diagnoses unused variables that were later unset by -U.
variables:
''
  cmake() {
    local argument variable
    local -a filtered=()
    for argument in "$@"; do
      if [[ "$argument" == -D* ]]; then
        variable="''${argument#-D}"
        variable="''${variable%%[:=]*}"
        case "$variable" in
          ${builtins.concatStringsSep "|" variables}) continue ;;
        esac
      fi
      filtered+=("$argument")
    done
    command cmake "''${filtered[@]}"
  }
''
