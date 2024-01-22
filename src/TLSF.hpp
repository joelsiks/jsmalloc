
// Author: Joel Sikström

#ifndef TLSF_HPP
#define TLSF_HPP

#include <cstddef>
#include <cstdint>

#define BLOCK_HEADER_LENGTH offsetof(TLSFBlockHeader, next)

struct TLSFMapping;

class TLSFBlockHeader {
private:
  static const size_t _BlockFreeMask = 0;
  static const size_t _BlockLastMask = 1;

public:
  size_t size;

  TLSFBlockHeader *prev_phys_block;

  // next and prev are only used in free (unused) blocks.
  TLSFBlockHeader *next;
  TLSFBlockHeader *prev;

  size_t get_size();

  bool is_free();
  bool is_last();

  void mark_free();
  void mark_used();

  void mark_last();
  void unmark_last();
};

class TLSF {
public:
  static TLSF *create(uintptr_t initial_pool, size_t pool_size);
  void destroy();

  void *allocate(size_t size);
  void free(void *address);

  // TODO: Should be removed.
  void print_phys_blocks();

private:
  // TLSF Structure Parameters (FLI, SLI & MBS)
  static const size_t _alignment = 8;
  static const size_t _fl_index = 32;
  static const size_t _sl_index_log2 = 4;
  static const size_t _sl_index = (1 << _sl_index_log2);
  static const size_t _mbs = 32;

  uintptr_t _mempool;
  size_t _pool_size;

  uint32_t _fl_bitmap;
  uint32_t _sl_bitmap[_fl_index];

  TLSFBlockHeader* _blocks[_fl_index][_sl_index_log2];

  static TLSFMapping get_mapping(size_t size);

  void insert_block(TLSFBlockHeader *blk);

  TLSFBlockHeader *find_block(size_t size);

  // Coalesces two blocks into one and returns a pointer to the coalesced block.
  TLSFBlockHeader *coalesce_blocks(TLSFBlockHeader *blk1, TLSFBlockHeader *blk2);

  // If blk is not nullptr, blk is removed, otherwise the head of the free-list
  // corresponding to mapping is removed.
  TLSFBlockHeader *remove_block(TLSFBlockHeader *blk, TLSFMapping mapping);

  // size is the number of bytes that should remain in blk. blk is shrinked to
  // size and a new block with the remaining blk->size - size is returned.
  TLSFBlockHeader *split_block(TLSFBlockHeader *blk, size_t size);

  TLSFBlockHeader *get_next_phys_block(TLSFBlockHeader *blk);
};

#endif // TLSF_HPP
