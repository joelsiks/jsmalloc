
// Author: Joel Sikström

#include <cmath>
#include <iostream>
#include <cassert>
#include <limits>

#include "JSMalloc.hpp"
#include "JSMallocUtil.inline.hpp"

template class JSMallocBase<BaseConfig>;
template class JSMallocBase<ZOptimizedConfig>;

// Contains first- and second-level index to segregated lists
// In the case of the optimized version, only the fl mapping is used.
struct Mapping { 
  static const uint32_t UNABLE_TO_FIND = std::numeric_limits<uint32_t>::max();
  size_t fl, sl;
};

size_t BlockHeader::get_size() {
  return size & ~(_BlockFreeMask | _BlockLastMask);
}

bool BlockHeader::is_free() {
  return (size & _BlockFreeMask) == _BlockFreeMask;
}

bool BlockHeader::is_last() {
  return (size & _BlockLastMask) == _BlockLastMask;
}

void BlockHeader::mark_free() {
    size |= _BlockFreeMask;
}

void BlockHeader::mark_used() {
    size &= ~_BlockFreeMask;
}

void BlockHeader::mark_last() {
  size |= _BlockLastMask;
}

void BlockHeader::unmark_last() {
  size &= ~_BlockLastMask;
}

template<typename Config>
JSMallocBase<Config>::JSMallocBase(void *pool, size_t pool_size, bool start_full) {
  initialize(pool, pool_size, start_full);
}

template<typename Config>
void JSMallocBase<Config>::reset(bool initial_block_allocated) {
  // Initialize bitmap and blocks
  _fl_bitmap = 0;
  for(size_t i = 0; i < _fl_index; i++) {
    if(Config::UseSecondLevels) {
      _sl_bitmap[i] = 0;
    }

    for(size_t j = 0; j < _sl_index; j++) {
      _blocks[i * _sl_index + j] = nullptr;
    }
  }
  _blocks[_num_lists] = nullptr;

  BlockHeader *blk = reinterpret_cast<BlockHeader *>(_block_start);
  blk->size = _pool_size - _block_header_length;
  if(!Config::DeferredCoalescing) {
    blk->prev_phys_block = nullptr;
  }

  if(!initial_block_allocated) {
    insert_block(blk);
  } else if(_block_header_length > 0) {
      blk->mark_used();
  }

  if(_block_header_length > 0) {
    blk->mark_last();
  }
}

template<typename Config>
void *JSMallocBase<Config>::allocate(size_t size) {
  BlockHeader *blk = find_block(size);

  if(blk == nullptr) {
    return nullptr;
  } 

  // Make sure addresses are aligned to the word-size (8-bytes).
  // TODO: This might not be necessary if everything is already aligned, and
  // should take into account that the block size might be smaller than expected.
  uintptr_t blk_start = (uintptr_t)blk + _block_header_length;
  return (void *)blk_start;
}

template<typename Config>
void JSMallocBase<Config>::print_blk(BlockHeader *blk) {
  std::cout << "Block (@ " << blk << ")\n" 
            << " size=" << blk->get_size() << "\n"
            << " LF=" << (blk->is_last() ? "1" : "0") << (blk->is_free() ? "1" : "0") << " (not accurate)\n";

  if(!Config::DeferredCoalescing) {
    std::cout << " phys_prev=" << ((blk->prev_phys_block == nullptr) ? 0 : blk->prev_phys_block) << "\n";
  }

  if(blk->is_free()) {
    std::cout << " next=" << blk_get_next(blk) << ","
              << " prev=" << blk_get_prev(blk)
              << std::endl;
  }
}

template<typename Config>
void JSMallocBase<Config>::print_phys_blks() {
  BlockHeader *current = reinterpret_cast<BlockHeader *>(_block_start);

  while(current != nullptr) {
    print_blk(current);
    current = get_next_phys_block(current);
  }
}

