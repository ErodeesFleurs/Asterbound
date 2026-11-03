set (star_mod_uploader_SOURCES
    StarModUploader.cpp
    StarSPlainTextEdit.cpp
    main.cpp
  )

add_executable (mod_uploader WIN32
  $<TARGET_OBJECTS:star_extern> $<TARGET_OBJECTS:star_core>
  ${star_mod_uploader_SOURCES})
target_sources (mod_uploader PRIVATE FILE_SET CXX_MODULES FILES
  StarModUploader.cppm StarStringConversion.cppm StarSPlainTextEdit.cppm)
target_include_directories (mod_uploader PRIVATE
    ${STAR_EXTERN_INCLUDES}
    ${STAR_CORE_INCLUDES}
    ${STAR_BASE_INCLUDES}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
  )
target_link_libraries (mod_uploader PRIVATE star_base_modules star_steam_dispatch_modules
  star_build_config star_steam_dependencies
  $<LINK_ONLY:star_core_dependencies> $<LINK_ONLY:star_extern_dependencies>
  Qt5::Core Qt5::Gui Qt5::Widgets)
star_import_qt_plugins(mod_uploader)
star_module_moc(mod_uploader StarSPlainTextEdit.cppm moc_StarSPlainTextEdit.cpp)
set_target_properties (mod_uploader PROPERTIES AUTOMOC OFF CXX_SCAN_FOR_MODULES ON)

