
// Author: Joel Sikström

#include <cmath>
#include <iostream>
#include <cassert>
#include <limits>

#include "TLSF.hpp"
#include "TLSFUtil.inline.hpp"

template class TLSFBase<TLSFBaseConfig>;
template class TLSFBase<TLSFZOptimizedConfig>;

// Contains first- and second-level index to segregated lists
// In the case of the optimized version, only the fl mapping is used.
struct TLSFMapping { 
  static const uint32_t UNABLE_TO_FIND = std::numeric_limits<uint32_t>::max();
  size_t fl, sl;
};

size_t TLSFBlockHeader::get_size() {
  return size & ~(_BlockFreeMask | _BlockLastMask);
}

bool TLSFBlockHeader::is_free() {
  return (size & _BlockFreeMask) == _BlockFreeMask;
}

bool TLSFBlockHeader::is_last() {
  return (size & _BlockLastMask) == _BlockLastMask;
}

void TLSFBlockHeader::mark_free() {
    size |= _BlockFreeMask;
}

void TLSFBlockHeader::mark_used() {
    size &= ~_BlockFreeMask;
}

void TLSFBlockHeader::mark_last() {
  size |= _BlockLastMask;
}

void TLSFBlockHeader::unmark_last() {
  size &= ~_BlockLastMask;
}

template<typename Config>
TLSFBase<Config>::TLSFBase(void *pool, size_t pool_size, allocation_size_func size_func, bool start_full) {
  _size_func = size_func;
  initialize(pool, pool_size, start_full);
}

template<typename Config>
void TLSFBase<Config>::reset(bool initial_block_allocated) {
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

  TLSFBlockHeader *blk = reinterpret_cast<TLSFBlockHeader *>(_block_start);
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
void *TLSFBase<Config>::allocate(size_t size) {
  TLSFBlockHeader *blk = find_block(size);

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
void TLSFBase<Config>::print_blk(TLSFBlockHeader *blk) {
  std::cout << "Block (@ " << blk << ")\n" 
            << " size=" << blk_get_size(blk) << "\n"
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
void TLSFBase<Config>::print_phys_blks() {
  TLSFBlockHeader *current = reinterpret_cast<TLSFBlockHeader *>(_block_start);

  while(current != nullptr) {
    print_blk(current);
    current = get_next_phys_block(current);
  }
}

template<typename Config>
void TLSFBase<Config>::print_free_lists() {
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
        TLSFBlockHeader *current = _blocks[flatten_mapping({i, j})];
        while(current != nullptr) {
          std::cout << current << " -> ";
          current = blk_get_next(current);
        }
        std::cout << "END" << std::endl;
      }

    } else {
      printf("FREE-LIST (%02ld): ", i);
      TLSFBlockHeader *current = _blocks[i];
      while(current != nullptr) {
        std::cout << current << " -> ";
        current = blk_get_next(current);
      }

      std::cout << "END" << std::endl;
    }
  }
}

template<typename Config>
void TLSFBase<Config>::initialize(void *pool, size_t pool_size, bool start_full) {
  uintptr_t aligned_initial_block = TLSFUtil::align_up((uintptr_t)pool, _alignment);
  _block_start = aligned_initial_block;

  // The pool size is shrinked to the initial aligned block size. This wastes
  // at maximum (_mbs - 1) bytes
  size_t aligned_block_size = TLSFUtil::align_down(pool_size - (aligned_initial_block - (uintptr_t)pool), _mbs);
  _pool_size = aligned_block_size;

  reset(start_full);
}

template<typename Config>
void TLSFBase<Config>::insert_block(TLSFBlockHeader *blk) {
  TLSFMapping mapping = get_mapping(blk_get_size(blk));
  uint32_t flat_mapping = flatten_mapping(mapping);

  TLSFBlockHeader *head = _blocks[flat_mapping];

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
}

