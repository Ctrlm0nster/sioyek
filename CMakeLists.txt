cmake_minimum_required(VERSION 3.20)
project(sioyek VERSION 2.0.0 LANGUAGES C CXX)

include(GNUInstallDirs)
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build configuration" FORCE)
endif()
include(cmake/SioyekSafety.cmake)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
  message(FATAL_ERROR "Use an out-of-source build: cmake -S . -B build/dev -G Ninja")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Qt Stuff
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(QT NAMES Qt6 REQUIRED COMPONENTS Core)
find_package(Qt6 6.7 REQUIRED COMPONENTS
    Core Gui Widgets Network OpenGL QuickWidgets Svg TextToSpeech OpenGLWidgets)

if(APPLE AND TARGET WrapOpenGL::WrapOpenGL)
  set_property(TARGET WrapOpenGL::WrapOpenGL PROPERTY INTERFACE_LINK_LIBRARIES "-framework OpenGL")
endif()

qt_standard_project_setup()

qt_add_executable(sioyek WIN32 MACOSX_BUNDLE
    resources.qrc
    pdf_viewer/OpenWithApplication.cpp pdf_viewer/OpenWithApplication.h
    pdf_viewer/RunGuard.cpp pdf_viewer/RunGuard.h
    pdf_viewer/book.cpp pdf_viewer/book.h
    pdf_viewer/checksum.cpp pdf_viewer/checksum.h
    pdf_viewer/config.cpp pdf_viewer/config.h
    pdf_viewer/coordinates.cpp pdf_viewer/coordinates.h
    pdf_viewer/database.cpp pdf_viewer/database.h
    pdf_viewer/document.cpp pdf_viewer/document.h
    pdf_viewer/document_view.cpp pdf_viewer/document_view.h
    pdf_viewer/fts_fuzzy_match.h
    pdf_viewer/input.cpp pdf_viewer/input.h
    pdf_viewer/main.cpp
    pdf_viewer/main_widget.cpp pdf_viewer/main_widget.h
    pdf_viewer/mysortfilterproxymodel.cpp pdf_viewer/mysortfilterproxymodel.h
    pdf_viewer/new_file_checker.cpp pdf_viewer/new_file_checker.h
    pdf_viewer/path.cpp pdf_viewer/path.h
    pdf_viewer/pdf_renderer.cpp pdf_viewer/pdf_renderer.h
    pdf_viewer/pdf_view_opengl_widget.cpp pdf_viewer/pdf_view_opengl_widget.h
    pdf_viewer/rapidfuzz_amalgamated.hpp
    pdf_viewer/touchui/TouchAudioButtons.cpp pdf_viewer/touchui/TouchAudioButtons.h
    pdf_viewer/touchui/TouchCheckbox.cpp pdf_viewer/touchui/TouchCheckbox.h
    pdf_viewer/touchui/TouchConfigMenu.cpp pdf_viewer/touchui/TouchConfigMenu.h
    pdf_viewer/touchui/TouchCopyOptions.cpp pdf_viewer/touchui/TouchCopyOptions.h
    pdf_viewer/touchui/TouchDeleteButton.cpp pdf_viewer/touchui/TouchDeleteButton.h
    pdf_viewer/touchui/TouchDrawControls.cpp pdf_viewer/touchui/TouchDrawControls.h
    pdf_viewer/touchui/TouchGenericButtons.cpp pdf_viewer/touchui/TouchGenericButtons.h
    pdf_viewer/touchui/TouchHighlightButtons.cpp pdf_viewer/touchui/TouchHighlightButtons.h
    pdf_viewer/touchui/TouchListView.cpp pdf_viewer/touchui/TouchListView.h
    pdf_viewer/touchui/TouchMacroEditor.cpp pdf_viewer/touchui/TouchMacroEditor.h
    pdf_viewer/touchui/TouchMainMenu.cpp pdf_viewer/touchui/TouchMainMenu.h
    pdf_viewer/touchui/TouchMarkSelector.cpp pdf_viewer/touchui/TouchMarkSelector.h
    pdf_viewer/touchui/TouchPageSelector.cpp pdf_viewer/touchui/TouchPageSelector.h
    pdf_viewer/touchui/TouchRangeSelectUI.cpp pdf_viewer/touchui/TouchRangeSelectUI.h
    pdf_viewer/touchui/TouchRectangleSelectUI.cpp pdf_viewer/touchui/TouchRectangleSelectUI.h
    pdf_viewer/touchui/TouchSearchButtons.cpp pdf_viewer/touchui/TouchSearchButtons.h
    pdf_viewer/touchui/TouchSettings.cpp pdf_viewer/touchui/TouchSettings.h
    pdf_viewer/touchui/TouchSlider.cpp pdf_viewer/touchui/TouchSlider.h
    pdf_viewer/touchui/TouchTextEdit.cpp pdf_viewer/touchui/TouchTextEdit.h
    pdf_viewer/ui.cpp pdf_viewer/ui.h
    pdf_viewer/utf8.h
    pdf_viewer/utf8/checked.h
    pdf_viewer/utf8/core.h
    pdf_viewer/utf8/unchecked.h
    pdf_viewer/utils.cpp pdf_viewer/utils.h
)

