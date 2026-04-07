cmake_minimum_required(VERSION 3.10)
project(PrintDriver VERSION 1.0.0 LANGUAGES C)

set(CMAKE_PREFIX_PATH 
    "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/vcpkg/pkgs/curl_x64-windows-static"
    "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/vcpkg/pkgs/zlib_x64-windows-static"
    CACHE STRING "Search paths for finding packages")

set(json-c_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/vcpkg/pkgs/json-c_x64-windows-static/share/json-c")

find_package(CURL REQUIRED)
find_package(ZLIB REQUIRED)
find_package(json-c REQUIRED)

set(SOURCES
    src/main.c
    src/http_client.c
    src/websocket.c
    src/login_dlg.c
    src/print_job.c
)

set(HEADERS
    src/config.h
    src/http_client.h
    src/websocket.h
    src/login_dlg.h
    src/print_job.h
    src/resource.h
)

add_executable(PrintDriver WIN32 ${SOURCES} ${HEADERS} src/resource.rc)

target_compile_definitions(PrintDriver PRIVATE CURL_STATICLIB)

target_compile_options(PrintDriver PRIVATE /MT)

target_link_libraries(PrintDriver PRIVATE CURL::libcurl ZLIB::ZLIB json-c::json-c)