template<typename Config>
TLSFBlockHeader *TLSFBase<Config>::find_block(size_t size) {
  size_t aligned_size = align_size(size);
  TLSFMapping mapping = find_suitable_mapping(aligned_size);

  if(mapping.sl == TLSFMapping::UNABLE_TO_FIND) {
    return nullptr;
  }

  // By now we now that we have an available block to use
  TLSFBlockHeader *blk = remove_block(nullptr, mapping);

  // If the block is larger than some threshold relative to the requested size
  // it should be split up to minimize internal fragmentation
  if((blk->get_size() - aligned_size) >= (_mbs + _block_header_length)) {
    TLSFBlockHeader *remainder_blk = split_block(blk, aligned_size);
    insert_block(remainder_blk);
  }

  return blk;
}

template<typename Config>
TLSFBlockHeader *TLSFBase<Config>::coalesce_blocks(TLSFBlockHeader *blk1, TLSFBlockHeader *blk2) {
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
    TLSFBlockHeader *next = get_next_phys_block(blk1);
    next->prev_phys_block = blk1;
  }

  return blk1;
}

template<typename Config>
TLSFBlockHeader *TLSFBase<Config>::remove_block(TLSFBlockHeader *blk, TLSFMapping mapping) {
  uint32_t flat_mapping = flatten_mapping(mapping);
  TLSFBlockHeader *target = blk;

  if(blk == nullptr) {
    target = _blocks[flat_mapping];
  }

  assert(target != nullptr);

  if(blk_get_next(target) != nullptr) {
    blk_set_prev(blk_get_next(target), blk_get_prev(target));
  }

  if(blk_get_prev(target) != nullptr) {
    blk_set_next(blk_get_prev(target), blk_get_next(target));
  }

  // Mark the block as used (no longer free)
  target->mark_used();

  // If the block is the head in the free-list, we need to update the head
  if(_blocks[flat_mapping] == target) {
    _blocks[flat_mapping] = blk_get_next(target);
  }

  // If the block is the last one in the free-list, we mark it as empty
  if(blk_get_next(target) == nullptr) {
    update_bitmap(mapping, false);
  }

  return target;
}

template<typename Config>
TLSFBlockHeader *TLSFBase<Config>::split_block(TLSFBlockHeader *blk, size_t size) {
  size_t remainder_size = blk_get_size(blk) - _block_header_length - size;

  // Needs to be checked before setting new size
  bool is_last = blk->is_last();

  // Shrink blk to size
  blk->size = size;

  // Use a portion of blk's memory for the new block
  TLSFBlockHeader *remainder_blk = reinterpret_cast<TLSFBlockHeader *>((uintptr_t)blk + _block_header_length + blk_get_size(blk));
  remainder_blk->size = remainder_size;
  if(!Config::DeferredCoalescing) {
    remainder_blk->prev_phys_block = blk;
  }

  if(is_last) {
    blk->unmark_last();
    remainder_blk->mark_last();
  } else if(!Config::DeferredCoalescing) {
    TLSFBlockHeader *next_phys = get_next_phys_block(remainder_blk);
    next_phys->prev_phys_block = remainder_blk;
  }

  return remainder_blk;
}

template<typename Config>
TLSFBlockHeader *TLSFBase<Config>::get_next_phys_block(TLSFBlockHeader *blk) {
  if(blk == nullptr) {
    return nullptr;
  }

  uintptr_t next = (uintptr_t)blk + _block_header_length + blk_get_size(blk);
  return ptr_in_pool(next)
    ? (TLSFBlockHeader *)next
    : nullptr;
}

