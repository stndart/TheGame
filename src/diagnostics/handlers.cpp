#include <cstdio> // IWYU pragma: keep
#include <cstring>

#ifdef THEGAME_BREAK_ON_AV
#include <windows.h> // must precede dbghelp.h (IMAGE_* types)

#include <intrin.h>  // __debugbreak
#include <dbghelp.h> // MINIDUMP_* types (MiniDumpWriteDump resolved at runtime)

using MiniDumpWriteDump_t = BOOL(WINAPI *)(
    HANDLE, DWORD, HANDLE, MINIDUMP_TYPE, PMINIDUMP_EXCEPTION_INFORMATION,
    PMINIDUMP_USER_STREAM_INFORMATION, PMINIDUMP_CALLBACK_INFORMATION);
#endif

#include "RMI/Nav.hpp"
#include "diagnostics/handler_pipe.hpp"
#include "diagnostics/handlers.hpp"
#include "diagnostics/namedpipe.hpp"
#include "helpers/strhelp.h"

using namespace Diagnostics;

namespace Diagnostics {
// Defined further down; forward-declared for the VEH below.
extern EXCEPTION_RECORD g_av_record;
extern CONTEXT g_av_context;
extern unsigned long g_av_count;
#ifdef THEGAME_BREAK_ON_AV
// Debugger knobs (named in the PDB so they are flippable from x64dbg):
//  g_av_park    - 1 = spin the faulting thread on the first AV so break-all
//                 can inspect it; set 0 to disable parking.
//  g_av_release - set to 1 from the debugger to let the parked thread continue.
extern volatile LONG g_av_park;
extern volatile LONG g_av_release;
#endif
} // namespace Diagnostics

namespace {

constexpr DWORD kDbgPrintExceptionC = 0x40010006;
constexpr DWORD kDbgPrintExceptionWideC = 0x4001000A;
constexpr DWORD kAccessViolationC = 0xC0000005;

// Re-entrancy guard: emit_game_log / pipe I/O must not recurse through this
// VEH.
thread_local bool g_in_vectored_handler = false;

// Direct stdout write for crash telemetry. Must not use printf/log_message/
// emit_game_log (VEH DbgPrint path and pipe locks).
void write_exception_console(const char *line) {
#ifndef THEGAME_NO_CONSOLE
  if (!line || !line[0])
    return;
  HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
  if (!out || out == INVALID_HANDLE_VALUE)
    return;
  DWORD written = 0;
  const DWORD len = static_cast<DWORD>(strlen(line));
  WriteFile(out, line, len, &written, nullptr);
  WriteFile(out, "\r\n", 2, &written, nullptr);
#endif
}

bool is_dbgprint_exception(DWORD code) {
  return code == kDbgPrintExceptionC || code == kDbgPrintExceptionWideC;
}

#ifdef THEGAME_BREAK_ON_AV
// Resolved once at startup so the fault path never touches the loader lock.
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

  char msg[MAX_PATH + 32];
  _snprintf_s(msg, sizeof(msg), _TRUNCATE, "minidump written: %s", dump);
  Diagnostics::emit_game_log(msg);
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
  int pos = _snprintf_s(line, sizeof(line), _TRUNCATE,
                        "fault stack @esp=0x%08lX:", c->Esp);
  __try {
    for (int i = 0; i < 8 && pos > 0; ++i) {
      const int n = _snprintf_s(line + pos, sizeof(line) - pos, _TRUNCATE,
                                " 0x%08lX", sp[i]);
      if (n < 0)
        break;
      pos += n;
    }
  } __except (EXCEPTION_EXECUTE_HANDLER) {
  }
  Diagnostics::emit_game_log(line);
}

// Spin the faulting thread in place so a debugger can break-all and inspect the
// live fault frame -- works even when the thread is hidden from the debugger
// (BlackCipher), because that only suppresses debug events, not thread
// suspend/context reads. Safety cap ~10 min so an unattended run won't wedge.
void park_faulting_thread() {
  if (!Diagnostics::g_av_park)
    return;
  Diagnostics::g_av_release = 0;
  for (int i = 0; i < 6000 && !Diagnostics::g_av_release; ++i)
    Sleep(100);
}
#endif

