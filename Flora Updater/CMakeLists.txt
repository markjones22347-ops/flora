cmake_minimum_required(VERSION 3.15)
project(FloraUpdater CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Source files
set(SOURCES
    src/main.cpp
    src/updater.cpp
    src/http.cpp
    src/zip.cpp
    src/version.cpp
    src/json.cpp
)

# Create executable
add_executable(FloraUpdater ${SOURCES})

# Include directories
target_include_directories(FloraUpdater PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Link Windows libraries
target_link_libraries(FloraUpdater 
    winhttp
    shell32
    ole32
)

# Compiler flags
if(MSVC)
    target_compile_options(FloraUpdater PRIVATE /W4)
else()
    target_compile_options(FloraUpdater PRIVATE -Wall -Wextra)
endif()
