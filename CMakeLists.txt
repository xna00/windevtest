cmake_minimum_required(VERSION 3.10)
project(MyApp VERSION 1.0.0 LANGUAGES C)

# Set prefix path to find packages
set(CMAKE_PREFIX_PATH 
    "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/vcpkg/pkgs/curl_x64-windows-static"
    "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/vcpkg/pkgs/zlib_x64-windows-static"
    CACHE STRING "Search paths for finding packages")

find_package(CURL REQUIRED)
find_package(ZLIB REQUIRED)

add_executable(MyApp src/main.c)
add_executable(WebSocketTest src/websocket_test.c)

target_compile_definitions(MyApp PRIVATE CURL_STATICLIB)
target_compile_definitions(WebSocketTest PRIVATE CURL_STATICLIB)

target_link_libraries(MyApp PRIVATE CURL::libcurl ZLIB::ZLIB)
target_link_libraries(WebSocketTest PRIVATE CURL::libcurl ZLIB::ZLIB)

target_compile_options(MyApp PRIVATE /MT)
target_compile_options(WebSocketTest PRIVATE /MT)