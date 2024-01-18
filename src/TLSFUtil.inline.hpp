
#ifndef TLSF_UTIL_INLINE_HPP
#define TLSF_UTIL_INLINE_HPP

#include "TLSFUtil.hpp"

inline bool TLSFUtil::is_aligned(size_t size, size_t alignment) {
    return (size & (alignment - 1)) == 0;
}

inline size_t TLSFUtil::align_up(size_t size, size_t alignment) {
    return (size + (alignment - 1)) & ~(alignment - 1);
}

inline size_t TLSFUtil::align_down(size_t size, size_t alignment) {
    return size - (size & (alignment - 1));
}

#endif // TLSF_UTIL_INLINE_HPP
