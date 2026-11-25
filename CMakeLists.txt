cmake_minimum_required(VERSION 3.20)
project(sioyek VERSION 3.0.0 LANGUAGES C CXX)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" OR ANDROID)
  message(FATAL_ERROR "This fork supports desktop Linux only (Arch Linux, Qt 6, Vulkan).")
endif()

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
    Core Gui GuiPrivate Widgets Network QuickWidgets Svg TextToSpeech ShaderTools)

qt_standard_project_setup()

qt_add_executable(sioyek
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
    pdf_viewer/pdf_view_widget.cpp pdf_viewer/pdf_view_widget.h
    pdf_viewer/vulkan_canvas.cpp pdf_viewer/vulkan_canvas.h
    pdf_viewer/render_types.cpp pdf_viewer/render_types.h
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

# Compile GLSL into embedded SPIR-V shader packages; no runtime shader compiler
# or files from a previous Sioyek installation are needed.
qt_add_shaders(sioyek sioyek_vulkan_shaders
    PREFIX "/sioyek/vulkan"
    BASE "pdf_viewer/shaders/vulkan"
    NOGLSL NOHLSL NOMSL
    FILES pdf_viewer/shaders/vulkan/canvas.vert pdf_viewer/shaders/vulkan/canvas.frag)


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

target_link_libraries(sioyek PRIVATE
    Qt::Core
    Qt::Gui
    Qt::Network
    Qt::GuiPrivate
    Qt::QuickWidgets
    Qt::Svg
    Qt::TextToSpeech
    Qt::Widgets
)

target_sources(sioyek PUBLIC
      pdf_viewer/synctex/synctex_parser.c pdf_viewer/synctex/synctex_parser.h
      pdf_viewer/synctex/synctex_parser_utils.c pdf_viewer/synctex/synctex_parser_utils.h
)


target_compile_definitions(sioyek PRIVATE
      LINUX_STANDARD_PATHS
)

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

find_program(SIOYEK_MAKE NAMES gmake make REQUIRED)
find_program(SIOYEK_PATCH NAMES patch REQUIRED)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
find_package(Threads REQUIRED)
find_package(JPEG REQUIRED)
# Switching JPEG implementations changes Make's source/header selection.
# Use fresh archives so ar cannot retain objects from the bundled decoder.
string(SHA256 SIOYEK_MUPDF_FLAGS_HASH "${SIOYEK_MUPDF_FLAGS_HASH};system-libjpeg;${JPEG_VERSION}")
string(SUBSTRING "${SIOYEK_MUPDF_FLAGS_HASH}" 0 12 SIOYEK_MUPDF_FLAGS_HASH)
set(SIOYEK_MUPDF_JOBS "2" CACHE STRING "Parallel jobs for vendored MuPDF")
set(mupdf_patches
  "${CMAKE_CURRENT_SOURCE_DIR}/patches/mupdf/1.26.11-null-reference-counting.patch"
  "${CMAKE_CURRENT_SOURCE_DIR}/patches/mupdf/1.26.11-text-color-shift.patch"
  "${CMAKE_CURRENT_SOURCE_DIR}/patches/mupdf/1.26.11-empty-layer-sort.patch"
  "${CMAKE_CURRENT_SOURCE_DIR}/patches/mupdf/1.26.11-svg-const-qualifier.patch"
  "${CMAKE_CURRENT_SOURCE_DIR}/patches/mupdf/lcms2-typed-interpolation.patch")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${mupdf_patches})
set(mupdf_source "${CMAKE_CURRENT_BINARY_DIR}/mupdf-source")
set(mupdf_out "${CMAKE_CURRENT_BINARY_DIR}/mupdf/${SIOYEK_MUPDF_FLAGS_HASH}")
# Always ask Make to check its dependencies, including nested submodule edits.
add_custom_target(sioyek_mupdf
  COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/scripts/prepare-mupdf.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/mupdf" "${mupdf_source}" ${mupdf_patches}
  COMMAND ${SIOYEK_MAKE} -C "${mupdf_source}"
    -j${SIOYEK_MUPDF_JOBS} build=release OUT=${mupdf_out}
    CC=${CMAKE_C_COMPILER} CXX=${CMAKE_CXX_COMPILER}
    "XCFLAGS=${SIOYEK_MUPDF_SAFETY_FLAGS}"
    USE_SYSTEM_HARFBUZZ=yes USE_SYSTEM_ZLIB=yes USE_SYSTEM_LIBJPEG=yes
    libs ${mupdf_out}/libmupdf-threads.a
  BYPRODUCTS ${mupdf_out}/libmupdf.a ${mupdf_out}/libmupdf-third.a
    ${mupdf_out}/libmupdf-threads.a
  VERBATIM USES_TERMINAL)
add_library(mupdf STATIC IMPORTED)
set_target_properties(mupdf PROPERTIES IMPORTED_LOCATION "${mupdf_out}/libmupdf.a")
# Keep public headers consistent with the configuration passed to MuPDF's Make.
target_compile_definitions(mupdf INTERFACE "${SIOYEK_MUPDF_ALIGNMENT_DEFINITION}")
add_dependencies(mupdf sioyek_mupdf)
target_link_libraries(sioyek PRIVATE
  "${mupdf_out}/libmupdf-third.a" "${mupdf_out}/libmupdf-threads.a"
  ${SIOYEK_HARFBUZZ_LIBS} ZLIB::ZLIB JPEG::JPEG Threads::Threads m)
