
// Author: Joel Sikström

#include <cmath>
#include <iostream>
#include <assert.h>

#include "TLSF.hpp"
#include "TLSFUtil.inline.hpp"

// Contains first- and second-level index to segregated lists
struct TLSFMapping { uint32_t fl, sl; };

#ifndef NODEBUG
static void print_blk(TLSFBlockHeader *blk) {
  std::cout << "Block (@ " << blk << ")\n" 
            << " size=" << blk->get_size() << "\n"
            << " prev=" << ((blk->prev_phys_block == nullptr) ? 0 : blk->prev_phys_block) << "\n";
}

static void print_mapping(TLSFMapping m) {
  std::cout << "Mapping: fl=" << m.fl << ", sl=" << m.sl << std::endl;
}

static void print_binary(uint32_t value) {
  for (int i = 31; i >= 0; --i) {
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

TLSF *TLSF::create(uintptr_t initial_pool, size_t pool_size) {
  // Check that the initial_pool is aligned
  if(!TLSFUtil::is_aligned(initial_pool, _alignment)) {
    return nullptr;
  }

  TLSF *tlsf = reinterpret_cast<TLSF *>(initial_pool);

  tlsf->_mempool = initial_pool;

  // Initialize bitmaps and blocks
  tlsf->_fl_bitmap = 0;
  for(int i = 0; i < _fl_index; i++) {
    tlsf->_sl_bitmap[i] = 0;
    for(int j = 0; j < _sl_index; j++) {
      tlsf->_blocks[i][j] = nullptr;
    }
  }

  uintptr_t aligned_initial_block = TLSFUtil::align_up(initial_pool + sizeof(TLSF), _alignment);
  TLSFBlockHeader *blk = reinterpret_cast<TLSFBlockHeader *>(aligned_initial_block);

  size_t aligned_block_size = TLSFUtil::align_down(pool_size - sizeof(TLSF) - BLOCK_HEADER_LENGTH, _mbs);

  // The pool size is shrinked to the initial aligned block size. This wastes
  // at maximum (_mbs - 1) bytes
  tlsf->_pool_size = aligned_block_size;

  blk->size = aligned_block_size;
  blk->prev_phys_block = nullptr;

  // Insert initial block to the corresponding free-list
  tlsf->insert_block(blk);

  return tlsf;
}

void TLSF::destroy() {}

void *TLSF::allocate(size_t size) {
  TLSFBlockHeader *blk = find_block(size);

  if(blk == nullptr) {
    return nullptr;
  } 

  // Make sure addresses are aligned to the word-size (8-bytes).
  // TODO: This might not be necessary if everything is already aligned, and
  // should take into account that the block size might be smaller than expected.
  uintptr_t blk_start = ((uintptr_t)blk + BLOCK_HEADER_LENGTH);
  return (void *)TLSFUtil::align_up(blk_start, _alignment);
}

void TLSF::free(void *address) {
  TLSFBlockHeader *blk = (TLSFBlockHeader *)((uintptr_t)address - BLOCK_HEADER_LENGTH);

  // Try to merge with adjacent blocks
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

void TLSF::print_phys_blocks() {
  TLSFBlockHeader *current = (TLSFBlockHeader *)TLSFUtil::align_up(_mempool + sizeof(TLSF), _alignment);

  while(current != nullptr) {
    print_blk(current);
    current = get_next_phys_block(current);
  }
}

void TLSF::insert_block(TLSFBlockHeader *blk) {
  TLSFMapping m = get_mapping(blk->get_size());

  TLSFBlockHeader *head = _blocks[m.fl][m.sl];

  // Insert the block into its corresponding free-list
  if(head != nullptr) {
    head->prev = blk;
  }
  blk->next = head;
  blk->prev = nullptr;
  _blocks[m.fl][m.sl] = blk;

  // Mark the block as free
  blk->mark_free();

  // Indicate that the list has free blocks by updating bitmap
  _fl_bitmap |= (1 << m.fl);
  _sl_bitmap[m.fl] |= (1 << m.sl);
}

TLSFBlockHeader *TLSF::coalesce_blocks(TLSFBlockHeader *blk1, TLSFBlockHeader *blk2) {
  // Make sure both blocks are removed from their free-lists
  if(blk1->is_free()) {
    remove_block(blk1, get_mapping(blk1->get_size()));
  }

  if(blk2->is_free()) {
    remove_block(blk2, get_mapping(blk2->get_size()));
  }

  // Combine the blocks by adding the size of blk2 to block1 and also the block
  // header size
  blk1->size += BLOCK_HEADER_LENGTH + blk2->get_size();

  // Make sure the next physical block points to the now coalesced block instead
  // of blk2
  TLSFBlockHeader *next_phys = get_next_phys_block(blk1);
  if(next_phys != nullptr) {
    next_phys->prev_phys_block = blk1;
  }

  return blk1;
}

TLSFBlockHeader *TLSF::remove_block(TLSFBlockHeader *blk, TLSFMapping mapping) {
  TLSFBlockHeader *target = blk;

  if(blk == nullptr) {
    target = _blocks[mapping.fl][mapping.sl];
  }

  assert(target != nullptr);

  if(target->next != nullptr) {
    target->next->prev = target->prev;
  }

  if(target->prev != nullptr) {
    target->prev->next = target->next;
  }

  // Mark the block as used (no longer free)
  target->mark_used();

  // If the block is the last one in the free-list we need to update
  if(_blocks[mapping.fl][mapping.sl] == target) {
    _blocks[mapping.fl][mapping.sl] = target->next;
  }

  // If the block is the last one in the free-list, we mark it as empty
  if(target->next == nullptr) {
    _sl_bitmap[mapping.fl] &= ~(1 << mapping.sl);

    // We also need to check if the first-level is now empty as well
    if(_sl_bitmap[mapping.fl] == 0) {
      _fl_bitmap &= ~(1 << mapping.fl);
    }
  }

  return target;
}

TLSFBlockHeader *TLSF::split_block(TLSFBlockHeader *blk, size_t size) {
  size_t remainder_size = blk->get_size() - BLOCK_HEADER_LENGTH - size;

  blk->size = size;

  // Use a portion of blk's memory for the new block
  TLSFBlockHeader *remainder_blk = (TLSFBlockHeader *)((uintptr_t)blk + BLOCK_HEADER_LENGTH + blk->get_size());
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
  uintptr_t pool_end = TLSFUtil::align_up(_mempool + sizeof(TLSF), _alignment) + _pool_size;
  uintptr_t next_blk = (uintptr_t)blk + BLOCK_HEADER_LENGTH + blk->get_size();
  return next_blk < pool_end ? (TLSFBlockHeader *)next_blk : nullptr;
}

TLSFBlockHeader *TLSF::find_block(size_t size) {
  // Round up to mbs and then to the nearest size class
  size_t aligned_size = TLSFUtil::align_up(size, _mbs);
  size_t target_size = aligned_size + (1UL << (TLSFUtil::ilog2(aligned_size) - _sl_index_log2)) - 1;

  // With the mapping we search for a free block
  TLSFMapping mapping = get_mapping(target_size);

  // If the first-level index is out of bounds, the request cannot be fulfilled
  if(mapping.fl >= _fl_index) {
    return nullptr;
  }

  uint32_t sl_map = _sl_bitmap[mapping.fl] & (~0UL << mapping.sl);
  if(sl_map == 0) {
    // No suitable block exists in the second level. Search in the next largest
    // first-level instead
    uint32_t fl_map = _fl_bitmap & (~0UL << (mapping.fl + 1));
    if(fl_map == 0) {
      // No suitable block exists.
      return nullptr;
    }

    mapping.fl = TLSFUtil::ffs(fl_map);
    sl_map = _sl_bitmap[mapping.fl];
  }

  mapping.sl = TLSFUtil::ffs(sl_map);

  // By now we now that we have an available block to use
  TLSFBlockHeader *blk = remove_block(nullptr, mapping);

  // If the block is larger than some threshold relative to the requested size
  // it should be split up to minimize internal fragmentation
  if((blk->get_size() - aligned_size) >= (_mbs + BLOCK_HEADER_LENGTH)) {
    TLSFBlockHeader *remainder_blk = split_block(blk, aligned_size);
    insert_block(remainder_blk);
  }

  return blk;
}

TLSFMapping TLSF::get_mapping(size_t size) {
  uint32_t fl = TLSFUtil::ilog2(size);
  uint32_t fl2 = (1 << fl);
  uint32_t sl = (size - fl2) * _sl_index / fl2;
  return {fl, sl};
}

int main() {
  void *pool = calloc(1, 1024 * 10);
  TLSF *tl = TLSF::create((uintptr_t)pool, 1024 * 10);

  void *a = tl->allocate(3000000000000);
  a = tl->allocate(32);
  a = tl->allocate(32);
  a = tl->allocate(32);
  a = tl->allocate(35);
  a = tl->allocate(18);

  tl->print_phys_blocks();

  a = tl->allocate(32);
  tl->free(a);

  std::cout << sizeof(TLSFBlockHeader) << std::endl;
  std::cout << BLOCK_HEADER_LENGTH << std::endl;
}
