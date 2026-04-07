cmake_minimum_required(VERSION 3.10)
project(MyApp VERSION 1.0.0 LANGUAGES C)

set(CURL_INCLUDE_DIR "C:/Users/xna/source/repos/test/vcpkg_installed/vcpkg/pkgs/curl_x64-windows-static/include")
set(CURL_LIBRARY "C:/Users/xna/source/repos/test/vcpkg_installed/vcpkg/pkgs/curl_x64-windows-static/lib/libcurl.lib")
set(ZLIB_LIBRARY "C:/Users/xna/source/repos/test/vcpkg_installed/vcpkg/pkgs/zlib_x64-windows-static/lib/zlib.lib")

include_directories(${CURL_INCLUDE_DIR})

add_executable(${PROJECT_NAME} src/main.c)

target_link_libraries(${PROJECT_NAME} PRIVATE ${CURL_LIBRARY} ${ZLIB_LIBRARY} ws2_32.lib crypt32.lib secur32.lib iphlpapi.lib)

target_compile_options(${PROJECT_NAME} PRIVATE /MT)