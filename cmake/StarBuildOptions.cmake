# Add project defaults only when the caller has not selected that class of
# option. CMake's own configuration initializers are not caller overrides.
function(star_default_compile_options config kind)
  string(TOUPPER "${config}" config_upper)
  if(kind STREQUAL "optimization")
    set(override_pattern "(^|[ \t])-O[^ \t]*([ \t]|$)")
  elseif(kind STREQUAL "math")
    set(override_pattern "(^|[ \t])-f(no-)?fast-math([ \t]|$)")
  else()
    set(override_pattern "(^|[ \t])-g([0-9]+|line-tables-only)?([ \t]|$)")
  endif()
  foreach(language C CXX)
    set(flag_variable CMAKE_${language}_FLAGS_${config_upper})
    string(STRIP "${${flag_variable}}" config_flags)
    string(STRIP "${${flag_variable}_INIT}" initial_flags)
    set(caller_flags "${CMAKE_${language}_FLAGS}")
    if(STAR_EXPLICIT_${language}_FLAGS_${config_upper} OR NOT config_flags STREQUAL initial_flags)
      string(APPEND caller_flags " ${config_flags}")
    endif()
    if(NOT caller_flags MATCHES "${override_pattern}")
      foreach(option IN LISTS ARGN)
        target_compile_options(star_build_config INTERFACE
          "$<$<AND:$<COMPILE_LANGUAGE:${language}>,$<CONFIG:${config}>>:${option}>")
      endforeach()
    endif()
  endforeach()
endfunction()

if(STAR_ENABLE_THINLTO)
  include(CheckIPOSupported)
  check_ipo_supported(LANGUAGES C CXX)
  # CMake selects -flto=thin and the matching LLVM archive tools for xclang.
  # Initialize targets before their creation, including std modules and Lua.
  foreach(config Release RelWithDebInfo RelWithAsserts MinSizeRel)
    string(TOUPPER "${config}" config_upper)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_${config_upper} ON)
  endforeach()
endif()
message(STATUS "ThinLTO for optimized configurations: ${STAR_ENABLE_THINLTO}")

target_compile_definitions(star_build_config INTERFACE
  "$<$<CONFIG:Release,RelWithDebInfo>:NDEBUG>")

# This is an ABI requirement, not an overridable optimization default.
# Standard-library BMIs and all project providers/importers use the same RTTI
# and exception features, including the maintained Lua/ImGui adapter.
target_compile_options(star_build_config INTERFACE
  "$<$<COMPILE_LANGUAGE:CXX>:-fno-rtti;-fno-exceptions>")

target_compile_options(star_build_config INTERFACE
  "$<$<COMPILE_LANGUAGE:C,CXX>:-Wall;-Wextra;-Wuninitialized;-Wno-parentheses-equality;-Wno-deprecated-declarations;-Wno-nan-infinity-disabled>"
  "$<$<COMPILE_LANGUAGE:CXX>:-Werror=unused-result>")
if(NOT STAR_SYSTEM_WINDOWS)
  target_link_options(star_build_config INTERFACE "$<$<LINK_LANGUAGE:CXX>:-stdlib=libc++>")
endif()
if(STAR_SYSTEM_MACOS)
  target_link_options(star_build_config INTERFACE "LINKER:-export_dynamic")
elseif(NOT STAR_SYSTEM_WINDOWS)
  target_link_options(star_build_config INTERFACE "LINKER:--export-dynamic")
  # Driver DSOs must not bind to the engine's private mimalloc operators.
  # Keep other dynamic symbols available for diagnostics.
  set(star_allocator_symbols "${CMAKE_CURRENT_LIST_DIR}/StarAllocator.map")
  target_link_options(star_build_config INTERFACE "LINKER:--version-script=${star_allocator_symbols}")
  set_property(TARGET star_build_config APPEND PROPERTY INTERFACE_LINK_DEPENDS "${star_allocator_symbols}")
endif()

if(NOT STAR_SYSTEM_WINDOWS AND NOT STAR_SYSTEM_MACOS)
  target_compile_options(star_build_config INTERFACE "$<$<COMPILE_LANGUAGE:C,CXX>:-pthread>")
  target_compile_definitions(star_build_config INTERFACE _REENTRANT)
  target_link_options(star_build_config INTERFACE -pthread)
endif()

foreach(config RelWithAsserts RelWithDebInfo Release)
  star_default_compile_options(${config} optimization -O3)
  star_default_compile_options(${config} math -ffast-math)
endforeach()

# Clang serializes complex arithmetic mode in every BMI. Keep it stable when
# macOS color conversion opts out of scalar fast-math for saturation accuracy.
if(STAR_SYSTEM_MACOS)
  target_compile_options(star_build_config INTERFACE
    "$<$<COMPILE_LANGUAGE:CXX>:-fno-cx-limited-range>")
endif()

