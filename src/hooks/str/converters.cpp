#include <atlexcept.h>

#include "console.h"
#include "helpers/strhelp.h"

#include "netstuff.h"
#include "target_hooks.h"

#define WSTRING_INLINE_BUF_SIZE 128

// struct MultiByteHolder {
//   LPSTR current_ptr; // points to heap buffer OR to the inline buffer
//   char inline_buf[WSTRING_INLINE_BUF_SIZE]; // embedded inline buffer
// };

void __cdecl ConvertWideToMultiByte(LPSTR *str_p, LPCWSTR lpWideCharStr,
                                    UINT CodePage) {
  if (!lpWideCharStr) {
    str_p[0] = NULL;
    logf("ConvertWideToMultiByte: null lpWideCharStr");
    return;
  }

  logf("ConvertWideToMultiByte: lpWideCharStr='%s', CodePage='%u'",
       wstring_to_string(lpWideCharStr).c_str(), CodePage);
  // return;

  int len = lstrlenW(lpWideCharStr) + 1;

  str_p[0] = EnsureWStringBufferCapacity(&(str_p[0]), len * 4,
                                         reinterpret_cast<LPSTR>(str_p + 1),
                                         WSTRING_INLINE_BUF_SIZE);
  if (WideCharToMultiByte(CodePage, 0, lpWideCharStr, len, str_p[0], 4 * len, 0,
                          0))
    return;

  if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
    int required_bytes =
        WideCharToMultiByte(CodePage, 0, lpWideCharStr, len, 0, 0, 0, 0);
    str_p[0] = EnsureWStringBufferCapacity(&(str_p[0]), required_bytes,
                                           reinterpret_cast<LPSTR>(str_p + 1),
                                           WSTRING_INLINE_BUF_SIZE);
    if (!WideCharToMultiByte(CodePage, 0, lpWideCharStr, len, str_p[0],
                             required_bytes, 0, 0)) {
      // throw string conversion error
      throw ATL::CAtlException(HRESULT_FROM_WIN32(GetLastError()));
    }
  }
}

extern "C" void __declspec(naked) hook_ConvertWideToMultiByte() {
  __asm {
    pushad // esp += 0x20

    mov eax, [esp + 0x24] ; // lpWideCharStr
    mov ebx, [esp + 0x28] ; // CodePage
    push ebx
    push eax
    push ecx ; // 'this' pointer (in ECX for __thiscall)
    
    call ConvertWideToMultiByte

        // Clean up stack (3 args * 4 bytes = 12)
    add esp, 12

    mov [esp + 0x1C], eax
    mov [esp + 0x18], ecx
    mov [esp + 0x14], edx

    popad

    ret 8

    // Execute original instructions that were overwritten and jump
    // back
    push ebx
    push ebp
    mov ebp, [esp + 0xC]
    jmp [g_target_ConvertWideToMultiByte.return_addr]
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