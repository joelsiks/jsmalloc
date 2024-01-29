
// Author: Joel Sikström

#include <cmath>
#include <iostream>
#include <cassert>

#include "TLSF.hpp"
#include "TLSFUtil.inline.hpp"

#ifndef NODEBUG
static void print_blk(TLSFBlockHeader *blk) {
  std::cout << "Block (@ " << blk << ")\n" 
            << " size=" << blk->get_size() << "\n"
            << " prev=" << ((blk->prev_phys_block == nullptr) ? 0 : blk->prev_phys_block) << "\n"
            << " LF=" << (blk->is_last() ? "1" : "0") << (blk->is_free() ? "1" : "0") << "\n";

  if(blk->is_free()) {
    std::cout << " next=" << ((blk->next == TLSFBlockHeader::NULL_OFFSET) ? "NULL" : std::to_string(blk->next))
              << " prev=" << ((blk->prev == TLSFBlockHeader::NULL_OFFSET) ? "NULL" : std::to_string(blk->prev))
              << std::endl;
  }
}

static void print_binary(uint32_t value) {
  for (int i = 31; i >= 0; --i) {
    std::cout << ((value >> i) & 1);
    if (i % 4 == 0) std::cout << ' ';
  }
  std::cout << std::endl;
}

static void print_binary(uint64_t value) {
  for (int i = 63; i >= 0; --i) {
    std::cout << ((value >> i) & 1);
    if (i % 4 == 0) std::cout << ' ';
  }
  std::cout << std::endl;
}
#endif

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

TLSF::TLSF(uintptr_t initial_pool, size_t pool_size, bool deferred_coalescing) {
  initialize(initial_pool, pool_size, deferred_coalescing);
}

TLSF *TLSF::create(uintptr_t initial_pool, size_t pool_size, bool deferred_coalescing) {
  TLSF *tlsf = reinterpret_cast<TLSF *>(initial_pool);
  tlsf->initialize(initial_pool + sizeof(TLSF), pool_size - sizeof(TLSF), deferred_coalescing);
  return tlsf;
}

void TLSF::clear(bool initial_block_allocated) {
  // Initialize bitmap and blocks
  _flatmap = 0;
  for(int i = 0; i < _num_lists; i++) {
    _blocks[i] = nullptr;
  }

  TLSFBlockHeader *blk = reinterpret_cast<TLSFBlockHeader *>(_block_start);

  blk->size = _pool_size - _block_header_length;
  blk->prev_phys_block = nullptr;

  // If the initial block is not allocated it should be inserted into its
  // corresponding free list.
  if(!initial_block_allocated) {
    insert_block(blk);
  } else {
    blk->mark_used();
  }
}

void *TLSF::allocate(size_t size) {
  TLSFBlockHeader *blk = find_block(size);

  if(blk == nullptr) {
    return nullptr;
  } 

  // Make sure addresses are aligned to the word-size (8-bytes).
  // TODO: This might not be necessary if everything is already aligned, and
  // should take into account that the block size might be smaller than expected.
  uintptr_t blk_start = (uintptr_t)blk + _block_header_length;
  return (void *)TLSFUtil::align_up(blk_start, _alignment);
}

void TLSF::free(void *address) {
  TLSFBlockHeader *blk = reinterpret_cast<TLSFBlockHeader *>((uintptr_t)address - _block_header_length);

  // Try to merge with adjacent blocks
  if(!_deferred_coalescing) {
    TLSFBlockHeader *prev_blk = blk->prev_phys_block;
    TLSFBlockHeader *next_blk = get_next_phys_block(blk);

    if(prev_blk != nullptr && prev_blk->is_free()) {
      blk = coalesce_blocks(prev_blk, blk);
    }

    if(next_blk != nullptr && next_blk->is_free()) {
      blk = coalesce_blocks(blk, next_blk);
    }
  }

  insert_block(blk);
}