template<typename Config>
void JSMallocBase<Config>::print_free_lists() {
  for(size_t i = 0; i < 64; i++) {
    if((_fl_bitmap & (1UL << i)) == 0) {
      continue;
    }

    if(Config::UseSecondLevels) {
      for(size_t j = 0; j < 32; j++) {
        if((_sl_bitmap[i] & (1UL << j)) == 0) {
          continue;
        }

        printf("FREE-LIST (%02ld): ", i * _fl_index + j);
        BlockHeader *current = _blocks[flatten_mapping({i, j})];
        while(current != nullptr) {
          std::cout << current << " -> ";
          current = blk_get_next(current);
        }
        std::cout << "END" << std::endl;
      }

    } else {
      printf("FREE-LIST (%02ld): ", i);
      BlockHeader *current = _blocks[i];
      current = reinterpret_cast<BlockHeader *>(JSMallocUtil::from_offset(_block_start, false, reinterpret_cast<uint64_t>(_blocks[i].load())));

      while(current != nullptr) {
        std::cout << current << " -> ";
        current = blk_get_next(current);
      }

      std::cout << "END" << std::endl;
    }
  }
}

template<typename Config>
void JSMallocBase<Config>::initialize(void *pool, size_t pool_size, bool start_full) {
  uintptr_t aligned_initial_block = JSMallocUtil::align_up((uintptr_t)pool, _alignment);
  _block_start = aligned_initial_block;

  // The pool size is shrinked to the initial aligned block size. This wastes at maximum (_mbs - 1) bytes
  size_t aligned_block_size = JSMallocUtil::align_down(pool_size - (aligned_initial_block - (uintptr_t)pool), _mbs);
  _pool_size = aligned_block_size;

  reset(start_full);
}

template<typename Config>
void JSMallocBase<Config>::insert_block(BlockHeader *blk) {
  Mapping mapping = get_mapping(blk->get_size());
  uint32_t flat_mapping = flatten_mapping(mapping);

  _list_lock.lock();

  BlockHeader *head = _blocks[flat_mapping];

  // Insert the block into its corresponding free-list
  if(head != nullptr) {
    blk_set_prev(head, blk);
  }

  blk_set_next(blk, head);
  blk_set_prev(blk, nullptr);
  _blocks[flat_mapping] = blk;

  // Mark the block as free
  blk->mark_free();

  update_bitmap(mapping, true);

  _list_lock.unlock();
}

template<typename Config>
BlockHeader *JSMallocBase<Config>::find_block(size_t size) {
  size_t aligned_size = align_size(size);
  size_t target_size = aligned_size + (1UL << (JSMallocUtil::ilog2(aligned_size) - _sl_index_log2)) - 1;

  Mapping mapping = get_mapping(target_size);

  BlockHeader *blk = nullptr;
  while(blk == nullptr) {
    Mapping adjusted_mapping = adjust_available_mapping(mapping);

    if(adjusted_mapping.sl == Mapping::UNABLE_TO_FIND) {
      return nullptr;
    }

    blk = remove_block(nullptr, adjusted_mapping);
  }

  // If the block can be split, we split it in order to minimize internal fragmentation
  if((blk->get_size() - aligned_size) >= (_mbs + _block_header_length)) {
    BlockHeader *remainder_blk = split_block(blk, aligned_size);
    insert_block(remainder_blk);
  }

  return blk;
}

template<typename Config>
BlockHeader *JSMallocBase<Config>::coalesce_blocks(BlockHeader *blk1, BlockHeader *blk2) {
  size_t blk2_size = blk2->get_size();
  remove_block(blk1, get_mapping(blk1->get_size()));
  remove_block(blk2, get_mapping(blk2_size));

  bool blk2_is_last = blk2->is_last();

  // Combine the blocks by adding the size of blk2 to blk1 and also the block
  // header size
  blk1->size += _block_header_length + blk2_size;

  if(blk2_is_last) {
    blk1->mark_last();
  } else if(!Config::DeferredCoalescing) {
    // We only want to re-point the prev_phys_block ptr if we are not deferring
    // coalescing
    BlockHeader *next = get_next_phys_block(blk1);
    next->prev_phys_block = blk1;
  }

  return blk1;
}

template<typename Config>
BlockHeader *JSMallocBase<Config>::remove_block(BlockHeader *blk, Mapping mapping) {
  uint32_t flat_mapping = flatten_mapping(mapping);
  BlockHeader *target = blk;
  BlockHeader *next_blk, *prev_blk;

  _list_lock.lock();

  if(blk == nullptr) {
    target = _blocks[flat_mapping];
  }

  if(target == nullptr) {
    _list_lock.unlock();
    return nullptr;
  }

  next_blk = blk_get_next(target);
  prev_blk = blk_get_prev(target);

  // If the block is the head in the free-list, we need to update the head
  if(_blocks[flat_mapping] == target) {
    _blocks[flat_mapping] = next_blk;
  }

  if(next_blk != nullptr) {
    blk_set_prev(next_blk, prev_blk);
  } else {
    // If the block is the last one in the free-list, we mark it as empty
    update_bitmap(mapping, false);
  }

  if(prev_blk != nullptr) {
    blk_set_next(prev_blk, next_blk);
  }

  _list_lock.unlock();

  // Mark the block as used (no longer free)
  target->mark_used();

  return target;
}

