#pragma once

#include <windows.h>

namespace Diagnostics {

void startup();
void teardown();
void emit_game_state(const char *phase);
void emit_game_log(const char *message);
void emit_exception_event(const char *type, EXCEPTION_POINTERS *info);

} // namespace Diagnostics

extern "C" void __cdecl diagnostics_startup();