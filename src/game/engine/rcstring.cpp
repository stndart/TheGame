// #include "crt/memory.h"
#include "console.h"

#include "game/engine/rcstring.h"

#define thisbase RefStringBase::from_refstring(this)

LPCSTR nullstr = *reinterpret_cast<const char **>(0x017B75E4);

inline int RefString::get_capacity() const {
  if (!str_p || str_p == nullstr)
    return 0;
  return thisbase->capacity;
}

// void RefString::truncate(int max_length) {
//   int old_size = get_capacity();
//   int new_size = max_length <= 0 ? 0 : max_length;
//   if (new_size >= old_size) {
//     new_size = old_size;
//   }
//   if (str_p && str_p != nullstr) {
//     if (thisbase) {
//       thisbase->capacity = new_size;
//       str_p[new_size] = 0;
//     }
//   }
// }

void __thiscall RefString::truncate(int max_length) {
  logf("RefString::truncate at %p", this);

  const CHAR *str_p; // eax
  int old_size;      // ecx
  int new_size;      // ecx
  RefStringBase *v5; // eax

  str_p = this->str_p;
  if (this->str_p) {
    if (str_p == nullstr)
      old_size = 0;
    else
      old_size = *((DWORD *)str_p - 2);
  } else {
    old_size = 0;
  }
  if ((max_length <= 0 ? 0 : max_length) >= old_size) {
    if (str_p) {
      if (str_p == nullstr)
        new_size = 0;
      else
        new_size = *((DWORD *)str_p - 2);
    } else {
      new_size = 0;
    }
  } else {
    new_size = max_length <= 0 ? 0 : max_length;
  }
  if (str_p != nullstr && str_p) {
    v5 = (RefStringBase *)(str_p - 8);
    if (v5) {
      v5->capacity = new_size;
      *((BYTE *)&v5->str.str_p + new_size) = 0;
    }
  }
}

/*
struct RefString {
  LPSTR str_p;

  RefString();                            // sub_9FCB50
  RefString(LPCSTR c_string);             // sub_CF0230
  RefString(RefString &other);            // sub_9FC6B0
  RefString &operator=(RefString &other); // sub_9FCAA0, weird

  LPSTR dec_ref(); // sub_9FCAF0, weird
  LPSTR inc_ref();
  LPSTR truncate(int max_length);   // sub_CEFCC0
  void realloc(int new_size);       // sub_CEFEF0
  LPSTR reserve(int required_size); // sub_CF0020

  inline int get_capacity() const; // sub_CEFB80 and inline
  inline size_t get_ref_cnt() const;
  LPCSTR get_str();       // sub_9FCD20
  LPCSTR get_str() const; // sub_CEFB70
};

struct RefStringBase {
  int str_size;
  size_t ref_cnt;
  RefString str;

  static inline RefStringBase *from_refstring(const RefString *rstr);
  inline const int get_capacity() const { return str_size; }
  inline void dec_ref() { InterlockedDecrement(&ref_cnt); }
  inline void inc_ref() { InterlockedIncrement(&ref_cnt); }

  RefStringBase(); // sub_CEFD20
};

*/

void log_string_structure(const RefString *str, const char *label) {
  if (str == nullptr) {
    logf("%s: null", label);
    return;
  }

  if (str->str_p == nullstr) {
    logf("%s: special empty string", label);
    return;
  }

  RefStringBase *header = RefStringBase::from_refstring(str);

  // int tt = 0;
  // if (header->capacity)
  //   tt = *reinterpret_cast<int *>(header->capacity);
  // int tt2 = 0;
  // if (header->ref_cnt)
  //   tt2 = *reinterpret_cast<int *>(header->ref_cnt);

  logf("%s: at %p, capacity=%d, refcount=%d, data='%s'", label, str,
       header->capacity, header->ref_cnt, str->str_p);
}

#undef thisbase
