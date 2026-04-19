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

# WebView2 SDK - use local SDK
# Include WebView2 headers
include_directories(
    third_party/webview2_sdk/build/native/include
    third_party
    third_party/nlohmann/include
)

# Main application
set(APP_SOURCES
    src/main.cpp
)

# Create executable
add_executable(Flora WIN32 ${APP_SOURCES})

# Link libraries
target_link_libraries(Flora PRIVATE
    user32
    gdi32
    shell32
    ole32
    oleaut32
    kernel32
    advapi32
    version
    shlwapi
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/webview2_sdk/build/native/x64/WebView2LoaderStatic.lib"
)

# Set output name
target_link_options(Flora PRIVATE /SUBSYSTEM:WINDOWS)

# Copy FloraAPI.dll to output directory after build
add_custom_command(TARGET Flora POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/../build/Release/FloraAPI.dll"
    $<TARGET_FILE_DIR:Flora>
)

# Copy web UI files to output directory
add_custom_command(TARGET Flora POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/web_ui"
    $<TARGET_FILE_DIR:Flora>/web_ui
)
