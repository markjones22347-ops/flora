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

# Include directories
include_directories(
    third_party
    third_party/nlohmann/include
    webview2_sdk/build/native/include
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
    uuid
    dwmapi
)

# Link WebView2 loader
target_link_libraries(Flora PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/webview2_sdk/build/native/x64/WebView2Loader.dll.lib")

# Set output name
target_link_options(Flora PRIVATE /SUBSYSTEM:WINDOWS)

# Copy FloraAPI.dll to output directory after build
add_custom_command(TARGET Flora POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/../build/Release/FloraAPI.dll"
    $<TARGET_FILE_DIR:Flora>
)

# Copy WebView2Loader.dll to output directory after build
add_custom_command(TARGET Flora POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/webview2_sdk/build/native/x64/WebView2Loader.dll"
    $<TARGET_FILE_DIR:Flora>
)
