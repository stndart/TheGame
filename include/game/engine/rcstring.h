#pragma once

#include <windows.h>

extern LPCSTR nullstr;

struct RefString {
  LPSTR str_p;

  RefString();                            // sub_9FCB50
  RefString(LPCSTR c_string);             // sub_CF0230
  RefString(RefString &other);            // sub_9FC6B0
  RefString &operator=(RefString &other); // sub_9FCAA0, weird

  LPSTR dec_ref(); // sub_9FCAF0, weird
  LPSTR inc_ref();
  void truncate(int max_length);    // sub_CEFCC0
  void realloc(int new_size);       // sub_CEFEF0
  LPSTR reserve(int required_size); // sub_CF0020

  inline int get_capacity() const; // sub_CEFB80 and inline
  inline size_t get_ref_cnt() const;
  LPCSTR get_str();       // sub_9FCD20
  LPCSTR get_str() const; // sub_CEFB70
};

struct RefStringBase {
  int capacity;
  volatile size_t ref_cnt;
  RefString str;

  static inline RefStringBase *from_refstring(const RefString *rstr) {
    if (!rstr)
      return nullptr;
    LPCSTR str_p = rstr->str_p;
    if (!str_p || str_p == nullptr || str_p == nullstr)
      return nullptr;

    return reinterpret_cast<RefStringBase *>(
        reinterpret_cast<uintptr_t>(str_p) - 8);
  }

  inline void dec_ref() { InterlockedDecrement(&ref_cnt); }
  inline void inc_ref() { InterlockedIncrement(&ref_cnt); }

  RefStringBase(); // sub_CEFD20
};

void log_string_structure(const RefString *str, const char *label);
