// #include "console.h"
#include <cstring>
#include <windows.h>

#include <atlexcept.h>
#include <cstdint>
#include <mbstring.h>
#include <wingdi.h>
#include <winnt.h>

#include "game/engine/AtomicOperations.h"
#include "game/engine/MemoryDefines.h"
#include "game/engine/String.h"

#define ALLOC_LOG 0
#define RESERVE_LOG 0
#define CONCAT_LOG 0
#define FORMAT_LOG 1

String::StringBody *String::nullstr =
    *reinterpret_cast<StringBody **>(0x017B75E4);

inline String::StringHeader *String::GetRealBufferStart(StringBody *pBody) {
  return reinterpret_cast<StringHeader *>(reinterpret_cast<uintptr_t>(pBody) -
                                          offsetof(StringData, m_data));
}

String::StringBody *String::Allocate(size_t stCount) {
  if (ALLOC_LOG)
    logf("String::Allocate %i", stCount);

  if (stCount == 0)
    stCount = 1;

  size_t stBufferSize = stCount + sizeof(StringData);

  StringData *pString =
      reinterpret_cast<StringData *> EE_HEAPALLOC(stBufferSize);
  if (!pString) {
    return nullptr;
  }

  pString->m_cbBufferSize = stCount;
  pString->m_RefCount = 1;
  return (StringBody *)(pString->m_data);
}

String::StringBody *String::AllocateAndCopy(LPCSTR pcStr, size_t stCount) {
  if (ALLOC_LOG)
    logf("String::AllocateAndCopy %i", stCount);

  if (pcStr == nullptr)
    return nullptr;

  if (stCount == 0)
    stCount = strlen(pcStr);

  StringBody *pBody = Allocate(stCount);
  if (pBody == nullptr)
    return nullptr;

  memcpy(pBody->m_data, pcStr, stCount);
  pBody->m_data[stCount] = '\0';

  if (ALLOC_LOG)
    logf("String::AllocateAndCopy: allocated at %p", pBody);

  return pBody;
}

inline void String::Deallocate(StringBody *&io_pBody) {
  if (ALLOC_LOG)
    logf("String::Deallocate %p", io_pBody);

  if (io_pBody != nullptr && io_pBody != nullstr) {
    StringHeader *pString = GetRealBufferStart(io_pBody);
    EE_HEAPFREE(pString);
    io_pBody = nullptr;
  }
}

String::String(const String &kStr) {
  m_kHandle = kStr.m_kHandle;
  IncRefCount();
}

String::String(LPCSTR pcStr) {
  // This call also updates string length
  m_kHandle = AllocateAndCopy(pcStr);
}

String::String(LPCSTR pcStr, size_t stStrLength) {
  m_kHandle = AllocateAndCopy(pcStr, stStrLength);
  CalcLength();
}

/// Constructs String from a single char.
String::String(char ch) {
  m_kHandle = Allocate(1);
  if (!m_kHandle)
    return;

  m_kHandle->m_data[0] = ch;
  m_kHandle->m_data[1] = '\0';
  // char *pcString = (char *)m_kHandle;
  // pcString[0] = ch;
  // pcString[1] = '\0';
  SetLength(1);
}

inline String &String::operator=(const String &kStr) {
  if (kStr.GetString() == GetString())
    return *this;

  SetBuffer(kStr.m_kHandle);
  return *this;
}

inline String &String::operator=(LPCSTR pcStr) {
  if (pcStr == GetString())
    return *this;

  Swap(pcStr);
  return *this;
}

inline String &String::operator=(char ch) {
  char acString[2];
  acString[0] = ch;
  acString[1] = '\0';
  return String::operator=((LPCSTR)&acString[0]);
}

void String::TruncateAtFirstOccurrence(char ch) {
  if (CONCAT_LOG) {
    logf("String::TruncateAtFirstOccurrence");
    log_str(this, "TruncateAtFirstOccurrence");
  }

  if (m_kHandle == nullptr)
    m_kHandle = nullstr;

  if (m_kHandle == nullstr || ch == '\0')
    return;

  StringBody *found_sym = 0;
  StringBody *handle = m_kHandle;
  char first_sym = handle->m_data[0];
  while (handle->m_data[0]) {
    if (first_sym == ch) {
      found_sym = handle;
      break;
    }
    handle = reinterpret_cast<StringBody *>(
        _mbsinc(reinterpret_cast<const unsigned char *>(handle)));
    first_sym = handle->m_data[0];
  }
  if (found_sym) {
    handle = m_kHandle;
    int len = reinterpret_cast<uintptr_t>(found_sym) -
              reinterpret_cast<uintptr_t>(handle);
    Reserve(len); // does CopyOnWrite inside
    Truncate(len);
  }

  // more efficient implementation
  // bool found = false;
  // size_t i = 0;
  // const unsigned char *pcStr =
  //     reinterpret_cast<const unsigned char *>(GetString());
  // while (pcStr[0]) {
  //   if (pcStr[0] == ch) {
  //     found = true;
  //     break;
  //   }
  //   i++;
  //   pcStr = _mbsinc(pcStr);
  // }
  // if (found) {
  //   Reserve(i); // does CopyOnWrite inside
  //   Truncate(i);
  // }
}

