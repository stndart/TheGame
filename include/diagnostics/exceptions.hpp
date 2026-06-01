#include <windows.h>

#include <dbghelp.h> // MINIDUMP_* types (MiniDumpWriteDump resolved at runtime)

using MiniDumpWriteDump_t = BOOL(WINAPI *)(HANDLE, DWORD, HANDLE, MINIDUMP_TYPE,
                                           PMINIDUMP_EXCEPTION_INFORMATION,
                                           PMINIDUMP_USER_STREAM_INFORMATION,
                                           PMINIDUMP_CALLBACK_INFORMATION);

namespace Diagnostics {

extern PVOID g_veh_handle;
extern MiniDumpWriteDump_t g_minidump_fn;
extern volatile LONG g_av_park;
extern volatile LONG g_av_release;

bool is_dbgprint_exception(DWORD code);
bool should_suppress_message(const char *message);

LONG WINAPI vectored_exception_handler(EXCEPTION_POINTERS *info);

LONG WINAPI unhandled_exception_handler(EXCEPTION_POINTERS *info);

} // namespace Diagnostics