#pragma once

#include <windows.h>

namespace Diagnostics {

void start_from_entrypoint();
void shutdown();
void emit_log(const char *message);
void emit_event_line(const char *line);

} // namespace Diagnostics

extern "C" void __cdecl diagnostics_start_from_entrypoint();
