
// Author: Joel Sikström

#ifndef TLSF_HPP
#define TLSF_HPP

#include <cstddef>
#include <cstdint>

// TLSF Structure Parameters (FLI, SLI & MBS)
#define FL_INDEX       32
#define SL_INDEX_LOG2  4
#define SL_INDEX       (1 << SL_INDEX_LOG2)
#define MIN_BLOCK_SIZE 16

struct TLSFBlockHeader;

class TLSF {
public:
  TLSF *create(uintptr_t initial_pool, size_t pool_size);
  void destroy();

  void *allocate(size_t bytes);
  void free(void *address);

private:
  void insert_block(TLSFBlockHeader *blk);
  void find_block(size_t bytes);
  void coalesce_blocks(); // TODO

  uintptr_t _mempool;
  size_t _pool_size;

  uint32_t _fl_bitmap;
  uint32_t _sl_bitmap[FL_INDEX];

  TLSFBlockHeader* _blocks[FL_INDEX][SL_INDEX_LOG2];
};

#endif // TLSF_HPP
