#pragma once

#include <windows.h>

#include <console.h>

/*
TODO:
sub_CF1DD0
sub_CF00F0
sub_CF0230 // String(LPCSTR pcStr) ?
sub_D59E20
sub_D5A900
sub_9FC6B0 // String(LPCSTR pcStr) ?
sub_9FCAA0 // String(const String &kStr) ?

// unrelated methods
sub_CF2B00 // GUID from String

// global vars
sub_156FBA0
sub_156FC90
sub_156FD40
sub_156FE40
*/

class String {
  static void __cdecl log_str(const String *handle, LPCSTR hint = "") {
    void *nullstr_hardcoded = *reinterpret_cast<void **>(0x017B75E4);
    if (handle == nullptr) {
      logf("log %s: null at %p", hint, handle);
    } else {
      String::StringBody *ptr = handle->m_kHandle;
      if (ptr == nullstr_hardcoded || ptr == nullptr) {
        logf("log %s: nullstr at %p at %p", hint, ptr, handle);
      } else {
        int *header =
            reinterpret_cast<int *>(reinterpret_cast<char *>(ptr) - 8);
        if (header) {
          int len = header[0];
          size_t cnt = header[1];
          int display_len = len > 15 ? 15 : len;
          char str[16];
          str[display_len] = 0;
          memcpy(str, ptr, display_len);
          logf("log %s: str at %p with len %i and refcnt %i starts as %s at %p",
               hint, ptr, len, cnt, str, handle);
        } else {
          logf("log %s: broken header at %p at %p", hint, ptr, handle);
        }
      }
    }
  }

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
  String(LPCSTR pcStr); // sub_CF0230 // TODO
  /// Constructs String from a part of input string. String is copied into
  /// internal buffer.
  String(LPCSTR pcStr, size_t stStrLength); // sub_CF00F0 // TODO
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

  /// Find the first occurence of a char and truncate the string before it.
  void TruncateAtFirstOccurrence(char ch); // sub_CF2D90
  /// Trim this character from the beginning of the string
  void TrimLeft(char ch); // sub_CF2CD0

  /// Add the String* to the end of this string, growing the buffer as
  /// necessary.
  void Concatenate(const String *pStr); // sub_CF1A90
  void Concatenate_cstr(LPCSTR pcStr);  // sub_CF1B50

public:
  static StringBody *Allocate(size_t stCount);
  static StringBody *AllocateAndCopy(LPCSTR pcStr, size_t stCount = 0);
  static void Deallocate(StringBody *&io_pBody);

  void IncRefCount();
  void DecRefCount(); // sub_9FCAF0
  size_t GetRefCount() const;

  void Swap(LPCSTR pcNewValue, size_t stLength = 0);
  LPCSTR Reserve(int stLength); // sub_CF0020
  void Realloc(int stLength);   // sub_CEFEF0

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

  void __thiscall w_old_reallok(int stLength);
};
