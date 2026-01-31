cmake_minimum_required(VERSION 3.16)
project(OpenArch LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
find_package(SQLite3 REQUIRED)
add_executable(openarch_cli
 src/cli/cli.cpp
 src/core/ArchitectureModel.cpp
 src/db/DbManagerSQLite.cpp
)
target_include_directories(openarch_cli PRIVATE src)
target_link_libraries(openarch_cli PRIVATE SQLite::SQLite3)
