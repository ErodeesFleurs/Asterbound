cmake_minimum_required(VERSION 3.23)
project(imgui VERSION 1.92.8 LANGUAGES CXX)

# Match the ImGui ABI in the OpenStarbound vcpkg baseline. Headless builds keep
# the Lua bindings and FreeType renderer, but do not need windowing backends.
option(IMGUI_BUILD_GUI_BACKENDS "Build SDL3 and OpenGL3 backends" ON)
find_package(Freetype REQUIRED)
if(IMGUI_BUILD_GUI_BACKENDS)
    find_package(SDL3 CONFIG REQUIRED)
    find_package(OpenGL REQUIRED)
endif()
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

add_library(imgui STATIC
    imgui.cpp
    imgui_demo.cpp
    imgui_draw.cpp
    imgui_tables.cpp
    imgui_widgets.cpp
    misc/cpp/imgui_stdlib.cpp
    misc/freetype/imgui_freetype.cpp
)
add_library(imgui::imgui ALIAS imgui)
target_compile_features(imgui PUBLIC cxx_std_11)
target_include_directories(imgui PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(imgui PUBLIC Freetype::Freetype)
if(IMGUI_BUILD_GUI_BACKENDS)
    add_library(imgui_backends STATIC
        backends/imgui_impl_sdl3.cpp
        backends/imgui_impl_opengl3.cpp
    )
    add_library(imgui::imgui_backends ALIAS imgui_backends)
    target_link_libraries(imgui_backends PUBLIC imgui SDL3::SDL3 OpenGL::GL ${CMAKE_DL_LIBS})
    install(TARGETS imgui_backends EXPORT imgui-targets ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()

install(TARGETS imgui EXPORT imgui-targets ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES
    imgui.h imconfig.h imgui_internal.h
    imstb_textedit.h imstb_rectpack.h imstb_truetype.h
    misc/cpp/imgui_stdlib.h misc/freetype/imgui_freetype.h
    backends/imgui_impl_sdl3.h backends/imgui_impl_opengl3.h
    backends/imgui_impl_opengl3_loader.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(EXPORT imgui-targets NAMESPACE imgui:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/imgui)
configure_package_config_file(imgui-config.cmake.in imgui-config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/imgui)
write_basic_package_version_file(imgui-config-version.cmake COMPATIBILITY ExactVersion)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/imgui-config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/imgui-config-version.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/imgui)
install(FILES LICENSE.txt DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/imgui)
