#pragma once

#include <windows.h>

#include <string>

std::string wstring_to_string(LPCWSTR wideStr);

std::string string_to_string_safe(const char *str);
std::string wstring_to_string_safe(const wchar_t *str);

void json_escape(char *dst, size_t dst_size, const char *src);