template<typename Config>
BlockHeader *JSMallocBase<Config>::split_block(BlockHeader *blk, size_t size) {
  size_t remainder_size = blk->get_size() - _block_header_length - size;

  // Needs to be checked before setting new size
  bool is_last = blk->is_last();

  // Shrink blk to size
  blk->size = size;

  // Use a portion of blk's memory for the new block
  BlockHeader *remainder_blk = reinterpret_cast<BlockHeader *>((uintptr_t)blk + _block_header_length + blk->get_size());
  remainder_blk->size = remainder_size;
  if(!Config::DeferredCoalescing) {
    remainder_blk->prev_phys_block = blk;
  }

  if(is_last) {
    blk->unmark_last();
    remainder_blk->mark_last();
  } else if(!Config::DeferredCoalescing) {
    BlockHeader *next_phys = get_next_phys_block(remainder_blk);
    next_phys->prev_phys_block = remainder_blk;
  }

  return remainder_blk;
}

template<typename Config>
BlockHeader *JSMallocBase<Config>::get_next_phys_block(BlockHeader *blk) {
  if(blk == nullptr) {
    return nullptr;
  }

  uintptr_t next = (uintptr_t)blk + _block_header_length + blk->get_size();
  return ptr_in_pool(next)
    ? (BlockHeader *)next
    : nullptr;
}

template<typename Config>
BlockHeader *JSMallocBase<Config>::get_block_containing_address(uintptr_t address) {
  uintptr_t target_addr = (uintptr_t)address;
  BlockHeader *current = reinterpret_cast<BlockHeader *>(_block_start);

  while(current != nullptr) {
    uintptr_t start = (uintptr_t)current;
    uintptr_t end = start + _block_header_length + current->get_size();

    if(target_addr >= start && target_addr <= end) {
      return current;
    }

    current = get_next_phys_block(current);
  }

  return nullptr;
}

template<typename Config>
bool JSMallocBase<Config>::ptr_in_pool(uintptr_t ptr) {
  return ptr >= _block_start && ptr < (_block_start + _pool_size);
}

template<typename Config>
size_t JSMallocBase<Config>::align_size(size_t size) {
  if(size == 0) {
    size = 1;
  }

  return JSMallocUtil::align_up(size, _mbs);
}

template<>
BlockHeader *JSMallocBase<BaseConfig>::blk_get_next(BlockHeader *blk) {
  return reinterpret_cast<BlockHeader *>(blk->f1);
}

template<>
BlockHeader *JSMallocBase<BaseConfig>::blk_get_prev(BlockHeader *blk) {
  return reinterpret_cast<BlockHeader *>(blk->f2);
}

template<>
void JSMallocBase<BaseConfig>::blk_set_next(BlockHeader *blk, BlockHeader *next) {
  blk->f1 = reinterpret_cast<uint64_t>(next);
}

template<>
void JSMallocBase<BaseConfig>::blk_set_prev(BlockHeader *blk, BlockHeader *prev) {
  blk->f2 = reinterpret_cast<uint64_t>(prev);
}

template <>
Mapping JSMallocBase<BaseConfig>::get_mapping(size_t size) {
  uint32_t fl = JSMallocUtil::ilog2(size);
  uint32_t sl = (size >> (fl - _sl_index_log2)) ^ (1 << _sl_index_log2);
  return {fl, sl};
}

template <>
uint32_t JSMallocBase<BaseConfig>::flatten_mapping(Mapping mapping) {
  return mapping.fl * _sl_index + mapping.sl;
}

template <>
Mapping JSMallocBase<BaseConfig>::adjust_available_mapping(Mapping mapping) {
  // If the first-level index is out of bounds, the request cannot be fulfilled
  if(mapping.fl >= _fl_index) {
    return {0, Mapping::UNABLE_TO_FIND};
  }

  uint32_t sl_map = _sl_bitmap[mapping.fl] & (~0UL << mapping.sl);
  if(sl_map == 0) {
    // No suitable block exists in the second level. Search in the next largest
    // first-level instead
    uint32_t fl_map = _fl_bitmap & (~0UL << (mapping.fl + 1));
    if(fl_map == 0) {
      // No suitable block exists
      return {0, Mapping::UNABLE_TO_FIND};
    }

    mapping.fl = JSMallocUtil::ffs(fl_map);
    sl_map = _sl_bitmap[mapping.fl];
  }

  mapping.sl = JSMallocUtil::ffs(sl_map);

  return mapping;
}

