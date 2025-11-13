#include "console.h"

#include "game/engine/string.h"
#include "game/engine/widestringholder.h"

void __thiscall WideStringHolder::ConvertMultiByteToWide(LPCSTR lpMultiByteStr,
                                                         UINT CodePage) {
  if (!lpMultiByteStr) {
    current_ptr = NULL;
    if (LOG_WCONVERT)
      logf("ConvertMultiByteToWide: null lpMultiByteStr");
    return;
  }

  if (LOG_WCONVERT)
    logf("ConvertMultiByteToWide: lpMultiByteStr='%s', CodePage='%u'",
         lpMultiByteStr, CodePage);

  int len = lstrlenA(lpMultiByteStr) + 1;

  EnsureWStringBufferCapacity(&current_ptr, len * 4, inline_buf,
                              WSTRING_INLINE_BUF_SIZE);
  if (MultiByteToWideChar(CodePage, 0, lpMultiByteStr, len, current_ptr,
                          4 * len))
    return;

  if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
    int required_bytes =
        MultiByteToWideChar(CodePage, 0, lpMultiByteStr, len, 0, 0);
    EnsureWStringBufferCapacity(&current_ptr, required_bytes, inline_buf,
                                WSTRING_INLINE_BUF_SIZE);
    if (!MultiByteToWideChar(CodePage, 0, lpMultiByteStr, len, current_ptr,
                             required_bytes)) {
      ThrowStringConversionError(GetLastError());
    }
  }
}
