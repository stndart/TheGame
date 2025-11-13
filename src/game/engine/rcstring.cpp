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

void RefString::truncate(int max_length) {
  int old_size = get_capacity();
  int new_size = max_length <= 0 ? 0 : max_length;
  if (new_size >= old_size) {
    new_size = old_size;
  }

  RefStringBase *base = thisbase;
  if (base) {
    base->capacity = new_size;
    str_p[new_size] = 0;
  }
}

void RefString::truncate_self() {
  if (!str_p)
    str_p = const_cast<LPSTR>(nullstr);
  truncate(strlen(str_p));
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
+  LPSTR truncate(int max_length);   // sub_CEFCC0
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
    logf("%s: NULL", label);
    return;
  }

  if (str->str_p == nullstr) {
    logf("%s: nullstr", label);
    return;
  }

  RefStringBase *header = RefStringBase::from_refstring(str);

  int capacity = 0;
  size_t ref_cnt = 0;
  if (header) {
    capacity = header->capacity;
    ref_cnt = header->ref_cnt;
  }

  logf("%s: at %p, capacity=%d, refcount=%d, data='%s'", label, str, capacity,
       ref_cnt, str->str_p);
}

#undef thisbase
