cmake_minimum_required(VERSION 3.10)

project(
    MiniWebServer
    VERSION 2.0.0
    LANGUAGES CXX
)

# ============================================================
# C++ 标准
# ============================================================

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


# ============================================================
# 可执行文件输出目录
# ============================================================

set(
    CMAKE_RUNTIME_OUTPUT_DIRECTORY
    ${PROJECT_SOURCE_DIR}/app
)

# 确保 app 目录存在
file(
    MAKE_DIRECTORY
    ${PROJECT_SOURCE_DIR}/app
)


# ============================================================
# 查找线程库
# ============================================================

find_package(
    Threads
    REQUIRED
)


# ============================================================
# 源文件
# ============================================================

set(
    CORE_SOURCES

    src/core/main.cpp
    src/core/server.cpp
    src/core/listener.cpp
    src/core/epoller.cpp
    src/core/thread_pool.cpp
    src/core/unique_fd.cpp
)

set(
    HTTP_SOURCES

    src/http/http_connection.cpp
    src/http/http_parser.cpp
    src/http/mapped_file.cpp
)


# ============================================================
# 生成可执行文件
# ============================================================

add_executable(
    miniwebserver_v2

    ${CORE_SOURCES}
    ${HTTP_SOURCES}
)


# ============================================================
# 头文件目录
# ============================================================

target_include_directories(
    miniwebserver_v2

    PRIVATE

    ${PROJECT_SOURCE_DIR}/include
)


# ============================================================
# 链接线程库
# ============================================================

target_link_libraries(
    miniwebserver_v2

    PRIVATE

    Threads::Threads
)


# ============================================================
# 编译警告
# ============================================================

target_compile_options(
    miniwebserver_v2

    PRIVATE

    -Wall
    -Wextra
    -Wpedantic
)