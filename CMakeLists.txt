cmake_minimum_required(VERSION 3.12.4)
cmake_policy(SET CMP0177 NEW)
cmake_policy(SET CMP0174 NEW)

project(lv_sim LANGUAGES C)

# SDL入口修复，必须在find_package之前
add_definitions(-DSDL_MAIN_HANDLED)

# 你的SDL路径
list(APPEND CMAKE_PREFIX_PATH "E:/SDL2/SDL2-2.30.12/x86_64-w64-mingw32")
set(SDL2_NO_MWINDOWS 1)
find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIRS})

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# 只编译核心lvgl和驱动，注释demos
add_subdirectory(lvgl)
add_subdirectory(lv_drivers)
# add_subdirectory(lvgl/demos) 删掉/注释，不再编译demo库

add_executable(main
    main/src/main.c
    main/src/mouse_cursor_icon.c
    main/src/bsp_Screen.c
)

target_include_directories(main PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/lvgl
    ${CMAKE_SOURCE_DIR}/lv_drivers
    ${CMAKE_SOURCE_DIR}/main/src
)

# 链接列表移除lvgl_demos
target_link_libraries(main PRIVATE
    lvgl
    lv_drivers
    SDL2::SDL2
    pthread
)