LONG WINAPI vectored_exception_handler(EXCEPTION_POINTERS *info) {
  if (g_in_vectored_handler)
    return EXCEPTION_CONTINUE_SEARCH;
  g_in_vectored_handler = true;

  EXCEPTION_RECORD *rec = info->ExceptionRecord;
  const DWORD code = rec->ExceptionCode;

  if (code == kDbgPrintExceptionC && rec->NumberParameters >= 2) {
    const char *msg =
        reinterpret_cast<const char *>(rec->ExceptionInformation[1]);
    Diagnostics::emit_game_log(string_to_string_safe(msg).c_str());
    g_in_vectored_handler = false;
    return EXCEPTION_CONTINUE_SEARCH;
  }
  if (code == kDbgPrintExceptionWideC && rec->NumberParameters >= 2) {
    const wchar_t *wmsg =
        reinterpret_cast<const wchar_t *>(rec->ExceptionInformation[1]);
    Diagnostics::emit_game_log(wstring_to_string_safe(wmsg).c_str());
    g_in_vectored_handler = false;
    return EXCEPTION_CONTINUE_SEARCH;
  }

  if (code == kAccessViolationC) {
    // Snapshot the faulting state so it is inspectable from an attached
    // debugger after the int3 below (named globals show up in the PDB).
    Diagnostics::g_av_record = *rec;
    if (info->ContextRecord)
      Diagnostics::g_av_context = *info->ContextRecord;
    ++Diagnostics::g_av_count;

    Diagnostics::emit_exception_event("access_violation", info);

#ifdef THEGAME_BREAK_ON_AV
    // Handle the first AV fully. BlackCipher/nProtect neutralizes debugger-
    // planted breakpoints (clears DRx, hides threads, eats int3), so we:
    //  1) write a full minidump (offline analysis),
    //  2) log the top-of-stack return addresses (pins the broken call site),
    //  3) int3 (best effort; usually swallowed by the anti-cheat),
    //  4) park this thread so x64dbg break-all can inspect the LIVE fault frame
    //     -- parking works even though the thread is hidden from the debugger.
    static LONG handled_av = 0;
    if (InterlockedExchange(&handled_av, 1) == 0) {
      write_av_minidump(info);
      log_fault_stack(info);
      __debugbreak();
      park_faulting_thread();
    }
#endif
    g_in_vectored_handler = false;
    return EXCEPTION_CONTINUE_SEARCH;
  }

  if (!is_dbgprint_exception(code))
    Diagnostics::emit_exception_event("exception", info);

  g_in_vectored_handler = false;
  return EXCEPTION_CONTINUE_SEARCH;
}

LONG WINAPI unhandled_exception_handler(EXCEPTION_POINTERS *info) {
  Diagnostics::emit_exception_event("fatal_exception", info);
  return EXCEPTION_CONTINUE_SEARCH;
}

}; // namespace

