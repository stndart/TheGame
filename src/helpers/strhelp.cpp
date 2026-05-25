#include "helpers/strhelp.h"

#include <codecvt>

std::string wstring_to_string(LPCWSTR wideStr, size_t length) {
  if (!wideStr)
    return {};

  std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
  return converter.to_bytes(wideStr, wideStr + length);
}

std::string wstring_to_string(LPCWSTR wideStr) {
  if (!wideStr)
    return {};

  std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
  return converter.to_bytes(wideStr);
}

std::string string_to_string_safe(const char *str) {
  if (!str)
    return {};

  __try {
    return std::string(str, strnlen_s(str, 4096));
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return {};
  }
}

std::string wstring_to_string_safe(const wchar_t *str) {
  if (!str)
    return {};

  __try {
    return wstring_to_string(str, wcsnlen_s(str, 4096));
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return {};
  }
}

void json_escape(char *dst, size_t dst_size, const char *src) {
  if (!dst_size)
    return;

  size_t pos = 0;
  for (const unsigned char *p = reinterpret_cast<const unsigned char *>(src);
       p && *p && pos + 1 < dst_size; ++p) {
    if (*p == '"' || *p == '\\') {
      if (pos + 2 >= dst_size)
        break;
      dst[pos++] = '\\';
      dst[pos++] = static_cast<char>(*p);
    } else if (*p == '\n') {
      if (pos + 2 >= dst_size)
        break;
      dst[pos++] = '\\';
      dst[pos++] = 'n';
    } else if (*p == '\r') {
      if (pos + 2 >= dst_size)
        break;
      dst[pos++] = '\\';
      dst[pos++] = 'r';
    } else if (*p == '\t') {
      if (pos + 2 >= dst_size)
        break;
      dst[pos++] = '\\';
      dst[pos++] = 't';
    } else if (*p < 0x20) {
      if (pos + 6 >= dst_size)
        break;
      int written = _snprintf_s(dst + pos, dst_size - pos, _TRUNCATE, "\\u%04x",
                                static_cast<unsigned>(*p));
      if (written < 0)
        break;
      pos += static_cast<size_t>(written);
    } else {
      dst[pos++] = static_cast<char>(*p);
    }
  }
  dst[pos] = '\0';
}