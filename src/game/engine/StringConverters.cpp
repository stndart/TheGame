#include "crt/memory.h"
#include <console.h>

#include <atlexcept.h>

#include "game/engine/StringConverters.h"

void ThrowStringConversionError(DWORD errcode) {
  throw ATL::CAtlException(HRESULT_FROM_WIN32(errcode));
}

LPSTR __cdecl EnsureMStringBufferCapacity(LPSTR *str_p, int required_bytes,
                                          LPSTR inline_buf,
                                          int inline_buf_size_bytes) {
  if (!str_p || (required_bytes < 0) || !inline_buf)
    throw ATL::CAtlException(E_INVALIDARG);

  if (*str_p != inline_buf) {                     // if using heap
    if (required_bytes > inline_buf_size_bytes) { // reallocate on heap
      *str_p = reinterpret_cast<LPSTR>(
          CRT::recalloc(reinterpret_cast<void *>(*str_p), required_bytes, 1));
      if (!*str_p)
        throw ATL::CAtlException(E_OUTOFMEMORY);
    } else { // use stack buf, since it's sufficient
      CRT::free(*str_p);
      *str_p = inline_buf;
    }
  } else { // str_p is pointing at stack_buf
    if (required_bytes > inline_buf_size_bytes) { // use heap only if needed
      *str_p = reinterpret_cast<LPSTR>(CRT::calloc(required_bytes, 1));
      if (!*str_p)
        throw ATL::CAtlException(E_OUTOFMEMORY);
    }
  }

  return *str_p;
}

LPWSTR __cdecl EnsureWStringBufferCapacity(LPWSTR *str_p, int required_bytes,
                                           LPWSTR inline_buf,
                                           int inline_buf_size_bytes) {
  if (!str_p || (required_bytes < 0) || !inline_buf)
    throw ATL::CAtlException(E_INVALIDARG);

  if (*str_p != inline_buf) {                     // if using heap
    if (required_bytes > inline_buf_size_bytes) { // reallocate on heap
      *str_p = reinterpret_cast<LPWSTR>(
          CRT::recalloc(reinterpret_cast<void *>(*str_p), required_bytes, 1));
      if (!*str_p)
        throw ATL::CAtlException(E_OUTOFMEMORY);
    } else { // use stack buf, since it's sufficient
      CRT::free(*str_p);
      *str_p = inline_buf;
    }
  } else { // str_p is pointing at stack_buf
    if (required_bytes > inline_buf_size_bytes) { // use heap only if needed
      *str_p = reinterpret_cast<LPWSTR>(CRT::calloc(required_bytes, 1));
      if (!*str_p)
        throw ATL::CAtlException(E_OUTOFMEMORY);
    }
  }

  return *str_p;
}

void __thiscall MultiByteHolder::ConvertWideToMultiByte(LPCWSTR lpWideCharStr,
                                                        UINT CodePage) {
  if (!lpWideCharStr) {
    current_ptr = NULL;
    if (LOG_WCONVERT)
      logf("ConvertWideToMultiByte: null lpWideCharStr");
    return;
  }

  if (LOG_WCONVERT)
    logf("ConvertWideToMultiByte: lpWideCharStr='%ls', CodePage='%u'",
         lpWideCharStr, CodePage);

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
