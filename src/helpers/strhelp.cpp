#include "helpers/strhelp.h"

#include <codecvt>

std::string wstring_to_string(LPCWSTR wideStr) {
  if (!wideStr)
    return "null";

  std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
  return converter.to_bytes(wideStr);
}