file(RELATIVE_PATH SIOYEK_RELATIVE_DATA_DIR
  "${CMAKE_INSTALL_FULL_BINDIR}" "${CMAKE_INSTALL_FULL_DATADIR}/sioyek")
target_compile_definitions(sioyek PRIVATE
  SIOYEK_RELATIVE_DATA_DIR="${SIOYEK_RELATIVE_DATA_DIR}"
  SIOYEK_INSTALL_DATA_DIR="${CMAKE_INSTALL_FULL_DATADIR}/sioyek")
# Shaders are embedded SPIR-V. Only data files are staged for local runs.
add_custom_target(sioyek_resources
  COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/resources"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/pdf_viewer/prefs.config"
    "${CMAKE_CURRENT_SOURCE_DIR}/pdf_viewer/keys.config"
    "${CMAKE_CURRENT_SOURCE_DIR}/tutorial.pdf"
    "${CMAKE_CURRENT_BINARY_DIR}/resources")
add_dependencies(sioyek sioyek_resources)

install(TARGETS sioyek
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

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


option(SIOYEK_BUILD_TESTS "Build checksum, UTF-8 and MuPDF runtime tests" OFF)
if(SIOYEK_BUILD_TESTS)
  enable_testing()
  add_executable(sioyek_renderer_response_test tests/renderer_response.cpp)
  target_include_directories(sioyek_renderer_response_test PRIVATE pdf_viewer mupdf/include)
  target_link_libraries(sioyek_renderer_response_test PRIVATE Qt::Gui)
  sioyek_enable_safety(sioyek_renderer_response_test)
  target_compile_definitions(sioyek_renderer_response_test PRIVATE
    QT_DISABLE_DEPRECATED_UP_TO=${SIOYEK_QT_API_VERSION})
  add_test(NAME renderer_response_state COMMAND sioyek_renderer_response_test)
  add_executable(sioyek_modern_api_test tests/modern_api.cpp pdf_viewer/checksum.cpp)
  target_include_directories(sioyek_modern_api_test PRIVATE pdf_viewer)
  target_link_libraries(sioyek_modern_api_test PRIVATE Qt::Core)
  sioyek_enable_safety(sioyek_modern_api_test)
  target_compile_definitions(sioyek_modern_api_test PRIVATE
    QT_DISABLE_DEPRECATED_UP_TO=${SIOYEK_QT_API_VERSION})
  add_test(NAME modern_api_compatibility
    COMMAND sioyek_modern_api_test "${CMAKE_CURRENT_SOURCE_DIR}/tutorial.pdf")
  if(TARGET sioyek_mupdf)
    add_test(NAME mupdf_source_staging
      COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tests/prepare_mupdf.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/scripts/prepare-mupdf.py")
    add_executable(sioyek_mupdf_test tests/mupdf_runtime.c)
    target_include_directories(sioyek_mupdf_test PRIVATE mupdf/include)
    target_link_libraries(sioyek_mupdf_test PRIVATE mupdf
      "${mupdf_out}/libmupdf-third.a" "${mupdf_out}/libmupdf-threads.a"
      ${SIOYEK_HARFBUZZ_LIBS} ZLIB::ZLIB JPEG::JPEG Threads::Threads m)
    sioyek_enable_safety(sioyek_mupdf_test)
    add_test(NAME mupdf_runtime
      COMMAND sioyek_mupdf_test "${CMAKE_CURRENT_SOURCE_DIR}/tutorial.pdf")
  endif()
endif()

option(SIOYEK_BUILD_GPU_TESTS "Build Vulkan rendering tests (requires a display and Vulkan ICD)" OFF)
if(SIOYEK_BUILD_GPU_TESTS)
  enable_testing()
  qt_add_executable(sioyek_vulkan_test tests/vulkan_canvas.cpp
    pdf_viewer/vulkan_canvas.cpp pdf_viewer/render_types.cpp)
  target_include_directories(sioyek_vulkan_test PRIVATE pdf_viewer)
  target_link_libraries(sioyek_vulkan_test PRIVATE Qt::Widgets Qt::GuiPrivate)
  sioyek_enable_safety(sioyek_vulkan_test)
  target_compile_definitions(sioyek_vulkan_test PRIVATE
    QT_DISABLE_DEPRECATED_UP_TO=${SIOYEK_QT_API_VERSION})
  qt_add_shaders(sioyek_vulkan_test sioyek_vulkan_test_shaders
    PREFIX "/sioyek/vulkan" BASE "pdf_viewer/shaders/vulkan"
    NOGLSL NOHLSL NOMSL
    FILES pdf_viewer/shaders/vulkan/canvas.vert pdf_viewer/shaders/vulkan/canvas.frag)
  add_test(NAME vulkan_rendering COMMAND sioyek_vulkan_test)
  qt_add_executable(sioyek_qt_vulkan_lifecycle_test tests/qt_vulkan_lifecycle.cpp)
  target_link_libraries(sioyek_qt_vulkan_lifecycle_test PRIVATE Qt::Widgets Qt::GuiPrivate)
  sioyek_enable_safety(sioyek_qt_vulkan_lifecycle_test)
  target_compile_definitions(sioyek_qt_vulkan_lifecycle_test PRIVATE
    QT_DISABLE_DEPRECATED_UP_TO=${SIOYEK_QT_API_VERSION})
  add_test(NAME qt_vulkan_lifecycle COMMAND sioyek_qt_vulkan_lifecycle_test)
  set_tests_properties(vulkan_rendering qt_vulkan_lifecycle PROPERTIES LABELS "gpu" TIMEOUT 60)
endif()
