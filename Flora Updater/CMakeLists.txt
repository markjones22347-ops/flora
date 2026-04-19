cmake_minimum_required(VERSION 3.15)
project(FloraUpdater CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ImGui
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui-master)
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
)
set(IMGUI_HEADERS
    ${IMGUI_DIR}/imgui.h
    ${IMGUI_DIR}/imgui_internal.h
    ${IMGUI_DIR}/imconfig.h
    ${IMGUI_DIR}/imgui_draw.h
    ${IMGUI_DIR}/imgui_widgets.h
    ${IMGUI_DIR}/imgui_tables.h
    ${IMGUI_DIR}/imstb_rectpack.h
    ${IMGUI_DIR}/imstb_textedit.h
    ${IMGUI_DIR}/imstb_truetype.h
)

# ImGui backends (Win32 + DirectX 11)
set(IMGUI_BACKEND_DIR ${IMGUI_DIR}/backends)
set(IMGUI_BACKEND_SOURCES
    ${IMGUI_BACKEND_DIR}/imgui_impl_win32.cpp
    ${IMGUI_BACKEND_DIR}/imgui_impl_dx11.cpp
)
set(IMGUI_BACKEND_HEADERS
    ${IMGUI_BACKEND_DIR}/imgui_impl_win32.h
    ${IMGUI_BACKEND_DIR}/imgui_impl_dx11.h
)

# Source files
set(SOURCES
    src/main.cpp
    src/updater.cpp
    src/http.cpp
    src/zip.cpp
    src/version.cpp
    src/json.cpp
    ${IMGUI_SOURCES}
    ${IMGUI_BACKEND_SOURCES}
)

# Create executable
add_executable(FloraUpdater ${SOURCES})

# Include directories
target_include_directories(FloraUpdater PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${IMGUI_DIR}
    ${IMGUI_BACKEND_DIR}
)

# Link Windows libraries
target_link_libraries(FloraUpdater 
    winhttp
    shell32
    ole32
    d3d11
    d3dcompiler
)

# Compiler flags
if(MSVC)
    target_compile_options(FloraUpdater PRIVATE /W4)
else()
    target_compile_options(FloraUpdater PRIVATE -Wall -Wextra)
endif()
