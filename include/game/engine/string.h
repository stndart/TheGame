#pragma once

#include <windows.h>

class String {
public:
  inline LPCSTR c_str() const {
    if (!m_kHandle)
      return "";
    return GetString();
  }

protected:
#pragma pack(push, 1) // Disable padding, use 1-byte alignment
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
#pragma pack(pop) // Restore default alignment

  static_assert(sizeof(StringData) == 8 + 1,
                "StringData should be 9 bytes with 1-byte alignment");

  /// A string's only member variable is a pointer directly to the Body of a
  /// StringData
  StringBody *m_kHandle;

public:
  /// Constructs an empty String.
  String() : m_kHandle(nullstr) {};
  /// Constructs String from another String. Increments RefCount only, no data
  /// is copied.
  String(const String &kStr);
  /// Constructs String from an input string. String is copied into internal
  /// buffer.
  String(LPCSTR pcStr);
  /// Constructs String from a part of input string. String is copied into
  /// internal buffer.
  String(LPCSTR pcStr, size_t stStrLength);
  /// Constructs an empty String, allocating given buffer size.
  // String(size_t stBuffLength); // disabled since our version does not
  // separate allocated buffer and string length
  /// Constructs String from a single char.
  String(char ch);
  /// Destroys String. Refcount is decremented and if it equals zero, the buffer
  /// is deleted.
  ~String() { DecRefCount(); };

  /// Ref-counted copy from another String
  inline String &operator=(const String &kStr);
  /// Copy string content from string
  inline String &operator=(LPCSTR pcStr);
  /// Set string content to single character
  inline String &operator=(char ch);

  static StringBody *Allocate(size_t stCount);
  static StringBody *AllocateAndCopy(LPCSTR pcStr, size_t stCount = 0);
  static void Deallocate(StringBody *&io_pBody);

  void IncRefCount();
  void DecRefCount(); // sub_9FCAF0
  size_t GetRefCount() const;

  void Swap(LPCSTR pcNewValue, size_t stLength = 0);

  LPSTR GetString() const;
  size_t GetLength() const;
  void SetLength(size_t stLength);

  void SetBuffer(StringBody *pBody);
  void CalcLength();

  void Truncate(int maxLength);                        // sub_CEFCC0
  static void __thiscall TruncateSelf(String **pBody); // sub_D5A7E0

  static bool __thiscall ValidateString(StringBody *pBody);

  static StringHeader *__thiscall GetRealBufferStart(StringBody *pBody);

  void CopyOnWrite(); // sub_CEFDF0

  static StringBody *nullstr;
};