if(APPLE)
    file(GLOB_RECURSE SIOYEK_SHADER_FILES CONFIGURE_DEPENDS
      "${CMAKE_CURRENT_SOURCE_DIR}/pdf_viewer/shaders/*"
    )

    target_sources(sioyek PRIVATE
      pdf_viewer/keys.config
      pdf_viewer/prefs.config
      ${SIOYEK_SHADER_FILES}
    )

    set_source_files_properties(
      pdf_viewer/keys.config
      pdf_viewer/prefs.config
        PROPERTIES MACOSX_PACKAGE_LOCATION "Resources"
    )

    set_source_files_properties(
      ${SIOYEK_SHADER_FILES}
        PROPERTIES MACOSX_PACKAGE_LOCATION "Resources/shaders"
    )

  target_sources(sioyek PRIVATE
        pdf_viewer/macos_specific.mm
  )
endif()

# Keep deprecated APIs out of application code without hiding any warnings.
sioyek_enable_safety(sioyek)
# Use a numeric value: Qt checks this before defining the QT_VERSION macro.
string(REPLACE "." ";" SIOYEK_QT_VERSION_PARTS "${Qt6Core_VERSION}")
list(GET SIOYEK_QT_VERSION_PARTS 0 SIOYEK_QT_MAJOR)
list(GET SIOYEK_QT_VERSION_PARTS 1 SIOYEK_QT_MINOR)
list(GET SIOYEK_QT_VERSION_PARTS 2 SIOYEK_QT_PATCH)
math(EXPR SIOYEK_QT_API_VERSION
  "(${SIOYEK_QT_MAJOR} << 16) | (${SIOYEK_QT_MINOR} << 8) | ${SIOYEK_QT_PATCH}"
  OUTPUT_FORMAT HEXADECIMAL)
target_compile_definitions(sioyek PRIVATE QT_DISABLE_DEPRECATED_UP_TO=${SIOYEK_QT_API_VERSION})

target_include_directories(sioyek PRIVATE 
    pdf_viewer 
    mupdf/include
    ${SQLite3_INCLUDE_DIRS}
)

target_compile_definitions(sioyek PRIVATE
    QT_OPENGLEXTENSIONS_LIB
    QT_OPENGL_LIB
    QT_WIDGETS_LIB
)

target_link_libraries(sioyek PRIVATE
    Qt::Core
    Qt::Gui
    Qt::Network
    Qt::OpenGL
    Qt::QuickWidgets
    Qt::Svg
    Qt::TextToSpeech
    Qt::Widgets
)

