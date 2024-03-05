
// Author: Joel Sikström

#ifndef JSMALLOC_UTIL_HPP
#define JSMALLOC_UTIL_HPP

#include <cstdint>
#include <cstdlib>

class JSMallocUtil {
public:
  static uint32_t get_bits(uint64_t value, bool lower);
  static void *from_offset(uintptr_t base, bool lower, uint64_t value);
  static void set_offset(bool lower, uint32_t offset, uint64_t *value);

  static uint64_t combine_halfwords(uint32_t upper, uint32_t lower);

  static bool is_aligned(size_t size, size_t alignment);

  // Alignment only works with powers of two.
  static size_t align_up(size_t size, size_t alignment);
  static size_t align_down(size_t size, size_t alignment);

  // These instructions should not be called with argument 0 due to undefined
  // behaviour.
  static size_t ffs(size_t number);
  static size_t fls(size_t number);
  static size_t ilog2(size_t number);
};

#endif // JSMALLOC_UTIL_HPP
