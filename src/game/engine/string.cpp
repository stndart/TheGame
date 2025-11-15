#include "console.h"

#include <WinSock2.h>
#include <basetsd.h>
#include <cstring>
#include <windows.h>
#include <winnt.h>

#include "game/engine/AtomicOperations.h"
#include "game/engine/MemoryDefines.h"
#include "game/engine/String.h"

String::StringBody *String::nullstr =
    reinterpret_cast<StringBody *>(0x017B75E4);

inline String::StringHeader *String::GetRealBufferStart(StringBody *pBody) {
  return reinterpret_cast<StringHeader *>(reinterpret_cast<uintptr_t>(pBody) -
                                          offsetof(StringData, m_data));
}

String::StringBody *String::Allocate(size_t stCount) {
  if (stCount == 0)
    stCount = 1;

  size_t stBufferSize = stCount + sizeof(StringData);
  StringData *pString =
      reinterpret_cast<StringData *>(EE_ALLOC(char, stBufferSize));
  if (!pString) {
    return nullptr;
  }

  pString->m_cbBufferSize = stBufferSize;
  pString->m_RefCount = 1;
  return (StringBody *)(pString->m_data);
}

String::StringBody *String::AllocateAndCopy(LPCSTR pcStr, size_t stCount) {
  if (pcStr == nullptr)
    return nullptr;

  if (stCount == 0)
    stCount = strlen(pcStr);

  StringBody *pBody = Allocate(stCount);
  if (pBody == nullptr)
    return nullptr;

  memcpy(pBody->m_data, pcStr, stCount);
  pBody->m_data[stCount] = '\0';

  return pBody;
}

inline void String::Deallocate(StringBody *&io_pBody) {
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

//--------------------------------------------------------------------------------------------------
inline String &String::operator=(LPCSTR pcStr) {
  if (pcStr == GetString())
    return *this;

  Swap(pcStr);
  return *this;
}

//--------------------------------------------------------------------------------------------------
inline String &String::operator=(char ch) {
  char acString[2];
  acString[0] = ch;
  acString[1] = '\0';
  return String::operator=((LPCSTR)&acString[0]);
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
  if (pcNewValue == nullptr) {
    DecRefCount();
    return;
  }

  if (stLength == 0) {
    stLength = strlen(pcNewValue);
  }

  if (m_kHandle == nullptr || m_kHandle == nullstr) {
    m_kHandle = AllocateAndCopy(pcNewValue, stLength);
    return;
  }

  StringHeader *header = GetRealBufferStart(m_kHandle);
  if (header->m_cbBufferSize < stLength) { // deallocation is inevitable
    DecRefCount();
    m_kHandle = AllocateAndCopy(pcNewValue, stLength);
    return;
  }

  size_t nRefCount = AtomicDecrement(header->m_RefCount);
  if (nRefCount == 0) { // means the only reference
    header->m_RefCount = 1;
    memcpy(m_kHandle->m_data, pcNewValue, stLength);
    m_kHandle->m_data[stLength] = '\0';
    return;
  }

  StringBody *body = (StringBody *)m_kHandle;
  Deallocate(body);
  m_kHandle = AllocateAndCopy(pcNewValue, stLength);
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

void String::CopyOnWrite() {}