template<typename Config>
TLSFBlockHeader *TLSFBase<Config>::get_block_containing_address(uintptr_t address) {
  uintptr_t target_addr = (uintptr_t)address;
  TLSFBlockHeader *current = reinterpret_cast<TLSFBlockHeader *>(_block_start);

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
bool TLSFBase<Config>::ptr_in_pool(uintptr_t ptr) {
  return ptr >= _block_start && ptr < (_block_start + _pool_size);
}

template<typename Config>
size_t TLSFBase<Config>::align_size(size_t size) {
  if(size == 0) {
    size = 1;
  }

  return TLSFUtil::align_up(size, _mbs);
}

template<>
size_t TLSFBase<TLSFBaseConfig>::blk_get_size(TLSFBlockHeader *blk) {
  return blk->get_size();
}

template<>
TLSFBlockHeader *TLSFBase<TLSFBaseConfig>::blk_get_next(TLSFBlockHeader *blk) {
  return reinterpret_cast<TLSFBlockHeader *>(blk->f1);
}

template<>
TLSFBlockHeader *TLSFBase<TLSFBaseConfig>::blk_get_prev(TLSFBlockHeader *blk) {
  return reinterpret_cast<TLSFBlockHeader *>(blk->f2);
}

template<>
void TLSFBase<TLSFBaseConfig>::blk_set_next(TLSFBlockHeader *blk, TLSFBlockHeader *next) {
  blk->f1 = reinterpret_cast<uint64_t>(next);
}

template<>
void TLSFBase<TLSFBaseConfig>::blk_set_prev(TLSFBlockHeader *blk, TLSFBlockHeader *prev) {
  blk->f2 = reinterpret_cast<uint64_t>(prev);
}

template <>
TLSFMapping TLSFBase<TLSFBaseConfig>::get_mapping(size_t size) {
  uint32_t fl = TLSFUtil::ilog2(size);
  uint32_t sl = (size >> (fl - _sl_index_log2)) ^ (1 << _sl_index_log2);
  return {fl, sl};
}

template <>
uint32_t TLSFBase<TLSFBaseConfig>::flatten_mapping(TLSFMapping mapping) {
  return mapping.fl * _sl_index + mapping.sl;
}

template <>
TLSFMapping TLSFBase<TLSFBaseConfig>::find_suitable_mapping(size_t aligned_size) {
  size_t target_size = aligned_size + (1UL << (TLSFUtil::ilog2(aligned_size) - _sl_index_log2)) - 1;

  // With the mapping we search for a free block
  TLSFMapping mapping = get_mapping(target_size);

  // If the first-level index is out of bounds, the request cannot be fulfilled
  if(mapping.fl >= _fl_index) {
    return {0, TLSFMapping::UNABLE_TO_FIND};
  }

  uint32_t sl_map = _sl_bitmap[mapping.fl] & (~0UL << mapping.sl);
  if(sl_map == 0) {
    // No suitable block exists in the second level. Search in the next largest
    // first-level instead
    uint32_t fl_map = _fl_bitmap & (~0UL << (mapping.fl + 1));
    if(fl_map == 0) {
      // No suitable block exists
      return {0, TLSFMapping::UNABLE_TO_FIND};
    }

    mapping.fl = TLSFUtil::ffs(fl_map);
    sl_map = _sl_bitmap[mapping.fl];
  }

  mapping.sl = TLSFUtil::ffs(sl_map);

  return mapping;
}

template <>
void TLSFBase<TLSFBaseConfig>::update_bitmap(TLSFMapping mapping, bool free_update) {
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

template<>
size_t TLSFBase<TLSFZOptimizedConfig>::blk_get_size(TLSFBlockHeader *blk) {
  size_t size = _size_func(reinterpret_cast<void *>(blk));

  if(size == 0) {
    return blk->get_size();
  } else {
    return size;
  }
}

template<>
TLSFBlockHeader *TLSFBase<TLSFZOptimizedConfig>::blk_get_next(TLSFBlockHeader *blk) {
  uintptr_t offset = static_cast<uint32_t>(blk->f1 >> 32);

  if(offset == std::numeric_limits<uint32_t>::max()) {
    return nullptr;
  } else {
    return reinterpret_cast<TLSFBlockHeader *>(_block_start + offset);
  }
}

template<>
TLSFBlockHeader *TLSFBase<TLSFZOptimizedConfig>::blk_get_prev(TLSFBlockHeader *blk) {
  uintptr_t offset = static_cast<uint32_t>(blk->f1 & 0xFFFFFFFF);

  if(offset == std::numeric_limits<uint32_t>::max()) {
    return nullptr;
  } else {
    return reinterpret_cast<TLSFBlockHeader *>(_block_start + offset);
  }
}

template<>
void TLSFBase<TLSFZOptimizedConfig>::blk_set_next(TLSFBlockHeader *blk, TLSFBlockHeader *next) {
  uintptr_t offset = std::numeric_limits<uint32_t>::max();

  if(next != nullptr) {
    offset = reinterpret_cast<uintptr_t>(next) - reinterpret_cast<uintptr_t>(_block_start);
  }
    
  blk->f1 = (static_cast<uint64_t>(offset) << 32) | (blk->f1 & 0xFFFFFFFF);
}

template<>
void TLSFBase<TLSFZOptimizedConfig>::blk_set_prev(TLSFBlockHeader *blk, TLSFBlockHeader *prev) {
  uintptr_t offset = std::numeric_limits<uint32_t>::max();

  if(prev != nullptr) {
    offset = reinterpret_cast<uintptr_t>(prev) - reinterpret_cast<uintptr_t>(_block_start);
  }

  blk->f1 = (blk->f1 & 0xFFFFFFFF00000000) | (static_cast<uint32_t>(offset) & 0xFFFFFFFF);
}

template <>
TLSFMapping TLSFBase<TLSFZOptimizedConfig>::get_mapping(size_t size) {
  int fl = TLSFUtil::ilog2(size);
  int sl = size >> (fl - _sl_index_log2) ^ (1UL << _sl_index_log2);
  size_t mapping = ((fl - _min_alloc_size_log2) << _sl_index_log2) + sl;
  return {mapping > _num_lists ? _num_lists : mapping, 0};
}

template <>
uint32_t TLSFBase<TLSFZOptimizedConfig>::flatten_mapping(TLSFMapping mapping) {
  return mapping.fl;
}

template <>
TLSFMapping TLSFBase<TLSFZOptimizedConfig>::find_suitable_mapping(size_t aligned_size) {
  if(aligned_size > (1UL << (_fl_index + 4))) {
    return {0, TLSFMapping::UNABLE_TO_FIND};
  }

  size_t target_size = aligned_size + (1UL << (TLSFUtil::ilog2(aligned_size) - _sl_index_log2)) - 1;

  // With the mapping we search for a free block
  TLSFMapping mapping = get_mapping(target_size);

  // If the first-level index is out of bounds, the request cannot be fulfilled
  if(mapping.fl > _num_lists) {
    return {0, TLSFMapping::UNABLE_TO_FIND};
  }

  uint64_t above_mapping = _fl_bitmap & (~0UL << mapping.fl);
  if(above_mapping == 0) {
    return {0, TLSFMapping::UNABLE_TO_FIND};
  }

  mapping.fl = TLSFUtil::ffs(above_mapping);

  return mapping;
}

template <>
void TLSFBase<TLSFZOptimizedConfig>::update_bitmap(TLSFMapping mapping, bool free_update) {
  if(free_update) {
    _fl_bitmap |= (1UL << mapping.fl);
  } else {
    _fl_bitmap &= ~(1UL << mapping.fl);
  }
}

TLSF *TLSF::create(void *pool, size_t pool_size, bool start_full) {
  TLSF *tlsf = reinterpret_cast<TLSF *>(pool);
  return new(tlsf) TLSF(reinterpret_cast<void *>((uintptr_t)pool + sizeof(TLSF)), pool_size - sizeof(TLSF), start_full);
}

void TLSF::free(void *ptr) {
  if(ptr == nullptr) {
    return;
  }

  if(!ptr_in_pool((uintptr_t)ptr)) {
    return;
  }

  TLSFBlockHeader *blk = reinterpret_cast<TLSFBlockHeader *>((uintptr_t)ptr - _block_header_length);

  TLSFBlockHeader *prev_blk = blk->prev_phys_block;
  TLSFBlockHeader *next_blk = get_next_phys_block(blk);

  if(prev_blk != nullptr && prev_blk->is_free()) {
    blk = coalesce_blocks(prev_blk, blk);
  }

  if(next_blk != nullptr && next_blk->is_free()) {
    blk = coalesce_blocks(blk, next_blk);
  }

  insert_block(blk);
}

size_t TLSF::get_allocated_size(void *address) {
  TLSFBlockHeader *blk = reinterpret_cast<TLSFBlockHeader *>((uintptr_t)address - _block_header_length);
  return blk->get_size();
}

ZPageOptimizedTLSF *ZPageOptimizedTLSF::create(void *pool, size_t pool_size, allocation_size_func size_func, bool start_full) {
  ZPageOptimizedTLSF *tlsf = reinterpret_cast<ZPageOptimizedTLSF *>(pool);
  return new(tlsf) ZPageOptimizedTLSF(reinterpret_cast<void *>((uintptr_t)pool + sizeof(ZPageOptimizedTLSF)), pool_size - sizeof(ZPageOptimizedTLSF), size_func, start_full);
}

void ZPageOptimizedTLSF::free(void *ptr) {
  if(ptr == nullptr) {
    return;
  }

  free(ptr, _size_func(ptr));
}

void ZPageOptimizedTLSF::free(void *ptr, size_t size) {
  if(ptr == nullptr) {
    return;
  }

  if(!ptr_in_pool((uintptr_t)ptr)) {
    return;
  }

  TLSFBlockHeader *blk = reinterpret_cast<TLSFBlockHeader *>(ptr);
  blk->size = size;

  insert_block(blk);
}

void ZPageOptimizedTLSF::free_range(void *start_ptr, size_t size) {
  TLSFBlockHeader *blk = (TLSFBlockHeader *)start_ptr;
  blk->size = size;
  insert_block(blk);
}

/*
void ZPageOptimizedTLSF::free_range(void *start_ptr, size_t size) {
  uintptr_t range_start = (uintptr_t)start_ptr;
  uintptr_t range_end = range_start + size;

  TLSFBlockHeader *blk = get_block_containing_address(range_start);
  uintptr_t blk_start = (uintptr_t)blk;
  uintptr_t blk_end = blk_start + blk->get_size();

  // If the range start and end are not in the same block, the user is calling
  // this function wrong and we return.
  if(blk != get_block_containing_address(range_end)) {
    return;
  }

  // Case 1: The range is inside the block but not touching any borders.
  if(range_start > blk_start && range_end < blk_end) {
    size_t left_size = range_start - blk_start;
    TLSFBlockHeader *free_blk = split_block(blk, left_size);
    split_block(free_blk, size);
    insert_block(free_blk);

  // Case 2: If the range is the entire block, we just free the block.
  } else if(range_start == blk_start && range_end == blk_end) {
    free(reinterpret_cast<TLSFBlockHeader *>(blk_start), _pool_size);

  // Case 3: The range is touching the block end.
  } else if(range_end == blk_end) {
    size_t split_size = range_start - blk_start;
    insert_block(split_block(blk, split_size));

  // Case 4: The range is touching the block start.
  } else if(range_start == blk_start) {
    size_t split_size = range_end - blk_start;
    split_block(blk, split_size);
    insert_block(blk);
  }
}
*/

void ZPageOptimizedTLSF::aggregate() {
  TLSFBlockHeader *current_blk = reinterpret_cast<TLSFBlockHeader *>(_block_start);

  while(current_blk != nullptr) {
    TLSFBlockHeader *next_blk = get_next_phys_block(current_blk);

    if(next_blk != nullptr && current_blk->is_free() && next_blk->is_free()) {
      current_blk = TLSFBase::coalesce_blocks(current_blk, next_blk);
      insert_block(current_blk);
    } else {
      current_blk = next_blk;
    }
  }
}

