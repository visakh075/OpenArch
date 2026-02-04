cmake_minimum_required(VERSION 3.16)
project(OpenArch LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ------------------------------------------------------------
# Dependencies
# ------------------------------------------------------------
find_package(SQLite3 REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(READLINE REQUIRED readline)

# ------------------------------------------------------------
# Source files
# ------------------------------------------------------------
set(SOURCES
    src/core/Checksum.cpp
    src/core/ArchitectureModel.cpp
    src/core/GraphJson.cpp
    src/db/DbManagerSQLite.cpp

    src/cli/cli.cpp
)

# ------------------------------------------------------------
# Target
# ------------------------------------------------------------
add_executable(openarch ${SOURCES})

target_include_directories(openarch PRIVATE
    src
    ${READLINE_INCLUDE_DIRS}
)

target_link_libraries(openarch PRIVATE
    SQLite::SQLite3
    ${READLINE_LIBRARIES}
)

target_compile_options(openarch PRIVATE
    ${READLINE_CFLAGS_OTHER}
    -Wall
    -Wextra
    -Wpedantic
)

find_package(Qt5 REQUIRED COMPONENTS Widgets)

add_executable(openarch-gui
    src/gui/main.cpp
    src/gui/MainWindow.cpp

    src/core/Checksum.cpp
    src/core/ArchitectureModel.cpp
    src/core/GraphJson.cpp

    src/db/DbManagerSQLite.cpp
)

target_include_directories(openarch-gui PRIVATE src)

target_link_libraries(openarch-gui PRIVATE
    Qt5::Widgets
    SQLite::SQLite3
)