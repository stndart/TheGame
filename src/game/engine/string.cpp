#include "console.h"

#include <WinSock2.h>
#include <cstdint>
#include <minwindef.h>
#include <windows.h>

#include "game/engine/AtomicOperations.h"
#include "game/engine/MemoryDefines.h"
#include "game/engine/String.h"

String::StringBody *String::nullstr =
    reinterpret_cast<StringBody *>(0x017B75E4);

void String::Deallocate(StringBody *&io_pBody) {
  if (io_pBody != nullptr && io_pBody != nullstr) {
    StringHeader *pString = GetRealBufferStart(io_pBody);
    EE_FREE(pString);
    io_pBody = nullptr;
  }
}

void String::IncRefCount(StringBody *pBody, bool bValidate) {
  if (pBody == nullptr || pBody == nullstr)
    return;

  StringHeader *pString = GetRealBufferStart(pBody);
  AtomicIncrement(pString->m_RefCount);
}

void String::DecRefCount(StringBody *&io_pBody, bool bValidate) {
  if (io_pBody == nullptr || io_pBody == nullstr)
    return;

  StringHeader *pString = GetRealBufferStart(io_pBody);
  AtomicDecrement(pString->m_RefCount);

  if (pString->m_RefCount == 0) {
    Deallocate(io_pBody);
  }

  io_pBody = nullptr;
}

size_t String::GetRefCount(StringBody *pBody, bool bValidate) {
  if (pBody == nullptr || pBody == nullstr) {
    return 0;
  } else {
    StringHeader *pString = GetRealBufferStart(pBody);
    return pString->m_RefCount;
  }
}

inline LPSTR String::GetString(StringBody *pBody, bool bValidate) {
  // No need to perform an if NULL check, because
  // it will correctly return NULL if pBody == NULL
  return pBody->m_data;
}

// SURE? (m_cchStringLength in orig code)
inline size_t String::GetLength(StringBody *pBody, bool bValidate) {
  if (pBody == nullptr || pBody == nullstr) {
    return 0;
  } else {
    StringHeader *pHeader = GetRealBufferStart(pBody);
    return pHeader->m_cbBufferSize;
  }
}

inline void String::SetLength(StringBody *pBody, size_t stLength) {
  if (pBody == nullptr || pBody == nullstr)
    return;

  StringHeader *pHeader = GetRealBufferStart(pBody);
  pHeader->m_cbBufferSize = stLength;
}

struct StringD {
  int cap;
  size_t rc;
  char m_data[1];
};

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

inline String::StringHeader *String::GetRealBufferStart(StringBody *pBody) {
  return reinterpret_cast<StringHeader *>(reinterpret_cast<uintptr_t>(pBody) -
                                          offsetof(StringData, m_data));
}

inline bool String::ValidateString(StringBody *pBody) {
  if (pBody == nullptr || pBody == nullstr)
    return true;

  size_t length = GetRealBufferStart(pBody)->m_cbBufferSize;

  if (length != strlen((const char *)pBody))
    return false;

  return true;
}