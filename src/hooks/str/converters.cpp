#include "console.h"
#include "helpers/strhelp.h"
#include "target_hooks.h"

#include "stringapiset.h"
#include <errhandlingapi.h>
#include <winerror.h>

int __cdecl EnsureWStringBufferCapacity(int *a1, int a2, int a3, int a4) {
  int result; // eax

  if (!a1 | (a2 < 0) | !a3)
    throw HRESULT_FROM_WIN32(ERROR_INVALID_PARAMETER);

  result = *a1;
  if (*a1 != a3) {
    if (a2 > a4) {
      result = _recalloc(result, a2, 1);
      if (!result)
        throw HRESULT_FROM_WIN32(ERROR_OUTOFMEMORY);
      *a1 = result;
      return result;
    }
    result = free(*a1);
    *a1 = a3;
    if (!*a1)
      throw HRESULT_FROM_WIN32(ERROR_OUTOFMEMORY);
    return result;
  }
  if (a2 <= a4) {
    *a1 = a3;
    if (!*a1)
      throw HRESULT_FROM_WIN32(ERROR_OUTOFMEMORY);
    return result;
  }
  result = calloc(a2, 1);

  *a1 = result;

  if (!*a1)
    throw HRESULT_FROM_WIN32(ERROR_OUTOFMEMORY);
  return result;
}

void __cdecl ConvertWideToMultiByte(LPSTR *target, LPCWSTR lpWideCharStr,
                                    UINT CodePage) {
  logf("ConvertWideToMultiByte: lpWideCharStr='%s', CodePage='%u'",
       wstring_to_string(lpWideCharStr).c_str(), CodePage);

  if (!lpWideCharStr) {
    *target = NULL;
    return;
  }

  int len = lstrlenW(lpWideCharStr) + 1;

  // ensure wstring buffer
  if (WideCharToMultiByte(CodePage, 0, lpWideCharStr, len, *target, 4 * len, 0,
                          0))
    return;

  if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
    int len2 = WideCharToMultiByte(CodePage, 0, lpWideCharStr, len, 0, 0, 0, 0);
    // ensure wstring buffer
    if (!WideCharToMultiByte(CodePage, 0, lpWideCharStr, len, *target, len2, 0,
                             0)) {
      // throw string conversion error
      throw HRESULT_FROM_WIN32(GetLastError());
    }
  }
}

extern "C" void __declspec(naked) hook_ConvertWideToMultiByte() {
  __asm {
    pushad // esp += 0x20
    pushfd // esp += 0x04

            // Push arguments in reverse order (__cdecl convention)
    mov eax, [esp + 0x2C] ; // CodePage
    mov ebx, [esp + 0x28] ; // lpWideCharStr 
    push eax
    push ebx
    push ecx ; // 'this' pointer (in ECX for __thiscall)
    
    call ConvertWideToMultiByte

        // Clean up stack (3 args * 4 bytes = 12)
    add esp, 12

    popfd
    popad

    ret

    // Execute original instructions that were overwritten
    // push ebx
    // push ebp
    // mov ebp, [esp + 0xC]

    // Jump back to original code after our patch
    // jmp [g_target_ConvertWideToMultiByte.return_addr]
  }
}

HookStub g_target_ConvertWideToMultiByte = {
    0x535740,
    (uint32_t)(uintptr_t)hook_ConvertWideToMultiByte,
    "hook_w_connect_1",
    {0},
    false,
    0x535746,
};