void TLSF::free_range(void *address, size_t range) {
  uintptr_t range_start = (uintptr_t)address;
  uintptr_t range_end = range_start + range;

  TLSFBlockHeader *blk = get_block_containing_address(range_start);
  uintptr_t blk_start = (uintptr_t)blk;
  uintptr_t blk_end = blk_start + _block_header_length + blk->get_size();

  // If the range start and end are not in the same block, the user is calling
  // this function wrong and we return.
  if(blk != get_block_containing_address(range_end)) {
    return;
  }

  // Case 1: The range is inside the block but not touching any borders.
  if(range_start > blk_start && range_end < blk_end) {
    size_t left_size = range_start - blk_start - _block_header_length;
    TLSFBlockHeader *free_blk = split_block(blk, left_size);
    TLSFBlockHeader *right_blk = split_block(free_blk, range - _block_header_length);
    insert_block(free_blk);

  // Case 2: If the range is the entire block, we just free the block.
  } else if(range_start == blk_start && range_end == blk_end) {
    free(reinterpret_cast<TLSFBlockHeader *>(blk_start + _block_header_length));

  // Case 3: The range is touching the block end.
  } else if(range_end == blk_end) {
    size_t split_size = range_start - blk_start - _block_header_length;
    insert_block(split_block(blk, split_size));

  // Case 4: The range is touching the block start.
  } else if(range_start == blk_start) {
    size_t split_size = range_end - blk_start - _block_header_length;
    insert_block(split_block(blk, split_size));
  }
}

void TLSF::coalesce_blocks() {
  TLSFBlockHeader *current_blk = reinterpret_cast<TLSFBlockHeader *>(_block_start);
  TLSFBlockHeader *next_blk = get_next_phys_block(current_blk);

  while(next_blk != nullptr) {
    if(current_blk->is_free() && next_blk->is_free()) {
      current_blk = coalesce_blocks(current_blk, next_blk);
      insert_block(current_blk);
    } else {
      current_blk = next_blk;
    }

    next_blk = get_next_phys_block(current_blk);
  }
}

void TLSF::print_phys_blocks() {
  TLSFBlockHeader *current = (TLSFBlockHeader *)_block_start;

  while(current != nullptr) {
    print_blk(current);
    current = get_next_phys_block(current);
  }
}

void TLSF::print_free_lists() {
  print_flatmap();
  for(size_t i = 0; i < 64; i++) {
    if((_flatmap & (1UL << i)) == 0) {
      continue;
    }
    printf("FREE-LIST (%02ld): ", i);
    TLSFBlockHeader *current = _blocks[i];
    while(current != nullptr) {
      std::cout << current << " -> ";
      current = block_address(current->next);
    }
    std::cout << "END" << std::endl;
  }
}

void TLSF::print_flatmap() {
  print_binary(_flatmap);
}

uint32_t TLSF::block_offset(TLSFBlockHeader *blk) {
  return (blk == nullptr)
    ? TLSFBlockHeader::NULL_OFFSET
    : ((uintptr_t)blk - _block_start);
}

TLSFBlockHeader *TLSF::block_address(uint32_t offset) {
  return (offset == TLSFBlockHeader::NULL_OFFSET) 
    ? nullptr 
    : reinterpret_cast<TLSFBlockHeader *>(offset + _block_start);
}

uint32_t TLSF::get_mapping(size_t size) {
  int fl = TLSFUtil::ilog2(size);
  int sl = size >> (fl - _sl_index_log2) ^ (1UL << _sl_index_log2);
  return ((fl - _min_alloc_size_log2) << _sl_index_log2) + sl;
}

void TLSF::initialize(uintptr_t initial_pool, size_t pool_size, bool deferred_coalescing) {
  _deferred_coalescing = deferred_coalescing;
  _block_header_length = deferred_coalescing ? BLOCK_HEADER_LENGTH_SMALL : BLOCK_HEADER_LENGTH;
  _mbs = deferred_coalescing ? 16 : 64;

  uintptr_t aligned_initial_block = TLSFUtil::align_up(initial_pool, _alignment);
  _block_start = aligned_initial_block;

  // The pool size is shrinked to the initial aligned block size. This wastes
  // at maximum (_mbs - 1) bytes
  size_t aligned_block_size = TLSFUtil::align_down(pool_size - (aligned_initial_block - initial_pool), _mbs);
  _pool_size = aligned_block_size;

  clear();
}

void TLSF::insert_block(TLSFBlockHeader *blk) {
  uint32_t mapping = get_mapping(blk->get_size());

  TLSFBlockHeader *head = _blocks[mapping];

  // Insert the block into its corresponding free-list
  if(head != nullptr) {
    head->prev = block_offset(blk);
  }
  blk->next = block_offset(head);
  blk->prev = TLSFBlockHeader::NULL_OFFSET;
  _blocks[mapping] = blk;

  // Mark the block as free
  blk->mark_free();

  // Indicate that the list has free blocks by updating bitmap
  _flatmap |= (1 << mapping);
}