template <>
void JSMallocBase<BaseConfig>::update_bitmap(Mapping mapping, bool free_update) {
  if(free_update) {
    _fl_bitmap |= (1 << mapping.fl);
    _sl_bitmap[mapping.fl] |= (1 << mapping.sl);
  } else {
    _sl_bitmap[mapping.fl] &= ~(1 << mapping.sl);
    if(_sl_bitmap[mapping.fl] == 0) {
      _fl_bitmap &= ~(1 << mapping.fl);
    }
  }
}

JSMalloc *JSMalloc::create(void *pool, size_t pool_size, bool start_full) {
  JSMalloc *jsmalloc = reinterpret_cast<JSMalloc *>(pool);
  return new(jsmalloc) JSMalloc(reinterpret_cast<void *>((uintptr_t)pool + sizeof(JSMalloc)), pool_size - sizeof(JSMalloc), start_full);
}

void JSMalloc::free(void *ptr) {
  if(ptr == nullptr) {
    return;
  }

  if(!ptr_in_pool((uintptr_t)ptr)) {
    return;
  }

  BlockHeader *blk = reinterpret_cast<BlockHeader *>((uintptr_t)ptr - _block_header_length);

  BlockHeader *prev_blk = blk->prev_phys_block;
  BlockHeader *next_blk = get_next_phys_block(blk);

  if(prev_blk != nullptr && prev_blk->is_free()) {
    blk = coalesce_blocks(prev_blk, blk);
  }

  if(next_blk != nullptr && next_blk->is_free()) {
    blk = coalesce_blocks(blk, next_blk);
  }

  insert_block(blk);
}

size_t JSMalloc::get_allocated_size(void *address) {
  BlockHeader *blk = reinterpret_cast<BlockHeader *>((uintptr_t)address - _block_header_length);
  return blk->get_size();
}

static uint32_t calculate_offset(BlockHeader *blk, uintptr_t start) {
  return (blk == nullptr) ? std::numeric_limits<uint32_t>::max() : reinterpret_cast<uintptr_t>(blk) - start;
}

template<>
BlockHeader *JSMallocBase<ZOptimizedConfig>::blk_get_next(BlockHeader *blk) {
  return reinterpret_cast<BlockHeader *>(JSMallocUtil::from_offset(_block_start, true, blk->f1));
}

template<>
BlockHeader *JSMallocBase<ZOptimizedConfig>::blk_get_prev(BlockHeader *blk) {
  return reinterpret_cast<BlockHeader *>(JSMallocUtil::from_offset(_block_start, false, blk->f1));
}

template<>
void JSMallocBase<ZOptimizedConfig>::blk_set_next(BlockHeader *blk, BlockHeader *next) {
  JSMallocUtil::set_offset(true, calculate_offset(next, _block_start), &blk->f1);
}

template<>
void JSMallocBase<ZOptimizedConfig>::blk_set_prev(BlockHeader *blk, BlockHeader *prev) {
  JSMallocUtil::set_offset(false, calculate_offset(prev, _block_start), &blk->f1);
}

template <>
Mapping JSMallocBase<ZOptimizedConfig>::get_mapping(size_t size) {
  int fl = JSMallocUtil::ilog2(size);
  int sl = size >> (fl - _sl_index_log2) ^ (1UL << _sl_index_log2);
  size_t mapping = ((fl - _min_alloc_size_log2) << _sl_index_log2) + sl;
  return {mapping > _num_lists ? _num_lists : mapping, 0};
}

template <>
uint32_t JSMallocBase<ZOptimizedConfig>::flatten_mapping(Mapping mapping) {
  return mapping.fl;
}

template <>
Mapping JSMallocBase<ZOptimizedConfig>::adjust_available_mapping(Mapping mapping) {
  // If the first-level index is out of bounds, the request cannot be fulfilled
  if(mapping.fl > _num_lists) {
    return {0, Mapping::UNABLE_TO_FIND};
  }

  uint64_t above_mapping = _fl_bitmap & (~0UL << mapping.fl);
  if(above_mapping == 0) {
    return {0, Mapping::UNABLE_TO_FIND};
  }

  mapping.fl = JSMallocUtil::ffs(above_mapping);

  return mapping;
}