void String::TrimLeft(char ch) {
  if (CONCAT_LOG) {
    logf("String::TrimLeft");
    log_str(this, "TrimLeft");
  }

  if (m_kHandle == nullptr)
    m_kHandle = nullstr;

  if (ch == '\0' || m_kHandle == NULL)
    return;

  size_t stTrimCount = 0;
  size_t stLength = GetLength();
  unsigned char *pcStr = reinterpret_cast<unsigned char *>(GetString());

  while (stTrimCount < stLength && pcStr[0] == ch) {
    stTrimCount++;
    pcStr = _mbsinc(pcStr);
  }

  if (stTrimCount > 0) {
    CopyOnWrite();
    // Update string pointer
    pcStr = reinterpret_cast<unsigned char *>(GetString());
    memmove(pcStr, pcStr + stTrimCount, stLength - stTrimCount + 1);
    Truncate(stLength - stTrimCount);
  }
}

void String::Concatenate(const String *pStr) {
  if (CONCAT_LOG) {
    logf("String::Concatenate(String*)");
    log_str(this, "Concatenate left");
    log_str(pStr, "Concatenate right");
  }

  if (pStr == nullptr)
    return;

  if (pStr->m_kHandle == nullptr || pStr->m_kHandle == nullstr)
    return;
  size_t otherLength = pStr->GetLength();
  if (otherLength == 0)
    return;

  size_t currentLength = GetLength();
  size_t newLength = currentLength + otherLength;
  Reserve(newLength);
  memcpy(&(m_kHandle->m_data[currentLength]), pStr->m_kHandle->m_data,
         otherLength);
  Truncate(newLength);
}

void String::Concatenate_cstr(LPCSTR pcStr) {
  if (CONCAT_LOG) {
    logf("String::Concatenate(LPCSTR)");
    log_str(this, "Concatenate left");
    logf("Concatenate right: %s", pcStr);
  }

  if (pcStr == nullptr)
    return;

  size_t otherLength = strlen(pcStr);
  size_t currentLength = GetLength();
  size_t newLength = currentLength + otherLength;

  Reserve(newLength);
  memcpy(&(m_kHandle->m_data[currentLength]), pcStr, otherLength);
  Truncate(newLength);
}

void String::IncRefCount() {
  if (m_kHandle == nullptr || m_kHandle == nullstr)
    return;

  StringHeader *header = GetRealBufferStart(m_kHandle);
  AtomicIncrement(header->m_RefCount);
}

void String::DecRefCount() {
  if (m_kHandle == nullptr) {
    m_kHandle = nullstr;
    return;
  }

  StringHeader *header = GetRealBufferStart(m_kHandle);
  if (header == nullptr) {
    m_kHandle = nullstr;
    return;
  }
  if (!AtomicDecrement(header->m_RefCount)) {
    EE_HEAPFREE(header);
  }
  m_kHandle = nullstr;
}

size_t String::GetRefCount() const {
  if (m_kHandle == nullptr || m_kHandle == nullstr) {
    return 0;
  }
  StringHeader *header = GetRealBufferStart(m_kHandle);
  return header->m_RefCount;
}

void String::Swap(LPCSTR pcNewValue, size_t stLength) {
  logf("String::Swap %i", stLength);

  if (pcNewValue == nullptr) {
    DecRefCount();
    return;
  }

  if (stLength == 0) {
    stLength = strlen(pcNewValue);
  }

  Reserve(stLength);
  memcpy(m_kHandle->m_data, pcNewValue, stLength);
  m_kHandle->m_data[stLength] = '\0';
}

LPCSTR String::Reserve(int stLength) {
  if (RESERVE_LOG)
    logf("String::Reserve %i for %p", stLength, this);

  size_t old_size = 0;
  if (m_kHandle != nullptr && m_kHandle != nullstr) {
    old_size = GetRealBufferStart(m_kHandle)->m_cbBufferSize;
  }

  if (stLength <= 1) {
    stLength = 1;
  }

  if (old_size < stLength) { // not enough allocated memory
    Realloc(stLength);
  }
  CopyOnWrite();

  if (RESERVE_LOG) {
    if (m_kHandle)
      logf("String::Reserve - reserved at %p", &m_kHandle->m_data);
  }

  if (!m_kHandle)
    return nullstr->m_data;
  return m_kHandle->m_data;
}

