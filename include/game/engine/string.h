#pragma once

#include <windows.h>

void ThrowStringConversionError(DWORD errcode);

LPSTR __cdecl EnsureMStringBufferCapacity(LPSTR *str_p, int required_bytes,
                                          LPSTR inline_buf,
                                          int inline_buf_size_bytes);

LPWSTR __cdecl EnsureWStringBufferCapacity(LPWSTR *str_p, int required_bytes,
                                           LPWSTR inline_buf,
                                           int inline_buf_size_bytes);