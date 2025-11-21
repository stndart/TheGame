inline WString::WStringHeader *WString::GetRealBufferStart(WStringBody *pBody) {
  return reinterpret_cast<WStringHeader *>(reinterpret_cast<uintptr_t>(pBody) -
                                           offsetof(WStringData, m_data));
}

inline WString &WString::operator=(const WString &kStr) {
  if (kStr.GetString() == GetString())
    return *this;

  SetBuffer(kStr.m_kHandle);
  return *this;
}

inline WString &WString::operator=(LPCWSTR pcStr) {
  if (pcStr == GetString())
    return *this;

  Swap(pcStr);
  return *this;
}

inline WString &WString::operator=(WCHAR ch) {
  WCHAR acString[2] = {ch, L'\0'};
  return WString::operator=((LPCWSTR)&acString[0]);
}

inline LPWSTR WString::GetString() const {
  // No need to perform an if NULL check, because
  // it will correctly return NULL if m_kHandle == NULL
  return m_kHandle->m_data;
}

inline size_t WString::GetLength() const {
  if (m_kHandle == nullptr || m_kHandle == nullwstr) {
    return 0;
  }
  WStringHeader *header = GetRealBufferStart(m_kHandle);
  return header->m_cbBufferSize;
}

inline void WString::SetLength(size_t stLength) {
  if (m_kHandle == nullptr || m_kHandle == nullwstr)
    return;

  WStringHeader *header = GetRealBufferStart(m_kHandle);
  header->m_cbBufferSize = stLength;
}

inline void WString::SetBuffer(WStringBody *pBody) {
  if (pBody == m_kHandle)
    return;

  DecRefCount();
  m_kHandle = pBody;
  IncRefCount();
}

inline void WString::CalcLength() {
  if (m_kHandle == nullptr || m_kHandle == nullwstr)
    return;
  SetLength(wcslen(m_kHandle->m_data));
}