
// Author: Joel Sikström

#ifndef TLSF_HPP
#define TLSF_HPP

#include <cstddef>
#include <cstdint>

#define FL_INDEX      32
#define SL_INDEX_LOG2 5

struct TLSFBlockHeader;

class TLSF {
public:
  TLSF *create(uintptr_t mempool, size_t pool_size, size_t SLI, size_t MBS);
  void destroy();

  void *allocate(size_t bytes);
  void free(void *address);

private:
  uintptr_t _mempool;
  size_t _pool_size;

  uint32_t _fl_bitmap;
  uint32_t _sl_bitmap[FL_INDEX];

  TLSFBlockHeader* _blocks[FL_INDEX][SL_INDEX_LOG2];
};

#endif // TLSF_HPP
