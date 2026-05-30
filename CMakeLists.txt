cmake_minimum_required(VERSION 3.15)
project(TheGame LANGUAGES C CXX)

# log_message() uses OutputDebugStringA; diagnostics VEH forwards those to the pipe (duplicate).
option(THEGAME_NO_OUTPUT_DEBUG_STRING "Omit OutputDebugStringA from log_message" ON)
# AllocConsole/printf; controller pipe + log files cover the same use case.
option(THEGAME_NO_CONSOLE "Do not allocate a debug console window" OFF)
option(THEGAME_PN_TCP_PIPE "Emit proudnet-tcp header events on diagnostics pipe" OFF)
# Wire-only offline tests: no in-process S2C RES injection (server must answer on TCP).
option(THEGAME_DISABLE_RMI_INJECT "Compile out RMI/Inject offline pumps/leaves" OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Set MSVC runtime library configuration
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    add_compile_options(/GS- /EHs)
endif()

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

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
    if(THEGAME_NO_OUTPUT_DEBUG_STRING)
        target_compile_definitions(TheGame PRIVATE THEGAME_NO_OUTPUT_DEBUG_STRING)
    endif()
    if(THEGAME_NO_CONSOLE)
        target_compile_definitions(TheGame PRIVATE THEGAME_NO_CONSOLE)
    endif()
    if(THEGAME_PN_TCP_PIPE)
        target_compile_definitions(TheGame PRIVATE THEGAME_PN_TCP_PIPE)
    endif()
    if(THEGAME_DISABLE_RMI_INJECT)
        target_compile_definitions(TheGame PRIVATE THEGAME_DISABLE_RMI_INJECT)
    endif()

    if (MSVC)
        # Add ATL/MFC include path
        set(MSVC_ATL_INCLUDE_DIR "$ENV{VCToolsInstallDir}/atlmfc/include")
        target_include_directories(TheGame PRIVATE "${MSVC_ATL_INCLUDE_DIR}")
    endif()

    target_link_libraries(TheGame PRIVATE kernel32 user32 Ws2_32)
endif()

# Set output directory
set_target_properties(TheGame PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)