#pragma once

#include <windows.h>

#define WSTRING_INLINE_BUF_SIZE 128

struct MultiByteHolder {
  LPSTR current_ptr; // points to heap buffer OR to the inline buffer
  char inline_buf[WSTRING_INLINE_BUF_SIZE]; // embedded inline buffer

  void __thiscall ConvertWideToMultiByte(LPCWSTR lpWideCharStr, UINT CodePage);
};