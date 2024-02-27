
// Author: Joel Sikström

#ifndef TLSF_HPP
#define TLSF_HPP

#include <cstddef>
#include <cstdint>
#include <limits>

class TLSFBlockHeader {
private:
  static const size_t _BlockFreeMask = 1;
  static const size_t _BlockLastMask = 1 << 1;

public: 
  // size does not include header size, represents usable chunk of the block.
  size_t size;
  uint64_t f1;
  uint64_t f2;
  TLSFBlockHeader *prev_phys_block;

  size_t get_size();

  bool is_free();
  bool is_last();

  void mark_free();
  void mark_used();

  void mark_last();
  void unmark_last();
};

struct TLSFMapping;

constexpr size_t BLOCK_HEADER_LENGTH_SMALL = 0;
constexpr size_t BLOCK_HEADER_LENGTH = sizeof(TLSFBlockHeader);

typedef size_t(* allocation_size_func)(void *address);

static size_t default_allocation_size(void *address) {
  return reinterpret_cast<TLSFBlockHeader *>((uintptr_t)address - BLOCK_HEADER_LENGTH)->size;
}

template <typename Config>
class TLSFBase {
public:
  TLSFBase(void *pool, size_t pool_size, allocation_size_func size_func, bool start_full);

  void reset(bool initial_block_allocated = true);
  void *allocate(size_t size);

  // TODO: Should be removed. Used for debugging.
  void print_phys_blks();
  void print_blk(TLSFBlockHeader *blk);
  void print_free_lists();

protected:
  TLSFBase() {}

  static const size_t _min_alloc_size_log2 = 4;
  static const size_t _min_alloc_size = 2 << _min_alloc_size_log2;
  static const size_t _alignment = 8;

  static const size_t _fl_index = Config::FirstLevelIndex;
  static const size_t _sl_index_log2 = Config::SecondLevelIndexLog2;
  static const size_t _sl_index = (1 << _sl_index_log2);
  static const size_t _num_lists = _fl_index * _sl_index;
  static const size_t _mbs = Config::MBS;
  static const size_t _block_header_length = Config::BlockHeaderLength;

  allocation_size_func _size_func;

  uintptr_t _block_start;
  size_t _pool_size;

  uint64_t _fl_bitmap;
  uint32_t _sl_bitmap[Config::UseSecondLevels ? _fl_index : 0];

  // We add an extra list for the optimized "large-list".
  TLSFBlockHeader* _blocks[_num_lists + 1];

  void initialize(void *pool, size_t pool_size, bool start_full);

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

  bool ptr_in_pool(uintptr_t ptr);

  size_t align_size(size_t size);

  // The following methods are calculated differently depending on the configuration.
  inline size_t blk_get_size(TLSFBlockHeader *blk);
  inline TLSFBlockHeader *blk_get_next(TLSFBlockHeader *blk);
  inline TLSFBlockHeader *blk_get_prev(TLSFBlockHeader *blk);
  inline void blk_set_next(TLSFBlockHeader *blk, TLSFBlockHeader *next);
  inline void blk_set_prev(TLSFBlockHeader *blk, TLSFBlockHeader *prev);

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
  static const size_t BlockHeaderLength = BLOCK_HEADER_LENGTH;
};

class TLSFZOptimizedConfig {
public:
  static const size_t FirstLevelIndex = 14;
  static const size_t SecondLevelIndexLog2 = 2;
  static const size_t MBS = 16;
  static const bool UseSecondLevels = false;
  static const bool DeferredCoalescing = true;
  static const size_t BlockHeaderLength = BLOCK_HEADER_LENGTH_SMALL;
};

class TLSF : public TLSFBase<TLSFBaseConfig> {
public:
  TLSF(void *pool, size_t pool_size, bool start_full = false)
    : TLSFBase(pool, pool_size, default_allocation_size, start_full) {}

  static TLSF *create(void *pool, size_t pool_size, bool start_full = false);

  void free(void *ptr);

  size_t get_allocated_size(void *address);
};

class ZPageOptimizedTLSF : public TLSFBase<TLSFZOptimizedConfig> {
public:
  ZPageOptimizedTLSF(void *pool, size_t pool_size, allocation_size_func size_func, bool start_full)
    : TLSFBase(pool, pool_size, size_func, start_full) {}

  static ZPageOptimizedTLSF *create(void *pool, size_t pool_size, allocation_size_func size_func, bool start_full);

  void free(void *ptr);
  void free(void *ptr, size_t size);

  // This assumes that the range that is described by (address -> (address + range))
  // contains one allocated block and no more.
  void free_range(void *start_ptr, size_t size);

  // Manually trigger block coalescing.
  void aggregate();
};

#endif // TLSF_HPP
