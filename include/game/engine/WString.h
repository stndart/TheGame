#pragma once

#include <windows.h>

#include <console.h>

class WString {
  static void __cdecl log_str(const WString *handle, LPCSTR hint = "") {
    void *nullstr_hardcoded = *reinterpret_cast<void **>(0x017B75E4);
    if (handle == nullptr) {
      logf("log %s: null at %p", hint, handle);
    } else {
      WString::WStringBody *ptr = handle->m_kHandle;
      if (ptr == nullstr_hardcoded || ptr == nullptr) {
        logf("log %s: nullstr at %p at %p", hint, ptr, handle);
      } else {
        int *header =
            reinterpret_cast<int *>(reinterpret_cast<char *>(ptr) - 8);
        if (header) {
          int len = header[0];
          size_t cnt = header[1];
          int display_len = len > 15 ? 15 : len;
          WCHAR wstr[16];
          wstr[display_len] = L'\0';
          memcpy(wstr, ptr, display_len * sizeof(WCHAR));

          logf(
              "log %s: str at %p with len %i and refcnt %i starts as %ls at %p",
              hint, ptr, len, cnt, wstr, handle);
        } else {
          logf("log %s: broken header at %p at %p", hint, ptr, handle);
        }
      }
    }
  }

public:
  inline LPCWSTR c_str() const {
    if (!m_kHandle)
      return L"";
    return GetString();
  }

protected:
#pragma pack(push, 1) // Disable padding, use 1-byte alignment
  /// Internal structure defining the header data for a string
  struct WStringHeader {
    size_t m_cbBufferSize;
    size_t m_RefCount;
    // size_t m_cchStringLength;
  };
  /// Internal structure defining the body, used for providing type information.
  /// The actually buffer allocated is larger than WStringBody making it safe to
  /// read beyond the one character defined in this class.
  struct WStringBody {
    WCHAR m_data[1];
  };
  /// The internal storage for the string
  struct WStringData : public WStringHeader, public WStringBody {};
#pragma pack(pop) // Restore default alignment

  static_assert(sizeof(WStringData) == 8 + 2,
                "WStringData should be 10 bytes with 1-byte alignment");

  /// A string's only member variable is a pointer directly to the Body of a
  /// WStringData
  WStringBody *m_kHandle;

public:
  /// Constructs an empty String.
  WString() : m_kHandle(nullwstr) {};
  /// Constructs String from another String. Increments RefCount only, no data
  /// is copied.
  WString(const WString &kStr);
  /// Constructs String from an input string. String is copied into internal
  /// buffer.
  WString(LPCWSTR pcStr);
  /// Constructs String from a part of input string. String is copied into
  /// internal buffer.
  WString(LPCWSTR pcStr, size_t stStrLength);
  /// Constructs an empty String, allocating given buffer size.
  // String(size_t stBuffLength); // disabled since our version does not
  // separate allocated buffer and string length
  /// Constructs String from a single char.
  WString(WCHAR ch);
  /// Destroys String. Refcount is decremented and if it equals zero, the buffer
  /// is deleted.
  ~WString() { DecRefCount(); };

  /// Ref-counted copy from another String
  inline WString &operator=(const WString &kStr);
  /// Copy string content from string
  inline WString &operator=(LPCWSTR pcStr);
  /// Set string content to single character
  inline WString &operator=(WCHAR ch);

public:
  static WStringBody *Allocate(size_t stCount);
  static WStringBody *AllocateAndCopy(LPCWSTR pcStr, size_t stCount = 0);
  static void Deallocate(WStringBody *&io_pBody);

  void IncRefCount();
  void DecRefCount();
  size_t GetRefCount() const;

  void Swap(LPCWSTR pcNewValue, size_t stLength = 0);
  LPCWSTR Reserve(int stLength);
  void Realloc(int stLength);

  LPWSTR GetString() const;
  size_t GetLength() const;
  void SetLength(size_t stLength);

  void SetBuffer(WStringBody *pBody);
  void CalcLength();

  static WStringHeader *__thiscall GetRealBufferStart(WStringBody *pBody);

  void CopyOnWrite();

  static WStringBody *nullwstr;
};