namespace Diagnostics {

bool should_suppress_game_log(const char *message) {
  if (!message)
    return false;
  if (std::strstr(message, "Error 7") != nullptr &&
      std::strstr(message, "StorageSystem::Registry::CNativeValue::Write") !=
          nullptr)
    return true;
  if (std::strstr(message, "Error 20") != nullptr &&
      std::strstr(message, "AVolute::GetProductInfoT") != nullptr)
    return true;
  return false;
}

PVOID g_veh_handle = nullptr;
LONG g_handling_exception = 0;
bool g_diagnostics_started = false;
char g_last_game_phase[32] = "";

// Most recent access violation seen by the VEH. Kept as named globals so they
// survive the int3 frame and are easy to read from an attached debugger.
EXCEPTION_RECORD g_av_record = {};
CONTEXT g_av_context = {};
unsigned long g_av_count = 0;
#ifdef THEGAME_BREAK_ON_AV
volatile LONG g_av_park = 1;
volatile LONG g_av_release = 0;
#endif

NamedPipe *g_pipe = nullptr;

NamedPipe *connect_pipe() {
  if (!g_pipe) {
    g_pipe = new NamedPipe(kPipeName);
  }
  if (!g_pipe || !g_pipe->connect_pipe())
    return nullptr;
  return g_pipe;
}

void startup() {
  if (g_diagnostics_started)
    return;

#ifndef THEGAME_NO_VEH
  if (!g_veh_handle)
    g_veh_handle = AddVectoredExceptionHandler(1, vectored_exception_handler);
  SetUnhandledExceptionFilter(unhandled_exception_handler);
#endif

#ifdef THEGAME_BREAK_ON_AV
  // Preload dbghelp now so the AV path (which may hold the loader lock) only
  // needs to call the cached function pointer.
  if (!g_minidump_fn) {
    if (HMODULE dbghelp = LoadLibraryA("dbghelp.dll"))
      g_minidump_fn = reinterpret_cast<MiniDumpWriteDump_t>(
          GetProcAddress(dbghelp, "MiniDumpWriteDump"));
  }
#endif

  for (int i = 0; i < 300; ++i) {
    g_pipe = connect_pipe();
    if (g_pipe)
      break;
    Sleep(100);
  }
  if (!g_pipe)
    return;

  g_diagnostics_started = true;

  char line[160];
  _snprintf_s(line, sizeof(line), _TRUNCATE,
              "{\"type\":\"hello\",\"pid\":%lu,\"version\":1}",
              GetCurrentProcessId());

  g_pipe->write_line_locked(line);
  emit_game_state("started");

  HandlerPipe::start();
  Rmi::NavLogStartup();
}

bool started() { return g_diagnostics_started; }

void teardown() {
  g_diagnostics_started = false;
  HandlerPipe::stop();

  if (g_veh_handle) {
    RemoveVectoredExceptionHandler(g_veh_handle);
    g_veh_handle = nullptr;
  }

  if (g_pipe) {
    delete g_pipe;
    g_pipe = nullptr;
  }
}

void emit_game_state(const char *phase) {
  g_pipe = connect_pipe();
  if (!g_pipe || !phase)
    return;

  if (strncmp(g_last_game_phase, phase, sizeof(g_last_game_phase)) == 0)
    return;

  _snprintf_s(g_last_game_phase, sizeof(g_last_game_phase), _TRUNCATE, "%s",
              phase);

  char escaped[64];
  json_escape(escaped, sizeof(escaped), phase);

  char line[256];
  _snprintf_s(line, sizeof(line), _TRUNCATE,
              "{\"type\":\"game_state\",\"pid\":%lu,\"phase\":\"%s\"}",
              GetCurrentProcessId(), escaped);
  g_pipe->write_line_locked(line);
}

void emit_game_log(const char *message) {
  if (should_suppress_game_log(message))
    return;

  g_pipe = connect_pipe();
  if (!g_pipe)
    return;

  char escaped[900];
  json_escape(escaped, sizeof(escaped), message ? message : "");

  char line[1024];
  _snprintf_s(line, sizeof(line), _TRUNCATE,
              "{\"type\":\"log\",\"pid\":%lu,\"message\":\"%s\"}",
              GetCurrentProcessId(), escaped);
  g_pipe->write_line_locked(line);
}

void emit_proudnet_tcp(DWORD thread_id, unsigned port, const char *direction,
                       unsigned long long sock, size_t chunk_len,
                       const PnTcpFrameHeader *frames, size_t frame_count,
                       size_t incomplete_tail) {
  g_pipe = connect_pipe();
  if (!g_pipe)
    return;

  char line[2048];
  int pos = _snprintf_s(
      line, sizeof(line), _TRUNCATE,
      "{\"type\":\"proudnet-tcp\",\"pid\":%lu,\"thread_id\":%lu,\"port\":%u,"
      "\"dir\":\"%s\",\"sock\":\"0x%llX\",\"chunk_len\":%zu,\"incomplete\":%zu,"
      "\"frames\":[",
      GetCurrentProcessId(), thread_id, static_cast<unsigned>(port),
      direction ? direction : "", static_cast<unsigned long long>(sock),
      chunk_len, incomplete_tail);

  if (pos < 0)
    return;

  for (size_t i = 0; i < frame_count && pos > 0; ++i) {
    const PnTcpFrameHeader &f = frames[i];
    const int n = _snprintf_s(
        line + pos, sizeof(line) - static_cast<size_t>(pos), _TRUNCATE,
        "%s{\"payload_len\":%u,\"opcode\":%u,\"rmi_id\":%u,\"body_len\":%u,"
        "\"has_rmi\":%s}",
        (i ? "," : ""), f.payload_len, f.opcode, f.rmi_id, f.body_len,
        f.has_rmi ? "true" : "false");
    if (n < 0)
      break;
    pos += n;
    if (static_cast<size_t>(pos) >= sizeof(line) - 4)
      break;
  }

  if (pos > 0 && static_cast<size_t>(pos) < sizeof(line) - 2)
    _snprintf_s(line + pos, sizeof(line) - static_cast<size_t>(pos), _TRUNCATE,
                "]}");

  g_pipe->write_line_locked(line);
}

void emit_proudnet_tcp_connect(DWORD thread_id, unsigned long long sock,
                               const char *addr, unsigned port) {
  g_pipe = connect_pipe();
  if (!g_pipe)
    return;

  char escaped[128];
  json_escape(escaped, sizeof(escaped), addr ? addr : "");

  char line[384];
  _snprintf_s(
      line, sizeof(line), _TRUNCATE,
      "{\"type\":\"proudnet-tcp\",\"event\":\"connect\",\"pid\":%lu,"
      "\"thread_id\":%lu,\"port\":%u,\"sock\":\"0x%llX\",\"addr\":\"%s\"}",
      GetCurrentProcessId(), thread_id, static_cast<unsigned>(port),
      static_cast<unsigned long long>(sock), escaped);
  g_pipe->write_line_locked(line);
}

void emit_exception_event(const char *type, EXCEPTION_POINTERS *info) {
  if (InterlockedExchange(&g_handling_exception, 1) != 0)
    return;

  char line[768];
  if (format_exception_event(line, sizeof(line), type, info)) {
    g_pipe = connect_pipe();
    if (g_pipe)
      g_pipe->write_line_unlocked(line);
    write_exception_console(line);
  }

  InterlockedExchange(&g_handling_exception, 0);
}

} // namespace Diagnostics
