#pragma once

#include <windows.h>

void ThrowStringConversionError(DWORD errcode);

LPSTR __cdecl EnsureStringBufferCapacity(LPSTR *str_p, int required_bytes,
                                         LPSTR inline_buf,
                                         int inline_buf_size_bytes);