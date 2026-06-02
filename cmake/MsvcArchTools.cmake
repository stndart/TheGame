# Ninja + MSVC presets use VsDevCmd -arch=x86 so cl.exe is 32-bit, but CMake 4.x
# can still cache Hostx64/x64/lib.exe and /machine:x64 (breaks fmt static lib).

if(NOT MSVC OR NOT CMAKE_CXX_COMPILER)
    return()
endif()

get_filename_component(_msvc_bindir "${CMAKE_CXX_COMPILER}" DIRECTORY)
if(NOT _msvc_bindir MATCHES "(^|[/\\\\])x86([/\\\\]|$)")
    return()
endif()

set(_lib "${_msvc_bindir}/lib.exe")
set(_link "${_msvc_bindir}/link.exe")
if(EXISTS "${_lib}")
    set(CMAKE_AR "${_lib}" CACHE FILEPATH "MSVC librarian" FORCE)
endif()
if(EXISTS "${_link}")
    set(CMAKE_LINKER "${_link}" CACHE FILEPATH "MSVC linker" FORCE)
endif()

foreach(_var IN ITEMS CMAKE_STATIC_LINKER_FLAGS
                      CMAKE_STATIC_LINKER_FLAGS_DEBUG
                      CMAKE_STATIC_LINKER_FLAGS_RELEASE
                      CMAKE_STATIC_LINKER_FLAGS_RELWITHDEBINFO
                      CMAKE_STATIC_LINKER_FLAGS_MINSIZEREL)
    if(DEFINED ${_var})
        string(REGEX REPLACE "/machine:x64" "/machine:x86" _fixed "${${_var}}")
        if(NOT _fixed MATCHES "/machine:x86")
            set(_fixed "/machine:x86 ${_fixed}")
        endif()
        set(${_var} "${_fixed}" CACHE STRING "" FORCE)
    endif()
endforeach()

if(CMAKE_MT AND CMAKE_MT MATCHES "/x64/")
    string(REPLACE "/x64/" "/x86/" _mt "${CMAKE_MT}")
    if(EXISTS "${_mt}")
        set(CMAKE_MT "${_mt}" CACHE FILEPATH "" FORCE)
    endif()
endif()
