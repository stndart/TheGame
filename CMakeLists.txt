cmake_minimum_required(VERSION 3.15)
project(TheGame LANGUAGES C CXX)

include(cmake/StaticLinking.cmake)
include(cmake/TheGameOptions.cmake)
include(cmake/FetchFmt.cmake)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(MSVC)
    add_compile_options(/GS- /EHs)
endif()

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

add_library(TheGame SHARED ${SOURCES})

target_include_directories(TheGame PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/stubs
)

if(WIN32)
    target_compile_definitions(TheGame PRIVATE
        _CRT_SECURE_NO_WARNINGS
        WIN32_LEAN_AND_MEAN
    )
    thegame_apply_options(TheGame)

    if(MSVC)
        set(MSVC_ATL_INCLUDE_DIR "$ENV{VCToolsInstallDir}/atlmfc/include")
        target_include_directories(TheGame PRIVATE "${MSVC_ATL_INCLUDE_DIR}")
    endif()

    target_link_libraries(TheGame PRIVATE fmt::fmt kernel32 user32 Ws2_32)
endif()

set_target_properties(TheGame PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)