TLSFBlockHeader *TLSF::find_block(size_t size) {
  // Round up to alignment and then mbs and then to the nearest size class
  size_t aligned_size = TLSFUtil::align_up(TLSFUtil::align_up(size, _alignment), _mbs);
  size_t target_size = aligned_size + (1UL << (TLSFUtil::ilog2(aligned_size) - _sl_index_log2)) - 1;

  // With the mapping we search for a free block
  uint32_t mapping = get_mapping(target_size);

  // If the first-level index is out of bounds, the request cannot be fulfilled
  if(mapping >= _num_lists) {
    return nullptr;
  }

  uint64_t above_mapping = _flatmap & (~0UL << mapping);
  if(above_mapping == 0) {
    return nullptr;
  }

  mapping = TLSFUtil::ffs(above_mapping);

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

TLSFBlockHeader *TLSF::coalesce_blocks(TLSFBlockHeader *blk1, TLSFBlockHeader *blk2) {
  remove_block(blk1, get_mapping(blk1->get_size()));
  remove_block(blk2, get_mapping(blk2->get_size()));

  // Combine the blocks by adding the size of blk2 to block1 and also the block
  // header size
  blk1->size += _block_header_length + blk2->get_size();

  // Make sure the next physical block points to the now coalesced block instead
  // of blk2
  TLSFBlockHeader *next_phys = get_next_phys_block(blk1);
  if(next_phys != nullptr) {
    next_phys->prev_phys_block = blk1;
  }

  return blk1;
}

TLSFBlockHeader *TLSF::remove_block(TLSFBlockHeader *blk, uint32_t mapping) {
  TLSFBlockHeader *target = blk;

  if(blk == nullptr) {
    target = _blocks[mapping];
  }

  assert(target != nullptr);

  if(target->next != TLSFBlockHeader::NULL_OFFSET) {
    block_address(target->next)->prev = target->prev;
  }

  if(target->prev != TLSFBlockHeader::NULL_OFFSET) {
    block_address(target->prev)->next = target->next;
  }

  // Mark the block as used (no longer free)
  target->mark_used();

  // If the block is the last one in the free-list we need to update
  if(_blocks[mapping] == target) {
    _blocks[mapping] = block_address(target->next);
  }

  // If the block is the last one in the free-list, we mark it as empty
  if(target->next == TLSFBlockHeader::NULL_OFFSET) {
    _flatmap &= ~(1UL << mapping);
  }

  return target;
}

TLSFBlockHeader *TLSF::split_block(TLSFBlockHeader *blk, size_t size) {
  size_t remainder_size = blk->get_size() - _block_header_length - size;

  blk->size = size;

  // Use a portion of blk's memory for the new block
  TLSFBlockHeader *remainder_blk = reinterpret_cast<TLSFBlockHeader *>((uintptr_t)blk + _block_header_length + blk->get_size());
  remainder_blk->size = remainder_size;
  remainder_blk->prev_phys_block = blk;

  // Update the next phys block's previous block to remainder_blk block instead
  // of blk
  TLSFBlockHeader *next_phys = get_next_phys_block(remainder_blk);
  if(next_phys != nullptr) {
    next_phys->prev_phys_block = remainder_blk;
  }

  return remainder_blk;
}

TLSFBlockHeader *TLSF::get_next_phys_block(TLSFBlockHeader *blk) {
  uintptr_t pool_end = _block_start + _pool_size;
  uintptr_t next_blk = (uintptr_t)blk + _block_header_length + blk->get_size();
  return next_blk < pool_end ? (TLSFBlockHeader *)next_blk : nullptr;
}

TLSFBlockHeader *TLSF::get_block_containing_address(uintptr_t address) {
  uintptr_t target_addr = (uintptr_t)address;
  uintptr_t physical_end = _block_start + _pool_size;
  uintptr_t current = _block_start;

  while(current <= physical_end) {
    TLSFBlockHeader *current_blk = reinterpret_cast<TLSFBlockHeader *>(current);
    uintptr_t current_end = current + _block_header_length + current_blk->get_size();

    if(target_addr >= current && target_addr <= current_end) {
      return current_blk;
    }

    current = current + current_blk->get_size() + _block_header_length;
  }

  return nullptr;
}

