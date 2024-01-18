
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

#define ALIGN_SIZE 8

struct TLSFBlockHeader;
struct TLSFMapping;

class TLSF {
public:
  static TLSF *create(uintptr_t initial_pool, size_t pool_size);
  void destroy();

  void *allocate(size_t size);
  void free(void *address);

  // TODO: Should probably be private
  static TLSFMapping get_mapping(size_t size);
private:
  void insert_block(TLSFBlockHeader *blk);

  // If blk is not nullptr, blk is removed, otherwise the head of the free-list
  // corresponding to mapping is removed.
  void *remove_block(TLSFBlockHeader *blk, TLSFMapping mapping);

  void *find_block(size_t size);
  void coalesce_blocks(); // TODO

  uintptr_t _mempool;
  size_t _pool_size;

  uint32_t _fl_bitmap;
  uint32_t _sl_bitmap[FL_INDEX];

  TLSFBlockHeader* _blocks[FL_INDEX][SL_INDEX_LOG2];
};

#endif // TLSF_HPP
