
// Author: Joel Sikström

#ifndef TLSF_HPP
#define TLSF_HPP

#include <cstddef>
#include <cstdint>
#include <limits>

#define BLOCK_HEADER_LENGTH_SMALL offsetof(TLSFBlockHeader, next)
#define BLOCK_HEADER_LENGTH sizeof(TLSFBlockHeader)

struct TLSFMapping;

class TLSFBlockHeader {
private:
  static const size_t _BlockFreeMask = 1;
  static const size_t _BlockLastMask = 1 << 1;

public: 
  // size does not include header size, represents usable chunk of the block.
  size_t size;
  TLSFBlockHeader *next;
  TLSFBlockHeader *prev;
  TLSFBlockHeader *prev_phys_block;

  size_t get_size();

  bool is_free();
  bool is_last();

  void mark_free();
  void mark_used();

  void mark_last();
  void unmark_last();
};

template <typename Config>
class TLSFBase {
public:
  TLSFBase(uintptr_t initial_pool, size_t pool_size);

  // Calling this function will erase all metadata about allocated objects inside
  // the allocator, allowing their location in memory to be overriden by new
  // calls to allocate(). Use with caution.
  void clear(bool initial_block_allocated = false);

  void *allocate(size_t size);
  void free(void *address);

  size_t get_allocated_size(void *address);

  double header_overhead();

  // TODO: Should be removed. Used for debugging.
  void print_phys_blocks();
  void print_free_lists();
  void print_flatmap();

protected:
  TLSFBase() {}

  static const size_t _min_alloc_size = 16;
  static const size_t _min_alloc_size_log2 = 4;
  static const size_t _alignment = 8;

  static const size_t _fl_index = Config::FirstLevelIndex;
  static const size_t _sl_index_log2 = Config::SecondLevelIndexLog2;
  static const size_t _sl_index = (1 << _sl_index_log2);
  static const size_t _num_lists = _fl_index * _sl_index;
  static const size_t _mbs = Config::MBS;

  bool _deferred_coalescing;
  size_t _block_header_length;

  uintptr_t _block_start;
  size_t _pool_size;

  uint64_t _fl_bitmap;
  uint32_t _sl_bitmap[Config::UseSecondLevels ? _fl_index : 0];
  TLSFBlockHeader* _blocks[_num_lists];

  void initialize(uintptr_t initial_pool, size_t pool_size);

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

  TLSFBlockHeader *get_block_containing_address(uintptr_t address);

  // The following methods are calculated differently depending on the
  // configuration.
  size_t align_size(size_t size);
  TLSFMapping get_mapping(size_t size);
  uint32_t flatten_mapping(TLSFMapping mapping);
  TLSFMapping find_suitable_mapping(size_t target_size);
  void update_bitmap(TLSFMapping mapping, bool free_update);
};

class TLSFBaseConfig {
public:
  static const size_t FirstLevelIndex = 32;
  static const size_t SecondLevelIndexLog2 = 5;
  static const size_t MBS = 32;
  static const bool UseSecondLevels = true;
  static const bool DeferredCoalescing = false;
};

class TLSFZOptimizedConfig {
public:
  static const size_t FirstLevelIndex = 14;
  static const size_t SecondLevelIndexLog2 = 2;
  static const size_t MBS = 16;
  static const bool UseSecondLevels = false;
  static const bool DeferredCoalescing = true;
};

class TLSF : public TLSFBase<TLSFBaseConfig> {
public:
  TLSF(uintptr_t initial_pool, size_t pool_size)
    : TLSFBase(initial_pool, pool_size) {}

  static TLSF *create(uintptr_t initial_pool, size_t pool_size);
};

class ZPageOptimizedTLSF : public TLSFBase<TLSFZOptimizedConfig> {
public:
  ZPageOptimizedTLSF(uintptr_t initial_pool, size_t pool_size)
    : TLSFBase(initial_pool, pool_size) {}

  static ZPageOptimizedTLSF *create(uintptr_t initial_pool, size_t pool_size);

  // This assumes that the range that is described by (address -> (address + range))
  // contains one allocated block and no more.
  void free_range(void *address, size_t range);

  // Manually trigger block coalescing.
  void coalesce_blocks();
};

#endif // TLSF_HPP
