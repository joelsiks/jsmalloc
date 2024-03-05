
// Author: Joel Sikström

#ifndef JSMALLOC_UTIL_INLINE_HPP
#define JSMALLOC_UTIL_INLINE_HPP

#include <climits>
#include <limits>

#include "JSMallocUtil.hpp"

inline uint32_t JSMallocUtil::get_bits(uint64_t value, bool lower) {
  return lower ? value & 0xFFFFFFFF : value >> 32;
}

inline void *JSMallocUtil::from_offset(uintptr_t base, bool lower, uint64_t value) {
  uint32_t offset = static_cast<uint32_t>(get_bits(value, lower));

  if(offset == std::numeric_limits<uint32_t>::max()) {
    return nullptr;
  } else {
    return reinterpret_cast<void *>(base + offset);
  }
}

inline void JSMallocUtil::set_offset(bool lower, uint32_t offset, uint64_t *value) {
  if(lower) {
    *value = (*value & 0xFFFFFFFF00000000) | offset;
  } else {
    uint64_t offset_large = static_cast<uint64_t>(offset);
    *value = (offset_large << 32) | (*value & 0xFFFFFFFF);
  }
}

inline uint64_t JSMallocUtil::combine_halfwords(uint32_t upper, uint32_t lower) {
  return (static_cast<uint64_t>(upper) << 32) | lower;
}

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
