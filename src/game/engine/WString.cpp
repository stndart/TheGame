// #include "console.h"
#include <cstring>
#include <windows.h>

#include <atlexcept.h>

#include "game/engine/AtomicOperations.h"
#include "game/engine/MemoryDefines.h"
#include "game/engine/WString.h"

#define ALLOC_LOG 0
#define RESERVE_LOG 0
#define CONCAT_LOG 0
#define FORMAT_LOG 0

WString::WStringBody *WString::nullwstr =
    *reinterpret_cast<WStringBody **>(0x017B75E8);

WString::WStringBody *WString::Allocate(size_t stCount) {
  if (ALLOC_LOG)
    logf("WString::Allocate %i", stCount);

  size_t stBufferSize = (stCount + 1) * sizeof(WCHAR) + sizeof(WStringHeader);

  WStringData *pString =
      reinterpret_cast<WStringData *> EE_HEAPALLOC(stBufferSize);
  if (!pString) {
    return nullptr;
  }

  pString->m_cbBufferSize = stCount;
  pString->m_RefCount = 1;
  return (WStringBody *)(pString->m_data);
}

WString::WStringBody *WString::AllocateAndCopy(LPCWSTR pcStr, size_t stCount) {
  if (ALLOC_LOG)
    logf("WString::AllocateAndCopy %i", stCount);

  if (pcStr == nullptr)
    return nullptr;

  if (stCount == 0)
    stCount = wcslen(pcStr);

  WStringBody *pBody = Allocate(stCount);
  if (pBody == nullptr)
    return nullptr;

  memcpy(pBody->m_data, pcStr, stCount * sizeof(WCHAR));
  pBody->m_data[stCount] = L'\0';
  GetRealBufferStart(pBody)->m_cbBufferSize = stCount;

  if (ALLOC_LOG)
    logf("WString::AllocateAndCopy: allocated at %p", pBody);

  return pBody;
}

inline void WString::Deallocate(WStringBody *&io_pBody) {
  if (ALLOC_LOG)
    logf("WString::Deallocate %p", io_pBody);

  if (io_pBody != nullptr && io_pBody != nullwstr) {
    WStringHeader *pString = GetRealBufferStart(io_pBody);
    EE_HEAPFREE(pString);
    io_pBody = nullptr;
  }
}

WString::WString(const WString &kStr) {
  m_kHandle = kStr.m_kHandle;
  IncRefCount();
}

WString::WString(LPCWSTR pcStr) {
  // This call also updates WString length
  m_kHandle = AllocateAndCopy(pcStr);
}

WString::WString(LPCWSTR pcStr, size_t stStrLength) {
  m_kHandle = AllocateAndCopy(pcStr, stStrLength);
  CalcLength();
}

/// Constructs WString from a single char.
WString::WString(WCHAR ch) {
  m_kHandle = Allocate(1);
  if (!m_kHandle)
    return;

  m_kHandle->m_data[0] = ch;
  m_kHandle->m_data[1] = L'\0';
  SetLength(1);
}

void WString::IncRefCount() {
  if (m_kHandle == nullptr || m_kHandle == nullwstr)
    return;

  WStringHeader *header = GetRealBufferStart(m_kHandle);
  AtomicIncrement(header->m_RefCount);
}

void WString::DecRefCount() {
  if (m_kHandle == nullptr) {
    m_kHandle = nullwstr;
    return;
  }

  WStringHeader *header = GetRealBufferStart(m_kHandle);
  if (header == nullptr) {
    m_kHandle = nullwstr;
    return;
  }
  if (!AtomicDecrement(header->m_RefCount)) {
    EE_HEAPFREE(header);
  }
  m_kHandle = nullwstr;
}

size_t WString::GetRefCount() const {
  if (m_kHandle == nullptr || m_kHandle == nullwstr) {
    return 0;
  }
  WStringHeader *header = GetRealBufferStart(m_kHandle);
  return header->m_RefCount;
}

void WString::Swap(LPCWSTR pcNewValue, size_t stLength) {
  logf("WString::Swap %i", stLength);

  if (pcNewValue == nullptr) {
    DecRefCount();
    return;
  }

  if (stLength == 0) {
    stLength = wcslen(pcNewValue);
  }

  Reserve(stLength);
  memcpy(m_kHandle->m_data, pcNewValue, stLength * sizeof(WCHAR));
  m_kHandle->m_data[stLength] = L'\0';
}

LPCWSTR WString::Reserve(int stLength) {
  if (RESERVE_LOG)
    logf("WString::Reserve %i for %p", stLength, this);

  size_t old_size = 0;
  if (m_kHandle != nullptr && m_kHandle != nullwstr) {
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
      logf("WString::Reserve - reserved at %p", &m_kHandle->m_data);
  }

  if (!m_kHandle)
    return nullwstr->m_data;
  return m_kHandle->m_data;
}

void WString::Realloc(int stLength) {
  if (RESERVE_LOG)
    logf("WString::Realloc %i for %p", stLength, this);

  if (!m_kHandle)
    m_kHandle = nullwstr;

  if (stLength < 0) {
    throw ATL::CAtlException(E_INVALIDARG);
  }

  if (stLength == 0) {
    DecRefCount();
    return;
  }

  WStringHeader *header = GetRealBufferStart(m_kHandle);
  if (stLength == header->m_cbBufferSize) {
    return;
  }

  m_kHandle = AllocateAndCopy(m_kHandle->m_data, stLength);
  m_kHandle->m_data[stLength] = L'\0';

  if (RESERVE_LOG) {
    logf("WString::Realloc - reserved at %p", &m_kHandle->m_data);
  }
}

void WString::CopyOnWrite() {
  if (ALLOC_LOG)
    logf("WString::CopyOnWrite");

  if (m_kHandle == nullptr || m_kHandle == nullwstr) {
    m_kHandle = Allocate(0);
    return;
  }

  if (GetRefCount() > 1) {
    LPCWSTR old_str = m_kHandle->m_data;
    DecRefCount();
    m_kHandle = AllocateAndCopy(old_str);
  }
}
