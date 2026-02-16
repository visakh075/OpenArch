cmake_minimum_required(VERSION 3.16)
project(OpenArch LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ------------------------------------------------------------
# Qt (REQUIRED for GUI)
# ------------------------------------------------------------
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt5 REQUIRED COMPONENTS Widgets)

# ------------------------------------------------------------
# Dependencies
# ------------------------------------------------------------
find_package(SQLite3 REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(READLINE REQUIRED readline)

# ------------------------------------------------------------
# Core library (shared by CLI + GUI)
# ------------------------------------------------------------
add_library(openarch_core
    src/core/Checksum.cpp
    src/core/ArchitectureModel.cpp
    src/core/GraphJson.cpp
    src/db/DbManagerSQLite.cpp
)

target_include_directories(openarch_core PUBLIC
    src
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

target_include_directories(openarch PRIVATE
    ${READLINE_INCLUDE_DIRS}
)

target_link_libraries(openarch PRIVATE
    openarch_core
    ${READLINE_LIBRARIES}
)

target_compile_options(openarch PRIVATE
    ${READLINE_CFLAGS_OTHER}
    -Wall
    -Wextra
    -Wpedantic
)

# ------------------------------------------------------------
# GUI target
# ------------------------------------------------------------
add_executable(openarch-gui
    src/gui/main.cpp
    src/gui/MainWindow.cpp

    src/gui/NodeEditorDialog.cpp
    src/gui/LayerEditorDialog.cpp
    src/gui/EdgeEditorDialog.cpp

    src/gui/GraphNodeItem.cpp
    src/gui/GraphEdgeItem.cpp
    

    src/gui/GraphView.cpp    
)


target_link_libraries(openarch-gui PRIVATE
    openarch_core
    Qt5::Widgets
)
