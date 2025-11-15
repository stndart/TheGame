#include "console.h"

#include <WinSock2.h>
#include <cstdint>
#include <windows.h>

#include "game/engine/AtomicOperations.h"
#include "game/engine/MemoryDefines.h"
#include "game/engine/rcstring.h"

LPCSTR nullstr = *reinterpret_cast<const char **>(0x017B75E4);

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

void RefString::Truncate(BYTE *pBody, int maxLength) {
  logf("Truncate %p", pBody);

  int old_size; // ecx
  int new_size; // ecx

  LPCSTR v2 = *(const CHAR **)pBody;
  if (*(DWORD *)pBody) {
    if (v2 == nullstr)
      old_size = 0;
    else
      old_size = *((DWORD *)v2 - 2);
  } else {
    old_size = 0;
  }
  if ((maxLength <= 0 ? 0 : maxLength) >= old_size) {
    if (v2) {
      if (v2 == nullstr)
        new_size = 0;
      else
        new_size = *((DWORD *)v2 - 2);
    } else {
      new_size = 0;
    }
  } else {
    new_size = maxLength <= 0 ? 0 : maxLength;
  }
  if (v2 != nullstr && v2) {
    StringD *full = reinterpret_cast<StringD *>((int *)(v2 - 8));
    if (full) {
      full->cap = new_size;
      // *((BYTE *)full + 8 + new_size) = 0
      full->m_data[new_size] = 0;
    }
  }
}

void RefString::TruncateSelf(BYTE **pBody) {
  logf("TruncateSelf %p", pBody);

  LPSTR m_data = reinterpret_cast<LPSTR>(*pBody);
  if (m_data == nullptr)
    m_data = const_cast<LPSTR>(nullstr);

  int oldLength = strlen(m_data);
  Truncate(*pBody, oldLength);
}

inline RefString::StringHeader *
RefString::GetRealBufferStart(StringBody *pBody) {
  return (StringHeader *)((StringData *)pBody->m_data);
}

inline bool RefString::ValidateString(StringBody *pBody) {
  if (pBody->m_data == nullptr || pBody->m_data == nullstr)
    return true;

  size_t length = GetRealBufferStart(pBody)->m_cbBufferSize;

  if (length != strlen((const char *)pBody))
    return false;

  return true;
}