void String::Realloc(int stLength) {
  if (RESERVE_LOG)
    logf("String::Realloc %i for %p", stLength, this);

  if (!m_kHandle)
    m_kHandle = nullstr;

  if (stLength < 0) {
    throw ATL::CAtlException(E_INVALIDARG);
  }

  if (stLength == 0) {
    DecRefCount();
    return;
  }

  StringHeader *header = GetRealBufferStart(m_kHandle);
  if (stLength == header->m_cbBufferSize) {
    return;
  }

  m_kHandle = AllocateAndCopy(m_kHandle->m_data, stLength);
  m_kHandle->m_data[stLength] = '\0';

  if (RESERVE_LOG) {
    logf("String::Realloc - reserved at %p", &m_kHandle->m_data);
  }
}

inline LPSTR String::GetString() const {
  // No need to perform an if NULL check, because
  // it will correctly return NULL if m_kHandle == NULL
  return m_kHandle->m_data;
}

// SURE? (m_cchStringLength in orig code)
inline size_t String::GetLength() const {
  if (m_kHandle == nullptr || m_kHandle == nullstr) {
    return 0;
  }
  StringHeader *header = GetRealBufferStart(m_kHandle);
  return header->m_cbBufferSize;
}

inline void String::SetLength(size_t stLength) {
  if (m_kHandle == nullptr || m_kHandle == nullstr)
    return;

  StringHeader *header = GetRealBufferStart(m_kHandle);
  header->m_cbBufferSize = stLength;
}

inline void String::SetBuffer(StringBody *pBody) {
  if (pBody == m_kHandle)
    return;

  DecRefCount();
  m_kHandle = pBody;
  IncRefCount();
}

inline void String::CalcLength() {
  if (m_kHandle == nullptr || m_kHandle == nullstr)
    return;
  SetLength(strlen(m_kHandle->m_data));
}

void String::Truncate(int maxLength) {
  if (RESERVE_LOG)
    logf("String::Truncate %i", maxLength);

  if (!m_kHandle || m_kHandle == nullstr)
    return;

  StringHeader *header = GetRealBufferStart(m_kHandle);

  int old_size = header->m_cbBufferSize;
  int new_size = maxLength <= 0 ? 0 : maxLength;
  if (new_size >= old_size)
    new_size = old_size;

  // logf("[x] Original size %i, new %i, maxlength %i, v2 %p, v5 %p", old_size,
  //      new_size, maxLength, m_kHandle, header);

  if (header) {
    header->m_cbBufferSize = new_size;
    m_kHandle->m_data[new_size] = '\0';
  }
}

void __thiscall String::TruncateSelf(String **this_ptr) {
  String *StringObj = *this_ptr;
  StringBody *body = StringObj->m_kHandle;

  // Calculate the current length of the string
  size_t len = 0;
  if (StringObj->m_kHandle && StringObj->m_kHandle != nullstr)
    len = strlen(StringObj->m_kHandle->m_data);

  // Truncate to the current length (effectively just ensures proper null
  // termination)
  StringObj->Truncate(len);
}

inline bool String::ValidateString(StringBody *pBody) {
  if (pBody == nullptr || pBody == nullstr)
    return true;

  size_t length = GetRealBufferStart(pBody)->m_cbBufferSize;

  if (length != strlen((const char *)pBody))
    return false;
  return true;
}

void String::CopyOnWrite() {
  if (ALLOC_LOG)
    logf("String::CopyOnWrite");

  if (m_kHandle == nullptr || m_kHandle == nullstr) {
    m_kHandle = Allocate(0);
    return;
  }

  if (GetRefCount() > 1) {
    LPCSTR old_str = m_kHandle->m_data;
    DecRefCount();
    m_kHandle = AllocateAndCopy(old_str);
  }
}

void String::vformat(String *pStr, LPCSTR pcFormat, va_list *argPtr) {
  if (FORMAT_LOG)
    logf("vformat of format %s", pcFormat);

  pStr->Vformat(pcFormat, *argPtr); // TODO: check argPtr typing
}

void String::Vformat(LPCSTR pcFormat, va_list argPtr) {
  if (FORMAT_LOG)
    logf("String::Vformat of format %s to %p", pcFormat, this);

  if (!pcFormat) {
    throw ATL::CAtlException(E_INVALIDARG);
  }

  char buffer[1024];
  int written = vsprintf_s(buffer, 1024, pcFormat, argPtr);

  LPCSTR new_buf = Reserve(written);
  strcpy(const_cast<LPSTR>(new_buf), buffer);
  int new_len = strlen(m_kHandle->m_data);
  Truncate(new_len);
}