
// Author: Joel Sikström

#ifndef JSMALLOC_HPP
#define JSMALLOC_HPP

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <mutex>

class BlockHeader {
private:
  static const size_t _BlockFreeMask = 1;
  static const size_t _BlockLastMask = 1 << 1;

public: 
  // size does not include header size, represents usable chunk of the block.
  size_t size;
  uint64_t f1;
  uint64_t f2;
  BlockHeader *prev_phys_block;

  size_t get_size();

  bool is_free();
  bool is_last();

  void mark_free();
  void mark_used();

  void mark_last();
  void unmark_last();
};

struct Mapping;

constexpr size_t BLOCK_HEADER_LENGTH_SMALL = 0;
constexpr size_t BLOCK_HEADER_LENGTH = sizeof(BlockHeader);

template <typename Config>
class JSMallocBase {
private:
  size_t _internal_fragmentation = 0;
  size_t _allocated = 0;

public:
  JSMallocBase(void *pool, size_t pool_size, bool start_full);

  void reset(bool initial_block_allocated = true);
  void *allocate(size_t size);

  double internal_fragmentation();

  // TODO: Should be removed. Used for debugging.
  void print_phys_blks();
  void print_blk(BlockHeader *blk);
  void print_free_lists();

protected:
  JSMallocBase() {}

  static const size_t _min_alloc_size_log2 = 4;
  static const size_t _min_alloc_size = 2 << _min_alloc_size_log2;
  static const size_t _alignment = 8;

  static const size_t _fl_index = Config::FirstLevelIndex;
  static const size_t _sl_index_log2 = Config::SecondLevelIndexLog2;
  static const size_t _sl_index = (1 << _sl_index_log2);
  static const size_t _num_lists = _fl_index * _sl_index;
  static const size_t _mbs = Config::MBS;
  static const size_t _block_header_length = Config::BlockHeaderLength;

  uintptr_t _block_start;
  size_t _pool_size;

  std::atomic<uint64_t> _fl_bitmap;
  uint32_t _sl_bitmap[Config::UseSecondLevels ? _fl_index : 0];

  // We add an extra list for the optimized "large-list".
  std::atomic<BlockHeader*> _blocks[_num_lists + 1];

  std::mutex _list_lock;

  void initialize(void *pool, size_t pool_size, bool start_full);

  void insert_block(BlockHeader *blk);

  BlockHeader *find_block(size_t size);

  // Coalesces two blocks into one and returns a pointer to the coalesced block.
  BlockHeader *coalesce_blocks(BlockHeader *blk1, BlockHeader *blk2);

  // If blk is not nullptr, blk is removed, otherwise the head of the free-list
  // corresponding to mapping is removed.
  BlockHeader *remove_block(BlockHeader *blk, Mapping mapping);

  // size is the number of bytes that should remain in blk. blk is shrinked to
  // size and a new block with the remaining blk->size - size is returned.
  BlockHeader *split_block(BlockHeader *blk, size_t size);

  BlockHeader *get_next_phys_block(BlockHeader *blk);

  BlockHeader *get_block_containing_address(uintptr_t address);

  bool ptr_in_pool(uintptr_t ptr);

  size_t align_size(size_t size);

  // The following methods are calculated differently depending on the configuration.
  inline BlockHeader *blk_get_next(BlockHeader *blk);
  inline BlockHeader *blk_get_prev(BlockHeader *blk);
  inline void blk_set_next(BlockHeader *blk, BlockHeader *next);
  inline void blk_set_prev(BlockHeader *blk, BlockHeader *prev);

  Mapping get_mapping(size_t size);
  uint32_t flatten_mapping(Mapping mapping);
  Mapping adjust_available_mapping(Mapping mapping);
  void update_bitmap(Mapping mapping, bool free_update);
};

class BaseConfig {
public:
  static const size_t FirstLevelIndex = 32;
  static const size_t SecondLevelIndexLog2 = 5;
  static const size_t MBS = 32;
  static const bool UseSecondLevels = true;
  static const bool DeferredCoalescing = false;
  static const size_t BlockHeaderLength = BLOCK_HEADER_LENGTH;
};

class ZOptimizedConfig {
public:
  static const size_t FirstLevelIndex = 14;
  static const size_t SecondLevelIndexLog2 = 2;
  static const size_t MBS = 16;
  static const bool UseSecondLevels = false;
  static const bool DeferredCoalescing = true;
  static const size_t BlockHeaderLength = BLOCK_HEADER_LENGTH_SMALL;
};

class JSMalloc : public JSMallocBase<BaseConfig> {
public:
  JSMalloc(void *pool, size_t pool_size, bool start_full = false)
    : JSMallocBase(pool, pool_size, start_full) {}

  static JSMalloc *create(void *pool, size_t pool_size, bool start_full = false);

  void free(void *ptr);

  size_t get_allocated_size(void *address);
};

class JSMallocZ : public JSMallocBase<ZOptimizedConfig> {
public:
  JSMallocZ(void *pool, size_t pool_size, bool start_full)
    : JSMallocBase(pool, pool_size, start_full) {}

  static JSMallocZ *create(void *pool, size_t pool_size, bool start_full);

  void free(void *ptr, size_t size);

  // This assumes that the range that is described by (address -> (address + range))
  // contains one allocated block and no more.
  void free_range(void *start_ptr, size_t size);

  // Manually trigger block coalescing.
  void aggregate();
};

#endif // JSMALLOC_HPP
