# C++ APIs must stay in the executable: xclang's hidden, static libc++ runtime
# is private to each shared object. Only libraries with C ABIs may be shared.
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_BUILD_TYPE release)
set(VCPKG_LOAD_VCVARS_ENV OFF)
set(VCPKG_ENV_PASSTHROUGH_UNTRACKED XCLANG_ROOT MSYS2_ROOT MAKEFLAGS)
set(VCPKG_ENV_PASSTHROUGH OSB_NATIVE_COMPILER_ROOT)
set(VCPKG_HASH_ADDITIONAL_FILES
    "${CMAKE_CURRENT_LIST_FILE}"
    "${CMAKE_CURRENT_LIST_DIR}/toolchain.cmake"
    "${CMAKE_CURRENT_LIST_DIR}/../scripts/ci/install-xclang.cmake"
    "${CMAKE_CURRENT_LIST_DIR}/../scripts/ci/configure-native-linux-sdk.cmake")
if(PORT STREQUAL "qt5-base")
    list(APPEND VCPKG_HASH_ADDITIONAL_FILES
        "${CMAKE_CURRENT_LIST_DIR}/../cmake/patches/qtbase-libcxx.patch")
endif()
if(NOT EXISTS "$ENV{XCLANG_ROOT}/lib/cmake/xclang/toolchain.cmake")
    message(FATAL_ERROR "Set XCLANG_ROOT to the unpacked xclang 23.1.3.6 toolchain")
endif()
set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/toolchain.cmake")
set(OSB_COMPILER_ROOT "$ENV{XCLANG_ROOT}")
# Driver configs contain the native C SDK identity as well as its flags.
# Compiler binary/version hashes alone do not capture this implicit input.
if(EXISTS "$ENV{XCLANG_ROOT}/bin/${XCLANG_TARGET}.cfg")
    list(APPEND VCPKG_HASH_ADDITIONAL_FILES
        "$ENV{XCLANG_ROOT}/bin/${XCLANG_TARGET}.cfg")
endif()
# CMAKE_HOST_SYSTEM_PROCESSOR is unset when vcpkg reads triplets in script mode.
cmake_host_system_information(RESULT OSB_HOST_PROCESSOR QUERY OS_PLATFORM)
if(OSB_HOST_PROCESSOR MATCHES "^(arm64|aarch64|ARM64)$")
    set(OSB_HOST_PROCESSOR aarch64)
endif()
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux"
        AND XCLANG_TARGET STREQUAL "${OSB_HOST_PROCESSOR}-unknown-linux-gnu"
        AND DEFINED ENV{OSB_NATIVE_COMPILER_ROOT}
        AND NOT "$ENV{OSB_NATIVE_COMPILER_ROOT}" STREQUAL "")
    set(OSB_COMPILER_ROOT "$ENV{OSB_NATIVE_COMPILER_ROOT}")
endif()
# Chainloading bypasses vcpkg's platform toolchain, including its CPU setup.
string(REGEX MATCH "^[^-]+" OSB_TARGET_PROCESSOR "${XCLANG_TARGET}")
# Compiler targets reach both CMake ports and the autotools/Qt compiler probes.
list(APPEND VCPKG_CMAKE_CONFIGURE_OPTIONS
    "-DOSB_COMPILER_ROOT=${OSB_COMPILER_ROOT}"
    "-DXCLANG_TARGET=${XCLANG_TARGET}"
    "-DCMAKE_C_COMPILER_TARGET=${XCLANG_TARGET}"
    "-DCMAKE_CXX_COMPILER_TARGET=${XCLANG_TARGET}"
    "-DCMAKE_SYSTEM_PROCESSOR=${OSB_TARGET_PROCESSOR}")
# Some upstream generators invoke the compiler with CFLAGS directly, without
# CMake's COMPILER_TARGET (for example libpng's genout.cmake preprocessing).
string(APPEND VCPKG_C_FLAGS " --target=${XCLANG_TARGET}")
string(APPEND VCPKG_CXX_FLAGS " --target=${XCLANG_TARGET}")
# The official chainloaded toolchain does not consume vcpkg's flag variables.
# Forward them explicitly for CMake ports as well as autotools/qmake probes.
list(APPEND VCPKG_CMAKE_CONFIGURE_OPTIONS
    "-DCMAKE_C_FLAGS=${VCPKG_C_FLAGS}"
    "-DCMAKE_CXX_FLAGS=${VCPKG_CXX_FLAGS}")

