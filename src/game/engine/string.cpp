#include "crt/memory.h"
#include <atlexcept.h>

#include "game/engine/string.h"

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