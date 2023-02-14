cmake_minimum_required(VERSION 3.0)
project(ClassroomPerf LANGUAGES CXX VERSION 1.0.0)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)
set(HTTPLIB_COMPILE ON)
add_subdirectory(lib/cpp-httplib)
add_subdirectory(lib/json)

add_executable(server "server.cpp")
target_precompile_headers(server PUBLIC "pch.hpp")
target_link_libraries(server PUBLIC httplib::httplib nlohmann_json)
