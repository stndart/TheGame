# Static CRT and static third-party libs are mandatory for the hook DLL.

if(NOT WIN32)
    message(FATAL_ERROR "TheGame targets Win32 only.")
endif()

set(_thegame_static_crt "MultiThreaded$<$<CONFIG:Debug>:Debug>")

if(MSVC)
    if(DEFINED CMAKE_MSVC_RUNTIME_LIBRARY
       AND NOT CMAKE_MSVC_RUNTIME_LIBRARY STREQUAL "${_thegame_static_crt}")
        message(FATAL_ERROR
            "CMAKE_MSVC_RUNTIME_LIBRARY must be \"${_thegame_static_crt}\" "
            "(static /MT, /MTd). Got \"${CMAKE_MSVC_RUNTIME_LIBRARY}\".")
    endif()
    set(CMAKE_MSVC_RUNTIME_LIBRARY "${_thegame_static_crt}" CACHE STRING
        "Static CRT (/MT, /MTd) required for TheGame" FORCE)

    foreach(_var IN ITEMS CMAKE_C_FLAGS CMAKE_CXX_FLAGS
            CMAKE_C_FLAGS_DEBUG CMAKE_CXX_FLAGS_DEBUG
            CMAKE_C_FLAGS_RELEASE CMAKE_CXX_FLAGS_RELEASE
            CMAKE_C_FLAGS_RELWITHDEBINFO CMAKE_CXX_FLAGS_RELWITHDEBINFO)
        if(DEFINED ${_var} AND "${${_var}}" MATCHES "(^| )/MD(d)?($| )")
            message(FATAL_ERROR
                "${_var} uses dynamic CRT (/MD). Static /MT is mandatory.")
        endif()
    endforeach()
endif()

set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build dependencies as static libraries" FORCE)
