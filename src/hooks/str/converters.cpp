#include <atlexcept.h>
#include <errhandlingapi.h>

#include "console.h"
#include "helpers/strhelp.h"

#include "netstuff.h"
#include "target_hooks.h"

#define WSTRING_INLINE_BUF_SIZE 128

void ThrowStringConversionError(DWORD errcode) {
  throw ATL::CAtlException(HRESULT_FROM_WIN32(errcode));
}

struct MultiByteHolder {
  LPSTR current_ptr; // points to heap buffer OR to the inline buffer
  char inline_buf[WSTRING_INLINE_BUF_SIZE]; // embedded inline buffer

  void __thiscall ConvertWideToMultiByte(LPCWSTR lpWideCharStr, UINT CodePage) {
    if (!lpWideCharStr) {
      current_ptr = NULL;
      logf("ConvertWideToMultiByte: null lpWideCharStr");
      return;
    }

    logf("ConvertWideToMultiByte: lpWideCharStr='%s', CodePage='%u'",
         wstring_to_string(lpWideCharStr).c_str(), CodePage);

    int len = lstrlenW(lpWideCharStr) + 1;

    EnsureWStringBufferCapacity(&current_ptr, len * 4, inline_buf,
                                WSTRING_INLINE_BUF_SIZE);
    if (WideCharToMultiByte(CodePage, 0, lpWideCharStr, len, current_ptr,
                            4 * len, 0, 0))
      return;

    if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
      int required_bytes =
          WideCharToMultiByte(CodePage, 0, lpWideCharStr, len, 0, 0, 0, 0);
      EnsureWStringBufferCapacity(&current_ptr, required_bytes, inline_buf,
                                  WSTRING_INLINE_BUF_SIZE);
      if (!WideCharToMultiByte(CodePage, 0, lpWideCharStr, len, current_ptr,
                               required_bytes, 0, 0)) {
        ThrowStringConversionError(GetLastError());
      }
    }
  }
};

extern "C" void __declspec(naked) hook_ConvertWideToMultiByte() {
  __asm {
    jmp MultiByteHolder::ConvertWideToMultiByte
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