set (star_extern_HEADERS
    imgui_iterator.inl
    imgui_lua_bindings.hpp
    xxhash.h
  )

set (star_extern_SOURCES
    xxhash.c
    imgui_lua_bindings.cpp
  )

add_library (star_extern OBJECT ${star_extern_SOURCES} ${star_extern_HEADERS})
target_include_directories (star_extern PRIVATE
    ${STAR_EXTERN_INCLUDES}
  )
target_link_libraries (star_extern PRIVATE star_build_config star_extern_dependencies)
target_link_libraries (star_extern PRIVATE star_std)
set_target_properties (star_extern PROPERTIES CXX_SCAN_FOR_MODULES ON)
