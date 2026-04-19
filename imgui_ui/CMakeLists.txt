cmake_minimum_required(VERSION 3.10)
project(Flora)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Force static linking of VC++ runtime (/MT)
if(MSVC)
    set(CompilerFlags
        CMAKE_CXX_FLAGS
        CMAKE_CXX_FLAGS_DEBUG
        CMAKE_CXX_FLAGS_RELEASE
    )
    foreach(CompilerFlag ${CompilerFlags})
        string(REPLACE "/MD" "/MT" ${CompilerFlag} "${${CompilerFlag}}")
        set(${CompilerFlag} "${${CompilerFlag}}" CACHE STRING "msvc" FORCE)
    endforeach()
endif()

# ImGui source files
set(IMGUI_SOURCES
    third_party/imgui/imgui.cpp
    third_party/imgui/imgui_demo.cpp
    third_party/imgui/imgui_draw.cpp
    third_party/imgui/imgui_tables.cpp
    third_party/imgui/imgui_widgets.cpp
    third_party/imgui/imgui_impl_win32.cpp
    third_party/imgui/imgui_impl_dx11.cpp
)

# Main application
set(APP_SOURCES
    src/main.cpp
)

# Include directories
include_directories(third_party/imgui)

# Create executable
add_executable(Flora WIN32 ${IMGUI_SOURCES} ${APP_SOURCES})

# Link libraries
target_link_libraries(Flora PRIVATE
    d3d11
    dxgi
    user32
    gdi32
    shell32
    ole32
    kernel32
)

# Set output name
target_link_options(Flora PRIVATE /SUBSYSTEM:WINDOWS)

# Copy FloraAPI.dll to output directory after build
add_custom_command(TARGET Flora POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/../build/Release/FloraAPI.dll"
    $<TARGET_FILE_DIR:Flora>
)
