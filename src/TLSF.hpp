
// Author: Joel Sikström

#ifndef TLSF_HPP
#define TLSF_HPP

#include <cstddef>
#include <cstdint>
#include <limits>

#define BLOCK_HEADER_LENGTH_SMALL offsetof(TLSFBlockHeader, next)
#define BLOCK_HEADER_LENGTH sizeof(TLSFBlockHeader)

class TLSFBlockHeader {
private:
  static const size_t _BlockFreeMask = 1;
  static const size_t _BlockLastMask = 1 << 1;

public:
  static const uint32_t NULL_OFFSET = std::numeric_limits<uint32_t>::max();

  // Size does not include header size and represents the usable chunk of the block.
  size_t size;

  // Indicates an offset from the start of the first block in the allocator.
  // next and prev are only used in free (unused) blocks
  uint32_t next;
  uint32_t prev;

  TLSFBlockHeader *prev_phys_block;

  uint32_t get_next(bool use_prev_phys);
  uint32_t get_prev(bool use_prev_phys);

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
  // Constructors
  TLSF(uintptr_t initial_pool, size_t pool_size, bool deferred_coalescing = false);
  static TLSF *create(uintptr_t initial_pool, size_t pool_size, bool deferred_coalescing = false);

  // Calling this function will erase all metadata about allocated objects inside
  // the allocator, allowing their location in memory to be overriden by new
  // calls to allocate(). Use with caution.
  void clear(bool initial_block_allocated = false);

  void *allocate(size_t size);
  void free(void *address);

  // This assumes that the range that is described by (address -> (address + range))
  // contains one allocated block and no more.
  void free_range(void *address, size_t range);

  // Manually trigger block coalescing. Usually only needed if allocator is
  // configured with deferred_coalescing.
  void coalesce_blocks();

  // TODO: Should be removed.
  void print_phys_blocks();
  void print_free_lists();
  void print_flatmap();

private:
  // TLSF Structure Parameters (FLI, SLI & MBS)
  static const size_t _min_alloc_size = 16;
  static const size_t _min_alloc_size_log2 = 4;

  static const size_t _alignment = 8;
  static const size_t _fl_index = 14;
  static const size_t _sl_index_log2 = 2;
  static const size_t _sl_index = (1 << _sl_index_log2);

  static const size_t _num_lists = _fl_index * _sl_index;

  bool _deferred_coalescing;
  size_t _block_header_length;
  size_t _mbs;

  uintptr_t _block_start;
  size_t _pool_size;

  uint64_t _flatmap;
  TLSFBlockHeader* _blocks[_num_lists];

  static uint32_t get_mapping(size_t size);

  void initialize(uintptr_t initial_pool, size_t pool_size, bool deferred_coalescing);

  // Used for converting to/from an offset for efficient storage in blocks.
  inline uint32_t block_offset(TLSFBlockHeader *blk);
  inline TLSFBlockHeader *block_address(uint32_t offset);

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