if(NOT ANDROID)
  target_sources(sioyek PUBLIC
        pdf_viewer/synctex/synctex_parser.c pdf_viewer/synctex/synctex_parser.h
        pdf_viewer/synctex/synctex_parser_utils.c pdf_viewer/synctex/synctex_parser_utils.h
  )
else()
  target_compile_definitions(sioyek PRIVATE
        SIOYEK_ANDROID
  )
endif()

if(WIN32)
  target_compile_definitions(sioyek PRIVATE
        _CRT_SECURE_NO_WARNINGS
        _CRT_NONSTDC_NO_DEPRECATE
        NOMINMAX
  )

  if(MSVC)
    target_compile_options(sioyek PRIVATE /bigobj /MP)
  endif()
endif()

if(UNIX AND NOT APPLE)
  target_compile_definitions(sioyek PRIVATE
        LINUX_STANDARD_PATHS
  )
endif()

find_package(ZLIB REQUIRED)
find_package(HarfBuzz REQUIRED)
find_package(SQLite3 REQUIRED)
if(TARGET SQLite3::SQLite3)
  set(SIOYEK_SQLITE_TARGET SQLite3::SQLite3)
else()
  set(SIOYEK_SQLITE_TARGET SQLite::SQLite3)
endif()

# Distributions ship HarfBuzz as a CMake config package that only exposes an
# imported target; older Find modules only defined HARFBUZZ_LIBRARIES.
if(TARGET harfbuzz::harfbuzz)
  set(SIOYEK_HARFBUZZ_LIBS harfbuzz::harfbuzz)
elseif(TARGET HarfBuzz::HarfBuzz)
  set(SIOYEK_HARFBUZZ_LIBS HarfBuzz::HarfBuzz)
else()
  set(SIOYEK_HARFBUZZ_LIBS ${HARFBUZZ_LIBRARIES})
endif()

if((QT_VERSION_MAJOR GREATER 5))
  target_compile_definitions(sioyek PRIVATE
        SIOYEK_QT6
  )

  target_link_libraries(sioyek PRIVATE
        Qt::OpenGLWidgets
  )
endif()

add_library(fzf STATIC
    fzf/fzf.c
    fzf/fzf.h
)

sioyek_enable_safety(fzf)

target_link_libraries(sioyek PRIVATE
    ${CMAKE_DL_LIBS}
    mupdf
    ${SIOYEK_HARFBUZZ_LIBS}
    ZLIB::ZLIB
    fzf
    ${SIOYEK_SQLITE_TARGET}
)

if(APPLE)
  # target_link_libraries(sioyek PRIVATE
  #       "-framework AppKit"
  #       mupdf-third
  #       mupdf-threads
  # )
  target_link_libraries(sioyek PRIVATE
      "-framework AppKit"
      -ldl
      -L${CMAKE_CURRENT_SOURCE_DIR}/mupdf/build/release
      -lmupdf
      -lmupdf-third
      -lmupdf-threads
      -lz
  )
