#pragma once

#include <windows.h>

#define WSTRING_INLINE_BUF_SIZE 128
#define MSTRING_INLINE_BUF_SIZE 128

const bool LOG_WCONVERT = false;

void ThrowStringConversionError(DWORD errcode);

LPSTR __cdecl EnsureMStringBufferCapacity(LPSTR *str_p, int required_bytes,
                                          LPSTR inline_buf,
                                          int inline_buf_size_bytes);

LPWSTR __cdecl EnsureWStringBufferCapacity(LPWSTR *str_p, int required_bytes,
                                           LPWSTR inline_buf,
                                           int inline_buf_size_bytes);

struct WideStringHolder {
  LPWSTR current_ptr; // points to heap buffer OR to the inline buffer
  WCHAR inline_buf[WSTRING_INLINE_BUF_SIZE]; // embedded inline buffer

  void __thiscall ConvertMultiByteToWide(LPCSTR lpCharStr, UINT CodePage);
};

struct MultiByteHolder {
  LPSTR current_ptr; // points to heap buffer OR to the inline buffer
  char inline_buf[MSTRING_INLINE_BUF_SIZE]; // embedded inline buffer

  void __thiscall ConvertWideToMultiByte(LPCWSTR lpWideCharStr, UINT CodePage);
};