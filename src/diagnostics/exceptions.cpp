#include <cstring>

#include "diagnostics/exceptions.hpp"

#include <intrin.h> // __debugbreak

#include "thegame/config.hpp"
#include "thegame/log.hpp"

using thegame::exceptionf;
using thegame::logf;
using thegame::LogMessage;

namespace Diagnostics {

thread_local bool g_in_vectored_handler = false;
PVOID g_veh_handle = nullptr;

// Most recent access violation seen by the VEH. Kept as named globals so they
// survive the int3 frame and are easy to read from an attached debugger.
EXCEPTION_RECORD g_av_record = {};
CONTEXT g_av_context = {};
unsigned long g_av_count = 0;

volatile LONG g_av_park;    // if 0, park thread is disabled
volatile LONG g_av_release; // set to 0 to release the thread

constexpr DWORD kDbgPrintExceptionC = 0x40010006;
constexpr DWORD kDbgPrintExceptionWideC = 0x4001000A;
constexpr DWORD kThreadNameExceptionC =
    0x406D1388; // MSVC debugger thread naming
constexpr DWORD kAccessViolationC = 0xC0000005;

bool is_dbgprint_exception(DWORD code) {
  return code == kDbgPrintExceptionC || code == kDbgPrintExceptionWideC;
}

bool is_benign_debugger_exception(DWORD code) {
  return is_dbgprint_exception(code) || code == kThreadNameExceptionC;
}

bool should_suppress_message(const char *message) {
#if DISABLE_GAME_STARTUP_TRASH
  if (!message)
    return false;
  if (std::strstr(message, "Error 7") != nullptr &&
      std::strstr(message, "StorageSystem::Registry::CNativeValue::Write") !=
          nullptr)
    return true;
  if (std::strstr(message, "Error 20") != nullptr &&
      std::strstr(message, "AVolute::GetProductInfoT") != nullptr)
    return true;
#endif
  (void)message;
  return false;
}

MiniDumpWriteDump_t g_minidump_fn = nullptr;

// BlackCipher/nProtect blinds x64dbg (clears DRx, hides threads, eats int3),
// so an in-process minidump is the only reliable way to inspect the fault.
// Load the resulting .dmp offline in WinDbg/x64dbg/IDA. One dump per process.
void write_av_minidump(EXCEPTION_POINTERS *info) {
  static LONG once = 0;
  if (!g_minidump_fn || InterlockedExchange(&once, 1) != 0)
    return;

  char exe[MAX_PATH];
  const DWORD n = GetModuleFileNameA(nullptr, exe, MAX_PATH);
  char *slash = (n > 0 && n < MAX_PATH) ? std::strrchr(exe, '\\') : nullptr;
  char dump[MAX_PATH];
  if (slash) {
    *slash = '\0';
    _snprintf_s(dump, sizeof(dump), _TRUNCATE, "%s\\TheGame_crash_%lu_%lu.dmp",
                exe, GetCurrentProcessId(), GetCurrentThreadId());
  } else {
    _snprintf_s(dump, sizeof(dump), _TRUNCATE, "TheGame_crash_%lu_%lu.dmp",
                GetCurrentProcessId(), GetCurrentThreadId());
  }

  HANDLE file = CreateFileA(dump, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                            FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE)
    return;

  MINIDUMP_EXCEPTION_INFORMATION mei;
  mei.ThreadId = GetCurrentThreadId();
  mei.ExceptionPointers = info;
  mei.ClientPointers = FALSE;

  const MINIDUMP_TYPE type = static_cast<MINIDUMP_TYPE>(
      MiniDumpWithFullMemory | MiniDumpWithHandleData | MiniDumpWithThreadInfo |
      MiniDumpWithUnloadedModules | MiniDumpWithFullMemoryInfo);

  g_minidump_fn(GetCurrentProcess(), GetCurrentProcessId(), file, type, &mei,
                nullptr, nullptr);
  CloseHandle(file);

  logf(LogMessage("minidump written: {}", dump));
}

// SEH must live in a function with no C++ unwinding (logf builds LogMessage).
static int append_fault_stack_dwords(const DWORD *sp, char *line, size_t cap,
                                     int pos) {
  __try {
    for (int i = 0; i < 8 && pos > 0; ++i) {
      const int n = _snprintf_s(line + pos, cap - static_cast<size_t>(pos),
                                _TRUNCATE, " 0x%08lX", sp[i]);
      if (n < 0)
        break;
      pos += n;
    }
  } __except (EXCEPTION_EXECUTE_HANDLER) {
  }
  return pos;
}

// Log the top stack dwords at the faulting esp. After a faulting `call` (incl.
// `call dword ptr [iat]`), [esp] is the return address right after the broken
// call -- resolve it against the module list to pin the exact call site.
void log_fault_stack(EXCEPTION_POINTERS *info) {
  CONTEXT *c = info->ContextRecord;
  if (!c)
    return;
  const DWORD *sp = reinterpret_cast<const DWORD *>(c->Esp);
  char line[256];
  const int pos = _snprintf_s(line, sizeof(line), _TRUNCATE,
                              "fault stack @esp=0x%08lX:", c->Esp);
  (void)append_fault_stack_dwords(sp, line, sizeof(line), pos);
  logf(LogMessage(line));
}

// Spin the faulting thread in place so a debugger can break-all and inspect the
// live fault frame -- works even when the thread is hidden from the debugger
// (BlackCipher), because that only suppresses debug events, not thread
// suspend/context reads. Safety cap ~10 min so an unattended run won't wedge.
void park_faulting_thread() {
  if (!g_av_park)
    return;
  g_av_release = 0;
  for (int i = 0; i < 6000 && !g_av_release; ++i)
    Sleep(100);
}

LONG WINAPI vectored_exception_handler(EXCEPTION_POINTERS *info) {
  // reentry guard
  if (g_in_vectored_handler)
    return EXCEPTION_CONTINUE_SEARCH;
  g_in_vectored_handler = true;

  EXCEPTION_RECORD *rec = info->ExceptionRecord;
  const DWORD code = rec->ExceptionCode;

  if (code == kAccessViolationC) {
    // Snapshot the faulting state so it is inspectable from an attached
    // debugger after the int3 below (named globals show up in the PDB).
    g_av_record = *rec;
    if (info->ContextRecord)
      g_av_context = *info->ContextRecord;
    ++g_av_count;

    exceptionf(info, "access_violation");

    // Handle the first AV fully. BlackCipher/nProtect neutralizes debugger-
    // planted breakpoints (clears DRx, hides threads, eats int3), so we:
    //  1) write a full minidump (offline analysis),
    //  2) log the top-of-stack return addresses (pins the broken call site),
    //  3) int3 (best effort; usually swallowed by the anti-cheat),
    //  4) park this thread so x64dbg break-all can inspect the LIVE fault frame
    //     -- parking works even though the thread is hidden from the debugger.

    static LONG handled_av = 0;
    if (InterlockedExchange(&handled_av, 1) == 0) {
      if (thegame::cfg.minidump_enabled) {
        write_av_minidump(info);
        log_fault_stack(info);
      }

      if (!thegame::cfg.disable_int) {
        __debugbreak();
      }

      if (!thegame::cfg.disable_park_thread) {
        park_faulting_thread();
      }
    }

    g_in_vectored_handler = false;
    return EXCEPTION_CONTINUE_SEARCH;
  }

  // the rest of exceptions go the usual path
  if (!is_benign_debugger_exception(code)) {
    if (rec->NumberParameters >= 2 &&
        !should_suppress_message(
            reinterpret_cast<const char *>(rec->ExceptionInformation[1]))) {
      exceptionf(info);
    }
  }

  g_in_vectored_handler = false;
  return EXCEPTION_CONTINUE_SEARCH;
}

LONG WINAPI unhandled_exception_handler(EXCEPTION_POINTERS *info) {
  exceptionf(info, "fatal_exception");
  return EXCEPTION_CONTINUE_SEARCH;
}

} // namespace Diagnostics