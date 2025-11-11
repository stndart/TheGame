#pragma once

#include <string>
#include <windows.h>

std::string wstring_to_string(LPCWSTR wideStr);

LPSTR __cdecl EnsureWStringBufferCapacity(LPSTR *str_p, int required_bytes,
                                          LPSTR inline_buf,
                                          int inline_buf_size_bytes);