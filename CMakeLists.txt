cmake_minimum_required(VERSION 3.18)
project(crez LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# third_party/imgui и third_party/dobby клонируются в workflow
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/third_party/imgui)
set(DOBBY_DIR ${CMAKE_SOURCE_DIR}/third_party/dobby)

add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)
# GLES3 вместо desktop GL: бэкенд использует ES-загрузчик
target_compile_definitions(imgui PUBLIC IMGUI_IMPL_OPENGL_ES3)

# Dobby: тесты, примеры и SymbolResolver выключены.
# SymbolResolver не собирается с текущей версией кода; нам он не нужен, символы ищем через dlsym.
set(DOBBY_GENERATE_SHARED OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(Plugin.SymbolResolver OFF CACHE BOOL "" FORCE)
add_subdirectory(${DOBBY_DIR} dobby_build)

add_library(crez SHARED src/crez_esp.cpp)
target_include_directories(crez PRIVATE ${DOBBY_DIR}/include)
target_link_libraries(crez
    PRIVATE imgui dobby
    log EGL GLESv3 android dl
)
