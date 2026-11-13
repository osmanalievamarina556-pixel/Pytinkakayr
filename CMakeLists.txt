cmake_minimum_required(VERSION 3.18)
project(crez LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/third_party/imgui)

# ImGui: скрытая видимость, символы не экспортируются из libcrez.so
add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)
target_compile_definitions(imgui PUBLIC IMGUI_IMPL_OPENGL_ES3)
target_compile_options(imgui PRIVATE -fvisibility=hidden -fvisibility-inlines-hidden)

add_library(crez SHARED src/crez_esp.cpp)
target_compile_options(crez PRIVATE -fvisibility=hidden -fvisibility-inlines-hidden)
target_link_libraries(crez
    PRIVATE imgui
    log EGL GLESv3 android dl
)
# strip: убирает символы из итоговой .so
target_link_options(crez PRIVATE -s)
