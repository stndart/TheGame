# README-aligned compile flags. Values are 0/1 macros consumed by include/thegame/config.hpp.

option(THEGAME_NO_CONSOLE "Disable debug console window" OFF)
option(THEGAME_PIPES "Controller diagnostics/handler pipes" ON)
option(DISABLE_HOOKS "Do not install any in-process hooks" OFF)
option(DISABLE_SYSHOOKS "Do not hook WS2 system calls" OFF)
option(DISABLE_ENTRYPOINT_HOOK "Do not hook GAME.exe entrypoint" OFF)
option(DISABLE_VEH "Do not install vectored exception handler" OFF)
option(DISABLE_PARK_THREAD "Do not park thread on fatal AV (experimental)" ON)
option(DISABLE_INT "Do not raise int3 on access violation (experimental)" ON)
option(NO_NETWORK_LOGS "Disable netlogs.txt" OFF)
option(SILENT_KEEPALIVE "Omit keepalive-sized packets from netlogs" ON)
option(SILENT_NETWORK "Omit network events from main log (netlogs still active)" OFF)
option(NO_PROUD_LOGS "Disable proudlogs.txt" OFF)
option(SILENT_PROUD "Omit ProudNet events from main log" OFF)
option(WS2_HOOKS "Hook and log WS2 send/sendto/wsasend" ON)
option(DISABLE_GAME_STARTUP_TRASH
       "Suppress StorageSystem error 7 and AVolute error 20 in main log" ON)
option(LOG_WCONVERT "Log WString conversions" OFF)
option(ALLOC_LOG "Log String::Allocate" OFF)
option(RESERVE_LOG "Log String::Reserve" OFF)
option(CONCAT_LOG "Log String::Concatenate" OFF)
option(FORMAT_LOG "Log String::Vformat" OFF)

function(thegame_apply_options target)
  set(_defs
    THEGAME_NO_CONSOLE
    THEGAME_PIPES
    DISABLE_HOOKS
    DISABLE_SYSHOOKS
    DISABLE_ENTRYPOINT_HOOK
    DISABLE_VEH
    DISABLE_PARK_THREAD
    DISABLE_INT
    NO_NETWORK_LOGS
    SILENT_KEEPALIVE
    SILENT_NETWORK
    NO_PROUD_LOGS
    SILENT_PROUD
    WS2_HOOKS
    DISABLE_GAME_STARTUP_TRASH
    LOG_WCONVERT
    ALLOC_LOG
    RESERVE_LOG
    CONCAT_LOG
    FORMAT_LOG
  )
  foreach(_def IN LISTS _defs)
    if(${_def})
      target_compile_definitions(${target} PRIVATE ${_def}=1)
    else()
      target_compile_definitions(${target} PRIVATE ${_def}=0)
    endif()
  endforeach()
endfunction()
