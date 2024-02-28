
// Author: Joel Sikström

#ifndef JSMALLOC_UTIL_INLINE_HPP
#define JSMALLOC_UTIL_INLINE_HPP

#include <climits>

#include "JSMallocUtil.hpp"

inline bool JSMallocUtil::is_aligned(size_t size, size_t alignment) {
  return (size & (alignment - 1)) == 0;
}

inline size_t JSMallocUtil::align_up(size_t size, size_t alignment) {
  return (size + (alignment - 1)) & ~(alignment - 1);
}

inline size_t JSMallocUtil::align_down(size_t size, size_t alignment) {
  return size - (size & (alignment - 1));
}

inline size_t JSMallocUtil::ffs(size_t number) {
  return __builtin_ffsl(number) - 1;
}

inline size_t JSMallocUtil::fls(size_t number) {
  return sizeof(size_t) * CHAR_BIT - __builtin_clzl(number);
}

inline size_t JSMallocUtil::ilog2(size_t number) {
  return JSMallocUtil::fls(number) - 1;
}

#endif // JSMALLOC_UTIL_INLINE_HPP