# Qt5's qmake bootstrap must not auto-detect MSVC or select a GNU mkspec.
if(VCPKG_CMAKE_SYSTEM_NAME STREQUAL "MinGW")
    # MinGW is vcpkg's platform name; CMake's platform is Windows. The official
    # xclang file replaces vcpkg's mingw.cmake, which normally translates this.
    list(APPEND VCPKG_CMAKE_CONFIGURE_OPTIONS "-DCMAKE_SYSTEM_NAME=Windows")
    set(VCPKG_QT_TARGET_MKSPEC win32-clang-g++)
    # Compiler executable names and PATH follow the host, not the target.
    if(CMAKE_HOST_WIN32)
        if(DEFINED ENV{MSYS2_ROOT})
            set(ENV{PATH} "$ENV{XCLANG_ROOT}/bin;$ENV{MSYS2_ROOT}/ucrt64/bin;$ENV{PATH}")
        else()
            set(ENV{PATH} "$ENV{XCLANG_ROOT}/bin;$ENV{PATH}")
        endif()
        set(host_exe ".exe")
    else()
        set(ENV{PATH} "$ENV{XCLANG_ROOT}/bin:$ENV{PATH}")
        set(host_exe "")
    endif()
    if(PORT STREQUAL "qt5-base")
        set(ENV{CC} "$ENV{XCLANG_ROOT}/bin/clang${host_exe}")
        set(ENV{CXX} "$ENV{XCLANG_ROOT}/bin/clang++${host_exe}")
        # The MinGW mkspec otherwise hardcodes GNU ar for static libraries.
        set(ENV{QMAKEFLAGS} "-late \"QMAKE_LIB=llvm-ar -rc\" \"QMAKE_AR=llvm-ar -rc\" \"QMAKE_OBJCOPY=llvm-objcopy\"")
    endif()
elseif(VCPKG_CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(VCPKG_QT_TARGET_MKSPEC linux-clang)
    set(VCPKG_QT_HOST_MKSPEC linux-clang)
    if(PORT STREQUAL "qt5-base")
        set(ENV{CC} "${OSB_COMPILER_ROOT}/bin/clang")
        set(ENV{CXX} "${OSB_COMPILER_ROOT}/bin/clang++")
        set(ENV{PATH} "${OSB_COMPILER_ROOT}/bin:$ENV{PATH}")
    endif()
elseif(VCPKG_CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(VCPKG_QT_TARGET_MKSPEC macx-clang)
    set(VCPKG_QT_HOST_MKSPEC macx-clang)
    set(VCPKG_OSX_DEPLOYMENT_TARGET 14.0)
endif()
if(CMAKE_HOST_WIN32)
    set(VCPKG_QT_HOST_MKSPEC win32-clang-g++)
elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
    set(VCPKG_QT_HOST_MKSPEC linux-clang)
elseif(CMAKE_HOST_APPLE)
    set(VCPKG_QT_HOST_MKSPEC macx-clang)
endif()
if(PORT MATCHES "^discord-")
    set(VCPKG_LIBRARY_LINKAGE dynamic)
endif()
if(VCPKG_CMAKE_SYSTEM_NAME STREQUAL "MinGW" AND PORT STREQUAL "sdl3")
    set(VCPKG_LIBRARY_LINKAGE dynamic)
endif()

if(PORT STREQUAL "opus")
    list(APPEND VCPKG_CMAKE_CONFIGURE_OPTIONS
        "-DOPUS_INSTALL_PKG_CONFIG_MODULE=OFF"
        "-DOPUS_INSTALL_CMAKE_CONFIG_MODULE=ON"
        "-DOPUS_ENABLE_FLOAT_API=ON"
        "-DOPUS_FLOAT_APPROX=ON"
        "-DOPUS_STACK_PROTECTOR=OFF"
        "-DOPUS_NONTHREADSAFE_PSEUDOSTACK=OFF"
        "-DOPUS_USE_ALLOCA=ON"
        "-DBUILD_TESTING=OFF")
    if(VCPKG_TARGET_ARCHITECTURE STREQUAL "x64")
        list(APPEND VCPKG_CMAKE_CONFIGURE_OPTIONS
            "-DOPUS_X86_MAY_HAVE_SSE=ON"
            "-DOPUS_X86_MAY_HAVE_AVX=ON"
            "-DOPUS_X86_MAY_HAVE_SSE4_1=ON")
    else()
        list(APPEND VCPKG_CMAKE_CONFIGURE_OPTIONS
            "-DOPUS_ARM_MAY_HAVE_NEON=ON"
            "-DOPUS_ARM_MAY_HAVE_NEON_INTR=ON")
    endif()
endif()
