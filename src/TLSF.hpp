
// Author: Joel Sikström

#ifndef TLSF_HPP
#define TLSF_HPP

#include <cstddef>
#include <cstdint>

#define BLOCK_HEADER_LENGTH offsetof(TLSFBlockHeader, next)

class TLSFBlockHeader {
private:
  static const size_t _BlockFreeMask = 1;
  static const size_t _BlockLastMask = 1 << 1;

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

  // Calling this function will erase all metadat about allocated objects inside
  // the allocator, allowing their location in memory to be overriden by new
  // calls to allocate(). Use with caution.
  void clear(bool initial_block_allocated = false);

  void *allocate(size_t size);
  void free(void *address);

  // This assumes that the range that is described by (address -> (address + range))
  // contains one (1) allocated block and no more.
  void free_range(void *address, size_t range);

  // TODO: Should be removed.
  void print_phys_blocks();
  void print_flatmap();

private:
  // TLSF Structure Parameters (FLI, SLI & MBS)
  static const size_t _min_alloc_size = 16;
  static const size_t _min_alloc_size_log2 = 4;
  static const size_t _max_alloc_size = 256 * 1024;

  static const size_t _alignment = 8;
  static const size_t _fl_index = 14;
  static const size_t _sl_index_log2 = 2;
  static const size_t _sl_index = (1 << _sl_index_log2);
  static const size_t _mbs = 32;

  static const size_t _num_lists = _fl_index * _sl_index;

  uintptr_t _block_start;
  size_t _pool_size;

  uint64_t _flatmap;
  TLSFBlockHeader* _blocks[_num_lists];

  static uint32_t get_mapping(size_t size);

  void insert_block(TLSFBlockHeader *blk);

  TLSFBlockHeader *find_block(size_t size);

  // Coalesces two blocks into one and returns a pointer to the coalesced block.
  TLSFBlockHeader *coalesce_blocks(TLSFBlockHeader *blk1, TLSFBlockHeader *blk2);

  // If blk is not nullptr, blk is removed, otherwise the head of the free-list
  // corresponding to mapping is removed.
  TLSFBlockHeader *remove_block(TLSFBlockHeader *blk, uint32_t mapping);

  // size is the number of bytes that should remain in blk. blk is shrinked to
  // size and a new block with the remaining blk->size - size is returned.
  TLSFBlockHeader *split_block(TLSFBlockHeader *blk, size_t size);

  TLSFBlockHeader *get_next_phys_block(TLSFBlockHeader *blk);

  TLSFBlockHeader *get_block_containing_address(uintptr_t address);
};

#endif // TLSF_HPP
