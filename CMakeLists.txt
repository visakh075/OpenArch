cmake_minimum_required(VERSION 3.16)
project(OpenArch LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ------------------------------------------------------------
# Cross-Platform Compiler & OS Configurations
# ------------------------------------------------------------
if(MSVC)
    add_compile_options(/W4 /utf-8 /permissive-)
    add_compile_definitions(NOMINMAX _USE_MATH_DEFINES _CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(-Wall -Wextra)
endif()

option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
if(ENABLE_ASAN)
    if(MSVC)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /fsanitize=address")
    else()
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -g")
        set(CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} -fsanitize=address")
    endif()
endif()

# ------------------------------------------------------------
# Qt (REQUIRED for GUI)
# ------------------------------------------------------------
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt5 REQUIRED COMPONENTS Widgets Svg)

# ------------------------------------------------------------
# Dependencies
# ------------------------------------------------------------
find_package(SQLite3 REQUIRED)

# GNU Readline (optional - CLI falls back to standard console I/O if not found)
find_package(PkgConfig QUIET)
set(READLINE_FOUND FALSE)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(READLINE QUIET readline)
endif()

if(NOT READLINE_FOUND)
    find_path(READLINE_INCLUDE_DIR readline/readline.h)
    find_library(READLINE_LIBRARY NAMES readline)
    if(READLINE_INCLUDE_DIR AND READLINE_LIBRARY)
        set(READLINE_FOUND TRUE)
        set(READLINE_INCLUDE_DIRS ${READLINE_INCLUDE_DIR})
        set(READLINE_LIBRARIES ${READLINE_LIBRARY})
    endif()
endif()

# ------------------------------------------------------------
# Core library (shared by CLI + GUI)
# ------------------------------------------------------------
add_library(openarch_core STATIC
    src/core/Checksum.cpp
    src/core/ArchitectureModel.cpp
    src/core/GraphJson.cpp
    src/db/DbManagerSQLite.cpp
    src/db/DbManagerJson.cpp
    src/db/DbConverter.cpp
)

target_include_directories(openarch_core PUBLIC
    src
    src/core
    src/db
    src/gui
    src/gui/EditorDialogs
    src/gui/GraphModules
    src/gui/theme
)

target_link_libraries(openarch_core PUBLIC
    SQLite::SQLite3
)

# ------------------------------------------------------------
# CLI target
# ------------------------------------------------------------
add_executable(openarch
    src/cli/cli.cpp
)

if(READLINE_FOUND)
    target_compile_definitions(openarch PRIVATE HAVE_READLINE)
    target_include_directories(openarch PRIVATE ${READLINE_INCLUDE_DIRS})
    target_link_libraries(openarch PRIVATE ${READLINE_LIBRARIES})
    if(READLINE_CFLAGS_OTHER)
        target_compile_options(openarch PRIVATE ${READLINE_CFLAGS_OTHER})
    endif()
    message(STATUS "GNU Readline found: CLI will have full tab-completion and history.")
else()
    message(STATUS "GNU Readline not found: CLI will use standard line input fallback.")
endif()

target_link_libraries(openarch PRIVATE
    openarch_core
)

if(NOT MSVC)
    target_compile_options(openarch PRIVATE -Wpedantic)
endif()

# ------------------------------------------------------------
# GUI common sources
# ------------------------------------------------------------
set(OPENARCH_GUI_COMMON_SOURCES
    resources.qrc
    src/gui/MainWindow.cpp
    src/gui/ShortcutManager.cpp
    src/gui/ShortcutConfigDialog.cpp
    src/gui/ConvertDialog.cpp
    src/gui/EditorDialogs/NodeEditorDialog.cpp
    src/gui/EditorDialogs/LayerEditorDialog.cpp
    src/gui/EditorDialogs/EdgeEditorDialog.cpp
    src/gui/EditorDialogs/JsonTreeEditor.cpp
    src/gui/WelcomeWidget.cpp
    src/gui/GraphModules/GraphNodeItem.cpp
    src/gui/GraphModules/GraphEdgeItem.cpp
    src/gui/GraphView.cpp    
    src/gui/theme/GraphThemeManager.cpp
    src/gui/theme/ThemeEditorDock.cpp
    src/gui/theme/ThemeEditorWindow.cpp
)

# ------------------------------------------------------------
# GUI target
# ------------------------------------------------------------
add_executable(openarch-gui WIN32
    src/gui/main.cpp
    ${OPENARCH_GUI_COMMON_SOURCES}
)

target_link_libraries(openarch-gui PRIVATE
    openarch_core
    Qt5::Widgets
    Qt5::Svg
)

# Deploy theme presets to build/runtime directory
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/src/gui/theme/themes DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
add_custom_command(TARGET openarch-gui POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gui/theme/themes
        $<TARGET_FILE_DIR:openarch-gui>/themes
    COMMENT "Deploying theme presets to runtime folder"
)

# ------------------------------------------------------------
# Theme Editor target (standalone application)
# ------------------------------------------------------------
add_executable(openarch-theme-editor WIN32
    src/gui/theme/ThemeEditorMain.cpp
    ${OPENARCH_GUI_COMMON_SOURCES}
)

target_link_libraries(openarch-theme-editor PRIVATE
    openarch_core
    Qt5::Widgets
    Qt5::Svg
)

add_custom_command(TARGET openarch-theme-editor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gui/theme/themes
        $<TARGET_FILE_DIR:openarch-theme-editor>/themes
    COMMENT "Deploying theme presets to theme editor folder"
)

# ------------------------------------------------------------
# Test targets
# ------------------------------------------------------------
enable_testing()

add_executable(test_shortcuts
    tests/test_shortcuts.cpp
    ${OPENARCH_GUI_COMMON_SOURCES}
)

target_link_libraries(test_shortcuts PRIVATE
    openarch_core
    Qt5::Widgets
    Qt5::Svg
)

add_executable(test_db_converter
    tests/test_db_converter.cpp
)

target_link_libraries(test_db_converter PRIVATE
    openarch_core
)

add_executable(test_interactive_html
    tests/test_interactive_html.cpp
    ${OPENARCH_GUI_COMMON_SOURCES}
)

target_link_libraries(test_interactive_html PRIVATE
    openarch_core
    Qt5::Widgets
    Qt5::Svg
)

add_test(NAME TestShortcuts COMMAND test_shortcuts)
add_test(NAME TestDbConverter COMMAND test_db_converter)
add_test(NAME TestInteractiveHtml COMMAND test_interactive_html)