else()
  if(UNIX AND NOT ANDROID)
    find_program(SIOYEK_MAKE NAMES gmake make REQUIRED)
    find_package(Threads REQUIRED)
    set(SIOYEK_MUPDF_JOBS "2" CACHE STRING "Parallel jobs for vendored MuPDF")
    set(mupdf_out "${CMAKE_CURRENT_BINARY_DIR}/mupdf/${SIOYEK_MUPDF_FLAGS_HASH}")
    # Always ask Make to check its dependencies, including nested submodule edits.
    add_custom_target(sioyek_mupdf
      COMMAND ${SIOYEK_MAKE} -C "${CMAKE_CURRENT_SOURCE_DIR}/mupdf"
        -j${SIOYEK_MUPDF_JOBS} build=release OUT=${mupdf_out}
        CC=${CMAKE_C_COMPILER} CXX=${CMAKE_CXX_COMPILER}
        "XCFLAGS=${SIOYEK_MUPDF_SAFETY_FLAGS}"
        USE_SYSTEM_HARFBUZZ=yes USE_SYSTEM_ZLIB=yes
        libs ${mupdf_out}/libmupdf-threads.a
      BYPRODUCTS ${mupdf_out}/libmupdf.a ${mupdf_out}/libmupdf-third.a
        ${mupdf_out}/libmupdf-threads.a
      VERBATIM USES_TERMINAL)
    add_library(mupdf STATIC IMPORTED)
    set_target_properties(mupdf PROPERTIES IMPORTED_LOCATION "${mupdf_out}/libmupdf.a")
    add_dependencies(mupdf sioyek_mupdf)
    target_link_libraries(sioyek PRIVATE
      "${mupdf_out}/libmupdf-third.a" "${mupdf_out}/libmupdf-threads.a"
      ${SIOYEK_HARFBUZZ_LIBS} ZLIB::ZLIB Threads::Threads m)
    file(RELATIVE_PATH SIOYEK_RELATIVE_DATA_DIR
      "${CMAKE_INSTALL_FULL_BINDIR}" "${CMAKE_INSTALL_FULL_DATADIR}/sioyek")
    target_compile_definitions(sioyek PRIVATE
      SIOYEK_RELATIVE_DATA_DIR="${SIOYEK_RELATIVE_DATA_DIR}"
      SIOYEK_INSTALL_DATA_DIR="${CMAKE_INSTALL_FULL_DATADIR}/sioyek")
    # Keep resources up to date even when only a shader or config changes.
    add_custom_target(sioyek_resources
      COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/resources"
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/pdf_viewer/prefs.config"
        "${CMAKE_CURRENT_SOURCE_DIR}/pdf_viewer/keys.config"
        "${CMAKE_CURRENT_SOURCE_DIR}/tutorial.pdf"
        "${CMAKE_CURRENT_BINARY_DIR}/resources"
      COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/pdf_viewer/shaders"
        "${CMAKE_CURRENT_BINARY_DIR}/resources/shaders")
    add_dependencies(sioyek sioyek_resources)
  endif()
endif()

install(TARGETS sioyek
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(UNIX AND NOT APPLE AND NOT ANDROID)
  install(DIRECTORY pdf_viewer/shaders/
          DESTINATION ${CMAKE_INSTALL_DATADIR}/sioyek/shaders)
  install(FILES tutorial.pdf
          DESTINATION ${CMAKE_INSTALL_DATADIR}/sioyek)
  install(FILES pdf_viewer/keys.config pdf_viewer/prefs.config
          DESTINATION ${CMAKE_INSTALL_DATADIR}/sioyek)
  install(FILES resources/sioyek.desktop
          DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
  install(FILES resources/sioyek-icon-linux.png
          DESTINATION ${CMAKE_INSTALL_DATADIR}/pixmaps)
  install(FILES resources/sioyek.1
          DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
endif()

if(APPLE OR WIN32)
  qt_generate_deploy_app_script(
      TARGET sioyek
      OUTPUT_SCRIPT deploy_script
      NO_UNSUPPORTED_PLATFORM_ERROR
  )
  install(SCRIPT ${deploy_script})
endif()

option(SIOYEK_BUILD_TESTS "Build checksum and UTF-8 compatibility tests" OFF)
if(SIOYEK_BUILD_TESTS)
  enable_testing()
  add_executable(sioyek_modern_api_test tests/modern_api.cpp pdf_viewer/checksum.cpp)
  target_include_directories(sioyek_modern_api_test PRIVATE pdf_viewer)
  target_link_libraries(sioyek_modern_api_test PRIVATE Qt::Core)
  sioyek_enable_safety(sioyek_modern_api_test)
  target_compile_definitions(sioyek_modern_api_test PRIVATE
    QT_DISABLE_DEPRECATED_UP_TO=${SIOYEK_QT_API_VERSION})
  add_test(NAME modern_api_compatibility
    COMMAND sioyek_modern_api_test "${CMAKE_CURRENT_SOURCE_DIR}/tutorial.pdf")
endif()