foreach(config Debug RelWithAsserts RelWithDebInfo)
  if(STAR_DEBUG_SYMBOL_LEVEL STREQUAL "minimal" OR
      (STAR_DEBUG_SYMBOL_LEVEL STREQUAL "default" AND config STREQUAL "RelWithDebInfo"))
    set(debug_option -gline-tables-only)
  else()
    set(debug_option -g)
  endif()
  if(STAR_SYSTEM_WINDOWS)
    # DbgHelp consumes native CodeView/PDB symbols, not GNU DWARF records.
    list(APPEND debug_option -gcodeview)
  endif()
  star_default_compile_options(${config} debug ${debug_option})
endforeach()
if(STAR_SYSTEM_WINDOWS)
  target_link_options(star_build_config INTERFACE
    "$<$<CONFIG:Debug,RelWithAsserts,RelWithDebInfo>:LINKER:--pdb=>")
else()
  target_compile_options(star_build_config INTERFACE
    "$<$<AND:$<COMPILE_LANGUAGE:C,CXX>,$<CONFIG:RelWithDebInfo>>:-gz=zlib>")
endif()

if(STAR_SYSTEM_MACOS)
  target_link_options(star_build_config INTERFACE
    "$<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,MODULE_LIBRARY>:-flat_namespace;-undefined;suppress>")
endif()

function(star_check_language_flags origin)
  if("${ARGN}" MATCHES "(^|[ \t\r\n;:>])(-frtti(-data)?|-f(cxx-|handle-)?exceptions)([ \t\r\n;<>]|$)")
    message(FATAL_ERROR "${origin} may not enable RTTI or exceptions in the project build.")
  endif()
endfunction()

# Run after every component has declared its targets. A new project target must
# join the same policy, and target/source options must not silently reverse it.
function(star_check_language_directory directory)
  get_directory_property(directory_options DIRECTORY "${directory}" COMPILE_OPTIONS)
  star_check_language_flags("${directory} compile options" "${directory_options}")
  get_directory_property(directory_variables DIRECTORY "${directory}" VARIABLES)
  foreach(variable IN LISTS directory_variables)
    if(variable MATCHES "^CMAKE_CXX_FLAGS($|_[A-Z0-9_]+$)")
      get_directory_property(flags DIRECTORY "${directory}" DEFINITION "${variable}")
      star_check_language_flags("${directory}: ${variable}" "${flags}")
    endif()
  endforeach()

  get_property(targets DIRECTORY "${directory}" PROPERTY BUILDSYSTEM_TARGETS)
  foreach(target IN LISTS targets)
    get_target_property(target_type "${target}" TYPE)
    if(target_type STREQUAL "UTILITY")
      continue()
    endif()
    if(NOT target_type STREQUAL "INTERFACE_LIBRARY")
      get_target_property(target_links "${target}" LINK_LIBRARIES)
      if(NOT "star_build_config" IN_LIST target_links)
        message(FATAL_ERROR "${target} must link star_build_config to enforce the project no-RTTI/no-exceptions policy.")
      endif()
    endif()
    foreach(property COMPILE_OPTIONS COMPILE_FLAGS INTERFACE_COMPILE_OPTIONS)
      get_target_property(flags "${target}" "${property}")
      star_check_language_flags("${target}: ${property}" "${flags}")
    endforeach()
    get_property(sources TARGET "${target}" PROPERTY SOURCES)
    get_property(module_sets TARGET "${target}" PROPERTY CXX_MODULE_SETS)
    foreach(module_set IN LISTS module_sets)
      if(module_set STREQUAL "CXX_MODULES")
        get_property(module_sources TARGET "${target}" PROPERTY CXX_MODULE_SET)
      else()
        get_property(module_sources TARGET "${target}" PROPERTY "CXX_MODULE_SET_${module_set}")
      endif()
      list(APPEND sources ${module_sources})
    endforeach()
    foreach(source IN LISTS sources)
      if(source MATCHES "^\\$<")
        continue()
      endif()
      foreach(property COMPILE_OPTIONS COMPILE_FLAGS)
        get_source_file_property(flags "${source}" TARGET_DIRECTORY "${target}" "${property}")
        star_check_language_flags("${target}: ${source}: ${property}" "${flags}")
      endforeach()
    endforeach()
  endforeach()

  # Imported libraries retain their own compilation policy, but may not export
  # flags that reverse the ABI of our source files.
  get_property(imported_targets DIRECTORY "${directory}" PROPERTY IMPORTED_TARGETS)
  foreach(target IN LISTS imported_targets)
    get_target_property(flags "${target}" INTERFACE_COMPILE_OPTIONS)
    star_check_language_flags("${target}: INTERFACE_COMPILE_OPTIONS" "${flags}")
  endforeach()
  get_property(subdirectories DIRECTORY "${directory}" PROPERTY SUBDIRECTORIES)
  foreach(subdirectory IN LISTS subdirectories)
    star_check_language_directory("${subdirectory}")
  endforeach()
endfunction()
