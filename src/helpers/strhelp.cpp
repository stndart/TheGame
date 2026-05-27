#include "helpers/strhelp.h"

#include <codecvt>
#include <cstring>

namespace {

constexpr size_t kSafeStringMaxLen = 4096;

bool try_copy_cstr(const char *str, char *buf, size_t buf_cap, size_t *out_len) {
  *out_len = 0;
  __try {
    const size_t len = strnlen_s(str, buf_cap);
    memcpy(buf, str, len);
    *out_len = len;
    return true;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
}

bool try_copy_wcstr(const wchar_t *str, wchar_t *buf, size_t buf_cap,
                    size_t *out_len) {
  *out_len = 0;
  __try {
    const size_t len = wcsnlen_s(str, buf_cap);
    memcpy(buf, str, len * sizeof(wchar_t));
    *out_len = len;
    return true;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
}

} // namespace

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

  char buf[kSafeStringMaxLen];
  size_t len = 0;
  if (!try_copy_cstr(str, buf, kSafeStringMaxLen, &len))
    return {};
  return std::string(buf, len);
}

std::string wstring_to_string_safe(const wchar_t *str) {
  if (!str)
    return {};

  wchar_t buf[kSafeStringMaxLen];
  size_t len = 0;
  if (!try_copy_wcstr(str, buf, kSafeStringMaxLen, &len))
    return {};
  return wstring_to_string(buf, len);
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