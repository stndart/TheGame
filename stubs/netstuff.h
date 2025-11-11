#pragma once

#include <windows.h>

LPSTR __cdecl EnsureWStringBufferCapacity(LPSTR *str_p, int required_bytes,
                                          LPSTR inline_buf,
                                          int inline_buf_size_bytes);