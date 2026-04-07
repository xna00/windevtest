# curl-test

使用 libcurl 的 HTTP 客户端示例程序

## 功能

- 从命令行参数读取 URL
- 支持 HTTP/HTTPS
- 静态链接，无需额外 DLL

## 构建

```bash
cmake -B build -G Ninja
cmake --build build
```

## 运行

```bash
build\MyApp.exe <URL>

# 示例
build\MyApp.exe https://example.com
build\MyApp.exe http://httpbin.org/get
```

## 依赖

- curl (静态链接，x64-windows-static)
- zlib (静态链接)
- Windows SDK

## 项目结构

```
test/
├── src/main.c          # 源代码
├── CMakeLists.txt     # 构建配置
├── README.md          # 说明文档
├── build/             # 构建输出
└── vcpkg_installed/  # 静态库
```

## 注意事项

- 使用 `--triplet x64-windows-static` 安装 curl 静态版本
- vcpkg manifest 模式下可通过 `vcpkg install --triplet x64-windows-static` 指定 triplet
- 静态链接需要额外链接 ws2_32.lib、crypt32.lib、secur32.lib、iphlpapi.lib
