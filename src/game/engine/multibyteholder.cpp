#include "console.h"
#include "helpers/strhelp.h"

#include "game/engine/multibyteholder.h"
#include "game/engine/string.h"

void __thiscall MultiByteHolder::ConvertWideToMultiByte(LPCWSTR lpWideCharStr,
                                                        UINT CodePage) {
  if (!lpWideCharStr) {
    current_ptr = NULL;
    if (LOG_WCONVERT)
      logf("ConvertWideToMultiByte: null lpWideCharStr");
    return;
  }

  if (LOG_WCONVERT)
    logf("ConvertWideToMultiByte: lpWideCharStr='%s', CodePage='%u'",
         wstring_to_string(lpWideCharStr).c_str(), CodePage);

  int len = lstrlenW(lpWideCharStr) + 1;

  EnsureMStringBufferCapacity(&current_ptr, len * 4, inline_buf,
                              MSTRING_INLINE_BUF_SIZE);
  if (WideCharToMultiByte(CodePage, 0, lpWideCharStr, len, current_ptr, 4 * len,
                          0, 0))
    return;

  if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
    int required_bytes =
        WideCharToMultiByte(CodePage, 0, lpWideCharStr, len, 0, 0, 0, 0);
    EnsureMStringBufferCapacity(&current_ptr, required_bytes, inline_buf,
                                MSTRING_INLINE_BUF_SIZE);
    if (!WideCharToMultiByte(CodePage, 0, lpWideCharStr, len, current_ptr,
                             required_bytes, 0, 0)) {
      ThrowStringConversionError(GetLastError());
    }
  }
}