template <>
void JSMallocBase<ZOptimizedConfig>::update_bitmap(Mapping mapping, bool free_update) {
  if(free_update) {
    _fl_bitmap |= (1UL << mapping.fl);
  } else {
    _fl_bitmap &= ~(1UL << mapping.fl);
  }
}

template<>
void JSMallocBase<ZOptimizedConfig>::insert_block(BlockHeader *blk) {
  Mapping mapping = get_mapping(blk->get_size());
  uint32_t flat_mapping = flatten_mapping(mapping);
  BlockHeader *head, *new_head;

  // Mark the block as free
  blk->mark_free();

  do {
    head = _blocks[flat_mapping].load();
    BlockHeader *offset = head;

    if(head == nullptr) {
      offset = reinterpret_cast<BlockHeader *>(std::numeric_limits<uint64_t>::max());
    }

    blk_set_next(blk, reinterpret_cast<BlockHeader *>(JSMallocUtil::from_offset(_block_start, false, reinterpret_cast<uint64_t>(offset))));

    uint64_t version = 1;
    new_head = reinterpret_cast<BlockHeader *>(version);
    JSMallocUtil::set_offset(false, calculate_offset(blk, _block_start), reinterpret_cast<uint64_t *>(&new_head));
  } while(!_blocks[flat_mapping].compare_exchange_strong(head, new_head));

  // Update bitmap to indicate level has a free block
  _fl_bitmap.fetch_or(1UL << mapping.fl);
}

template<>
BlockHeader *JSMallocBase<ZOptimizedConfig>::remove_block(BlockHeader *target_blk, Mapping mapping) {
  // blk should always be nullptr for the optimized allocator since we only
  // allow removing the head of the free-list, not a block in the middle
  (void)target_blk;

  uint32_t flat_mapping = flatten_mapping(mapping);
  BlockHeader *head = _blocks[flat_mapping].load();
  if(head == nullptr) {
    return nullptr;
  }

  uint64_t head_bits = reinterpret_cast<uint64_t>(head);
  uint32_t version = JSMallocUtil::get_bits(head_bits, true);
  BlockHeader *actual_head = reinterpret_cast<BlockHeader *>(JSMallocUtil::from_offset(_block_start, false, head_bits));

  BlockHeader *next_blk = (actual_head == nullptr)
    ? nullptr
    : blk_get_next(actual_head);

  BlockHeader *new_head = reinterpret_cast<BlockHeader *>(version + 1);
  JSMallocUtil::set_offset(false, calculate_offset(next_blk, _block_start), reinterpret_cast<uint64_t *>(&new_head));

  if(!_blocks[flat_mapping].compare_exchange_strong(head, new_head)) {
    return nullptr;
  }

  if(next_blk == nullptr) {
    _fl_bitmap.fetch_and(~(1UL << mapping.fl));
  }

  return actual_head;
}

JSMallocZ *JSMallocZ::create(void *pool, size_t pool_size, bool start_full) {
  JSMallocZ *jsmallocz = reinterpret_cast<JSMallocZ *>(pool);
  return new(jsmallocz) JSMallocZ(reinterpret_cast<void *>((uintptr_t)pool + sizeof(JSMallocZ)), pool_size - sizeof(JSMallocZ), start_full);
}

void JSMallocZ::free(void *ptr, size_t size) {
  if(ptr == nullptr) {
    return;
  }

  if(!ptr_in_pool((uintptr_t)ptr)) {
    return;
  }

  BlockHeader *blk = reinterpret_cast<BlockHeader *>(ptr);
  blk->size = size;
  insert_block(blk);
}

void JSMallocZ::free_range(void *start_ptr, size_t size) {
  free(start_ptr, size);
}

void JSMallocZ::aggregate() {
  BlockHeader *current_blk = reinterpret_cast<BlockHeader *>(_block_start);

  while(current_blk != nullptr) {
    BlockHeader *next_blk = get_next_phys_block(current_blk);

    if(next_blk != nullptr && current_blk->is_free() && next_blk->is_free()) {
      current_blk = JSMallocBase::coalesce_blocks(current_blk, next_blk);
      insert_block(current_blk);
    } else {
      current_blk = next_blk;
    }
  }
}
