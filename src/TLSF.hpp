
// Author: Joel Sikström

#ifndef TLSF_HPP
#define TLSF_HPP

#include <cstddef>
#include <cstdint>

// TLSF Structure Parameters (FLI, SLI & MBS)
#define FL_INDEX       32
#define SL_INDEX_LOG2  4
#define SL_INDEX       (1 << SL_INDEX_LOG2)
#define MIN_BLOCK_SIZE 32

#define ALIGN_SIZE 8

struct TLSFBlockHeader;
struct TLSFMapping;

class TLSF {
public:
  static TLSF *create(uintptr_t initial_pool, size_t pool_size);
  void destroy();

  void *allocate(size_t size);
  void free(void *address);

  // TODO: Should be removed.
  void print_phys_blocks();

  // TODO: Should probably be private
  static TLSFMapping get_mapping(size_t size);
  TLSFBlockHeader *find_block(size_t size);
private:
  void insert_block(TLSFBlockHeader *blk);

  // Coalesces two blocks into one and returns a pointer to the coalesced block.
  TLSFBlockHeader *coalesce_blocks(TLSFBlockHeader *blk1, TLSFBlockHeader *blk2);

  // If blk is not nullptr, blk is removed, otherwise the head of the free-list
  // corresponding to mapping is removed.
  TLSFBlockHeader *remove_block(TLSFBlockHeader *blk, TLSFMapping mapping);

  // size is the number of bytes that should remain in blk. blk is shrinked to
  // size and a new block with the remaining blk->size - size is returned.
  TLSFBlockHeader *split_block(TLSFBlockHeader *blk, size_t size);

  TLSFBlockHeader *get_next_phys_block(TLSFBlockHeader *blk);

  uintptr_t _mempool;
  size_t _pool_size;

  uint32_t _fl_bitmap;
  uint32_t _sl_bitmap[FL_INDEX];

  TLSFBlockHeader* _blocks[FL_INDEX][SL_INDEX_LOG2];
};

#endif // TLSF_HPP
