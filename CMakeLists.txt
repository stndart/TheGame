cmake_minimum_required(VERSION 3.15)
project(TheGame LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Set MSVC runtime library configuration
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    add_compile_options(/GS- /EHa)
endif()

file(GLOB_RECURSE SOURCES src/*.cpp)

# Create DLL target
add_library(TheGame SHARED ${SOURCES})

# Include directories
target_include_directories(TheGame PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/stubs
)

# Windows-specific settings
if(WIN32)
    target_compile_definitions(TheGame PRIVATE 
        _CRT_SECURE_NO_WARNINGS
        WIN32_LEAN_AND_MEAN
    )

    if (MSVC)
        add_compile_options(/GS- /EHa)

        # Add ATL/MFC include path
        set(MSVC_ATL_INCLUDE_DIR "$ENV{VCToolsInstallDir}/atlmfc/include")
        target_include_directories(TheGame PRIVATE "${MSVC_ATL_INCLUDE_DIR}")
    endif()

    target_link_libraries(TheGame PRIVATE kernel32 user32)
endif()

# Set output directory
set_target_properties(TheGame PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)