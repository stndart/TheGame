#include "console.h"

#include <WinSock2.h>
#include <cstdint>
#include <minwindef.h>
#include <windows.h>

#include "game/engine/AtomicOperations.h"
#include "game/engine/MemoryDefines.h"
#include "game/engine/rcstring.h"

LPCSTR nullstr = *reinterpret_cast<const char **>(0x017B75E4);
RefString::StringBody *RefString::nullstr2 =
    reinterpret_cast<StringBody *>(0x017B75E4);

void RefString::Deallocate(StringBody *&io_pBody) {
  if (io_pBody->m_data != nullptr && io_pBody->m_data != nullstr) {
    StringHeader *pString = GetRealBufferStart(io_pBody);
    EE_FREE(pString);
    io_pBody = nullptr;
  }
}

void RefString::IncRefCount(StringBody *pBody, bool bValidate) {
  if (pBody->m_data == nullptr || pBody->m_data == nullstr)
    return;

  StringHeader *pString = GetRealBufferStart(pBody);
  AtomicIncrement(pString->m_RefCount);
}

void RefString::DecRefCount(StringBody *&io_pBody, bool bValidate) {
  if (io_pBody->m_data == nullptr || io_pBody->m_data == nullstr)
    return;

  StringHeader *pString = GetRealBufferStart(io_pBody);
  AtomicDecrement(pString->m_RefCount);

  if (pString->m_RefCount == 0) {
    Deallocate(io_pBody);
  }

  io_pBody = nullptr;
}

size_t RefString::GetRefCount(StringBody *pBody, bool bValidate) {
  if (pBody->m_data == nullptr || pBody->m_data == nullstr) {
    return 0;
  } else {
    StringHeader *pString = GetRealBufferStart(pBody);
    return pString->m_RefCount;
  }
}

inline LPSTR RefString::GetString(StringBody *pBody, bool bValidate) {
  // No need to perform an if NULL check, because
  // it will correctly return NULL if pBody == NULL
  return pBody->m_data;
}

// SURE? (m_cchStringLength in orig code)
inline size_t RefString::GetLength(StringBody *pBody, bool bValidate) {
  if (pBody->m_data == nullptr || pBody->m_data == nullstr) {
    return 0;
  } else {
    StringHeader *pHeader = GetRealBufferStart(pBody);
    return pHeader->m_cbBufferSize;
  }
}

inline void RefString::SetLength(StringBody *pBody, size_t stLength) {
  if (pBody->m_data == nullptr || pBody->m_data == nullstr)
    return;

  StringHeader *pHeader = GetRealBufferStart(pBody);
  pHeader->m_cbBufferSize = stLength;
}

struct StringD {
  int cap;
  size_t rc;
  char m_data[1];
};

void RefString::Truncate(int maxLength) {
  if (!m_kHandle || m_kHandle == nullstr2)
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

void __thiscall RefString::TruncateSelf(RefString **this_ptr) {
  RefString *refStringObj = *this_ptr;
  StringBody *body = refStringObj->m_kHandle;

  // Calculate the current length of the string
  size_t len = 0;
  if (refStringObj->m_kHandle && refStringObj->m_kHandle != nullstr2)
    len = strlen(refStringObj->m_kHandle->m_data);

  // Truncate to the current length (effectively just ensures proper null
  // termination)
  refStringObj->Truncate(len);
}

inline RefString::StringHeader *
RefString::GetRealBufferStart(StringBody *pBody) {
  return reinterpret_cast<StringHeader *>(reinterpret_cast<uintptr_t>(pBody) -
                                          offsetof(StringData, m_data));
}

inline bool RefString::ValidateString(StringBody *pBody) {
  if (pBody->m_data == nullptr || pBody->m_data == nullstr)
    return true;

  size_t length = GetRealBufferStart(pBody)->m_cbBufferSize;

  if (length != strlen((const char *)pBody))
    return false;

  return true;
}