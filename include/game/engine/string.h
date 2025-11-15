#pragma once

#include <windows.h>

class String {
public:
  inline LPCSTR c_str() const {
    if (!m_kHandle)
      return "";
    return GetString(m_kHandle);
  }

protected:
  /// Internal structure defining the header data for a string
  struct StringHeader {
    size_t m_cbBufferSize;
    size_t m_RefCount;
    // size_t m_cchStringLength;
  };
  /// Internal structure defining the body, used for providing type information.
  /// The actually buffer allocated is larger than StringBody making it safe to
  /// read beyond the one character defined in this class.
  struct StringBody {
    CHAR m_data[1];
  };
  /// The internal storage for the string
  struct StringData : public StringHeader, public StringBody {};

  /// A string's only member variable is a pointer directly to the Body of a
  /// StringData
  StringBody *m_kHandle;

public:
  // static StringBody *Allocate(size_t stCount);
  // static StringBody *AllocateAndCopy(LPCSTR pcStr, size_t stCount = 0);
  // static StringBody *AllocateAndCopyHandle(StringBody *kHandle);
  static void __thiscall Deallocate(StringBody *&io_pBody);

  static void __thiscall IncRefCount(StringBody *pBody, bool bValidate = true);
  void DecRefCount(); // sub_9FCAF0
  static size_t __thiscall GetRefCount(StringBody *pBody,
                                       bool bValidate = true);

  static char *__thiscall GetString(StringBody *pBody, bool bValidate = true);
  static size_t __thiscall GetLength(StringBody *pBody, bool bValidate = true);
  static size_t __thiscall GetAllocationSize(StringBody *pBody,
                                             bool bValidate = true);
  static size_t __thiscall GetBufferSize(StringBody *pBody,
                                         bool bValidate = true);
  static bool __thiscall ValidateString(StringBody *pBody);

  static void __thiscall SetLength(StringBody *pBody, size_t stLength);
  static StringHeader *__thiscall GetRealBufferStart(StringBody *pBody);

  void Truncate(int maxLength);                        // sub_CEFCC0
  static void __thiscall TruncateSelf(String **pBody); // sub_D5A7E0

  static StringBody *nullstr;
};
