
// Author: Joel Sikström

#ifndef TLSF_UTIL_HPP
#define TLSF_UTIL_HPP

#include <cstdlib>

class TLSFUtil {
public:
  static bool is_aligned(size_t size, size_t alignment);

  // Alignment only works with powers of two.
  static size_t align_up(size_t size, size_t alignment);
  static size_t align_down(size_t size, size_t alignment);
};

#endif // TLSF_UTIL_HPP
