#pragma once

#include <windows.h>

#define WSTRING_INLINE_BUF_SIZE 128

struct WideStringHolder {
  LPWSTR current_ptr; // points to heap buffer OR to the inline buffer
  WCHAR inline_buf[WSTRING_INLINE_BUF_SIZE]; // embedded inline buffer

  void __thiscall ConvertMultiByteToWide(LPCSTR lpCharStr, UINT CodePage);
};