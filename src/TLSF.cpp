
// Author: Joel Sikström

#include <cmath>
#include <iostream>
#include <assert.h>

#include "TLSF.hpp"
#include "TLSFUtil.inline.hpp"

struct TLSFBlockHeader {
  size_t size;

  TLSFBlockHeader *prev_phys_block;

  // next and prev are only used in free (unused) blocks.
  TLSFBlockHeader *next;
  TLSFBlockHeader *prev;
};

#define BLOCK_HEADER_LENGTH offsetof(TLSFBlockHeader, next)

#define BLOCK_FREE_MASK 1 << 0
#define BLOCK__MASK 1 << 1

// Contains first- and second-level index to segregated lists.
struct TLSFMapping { uint32_t fl, sl; };

#ifndef NODEBUG
static void print_blk(TLSFBlockHeader *blk) {
  std::cout << "Block (@ " << blk << ")\n" 
            << " size=" << blk->size << "\n"
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

TLSF *TLSF::create(uintptr_t initial_pool, size_t pool_size) {
  // Check that the initial_pool is aligned.
  if(!TLSFUtil::is_aligned(initial_pool, ALIGN_SIZE)) {
    return nullptr;
  }

  TLSF *tlsf = reinterpret_cast<TLSF *>(initial_pool);

  tlsf->_mempool = initial_pool;

  // Initialize bitmaps and blocks
  tlsf->_fl_bitmap = 0;
  for(int i = 0; i < FL_INDEX; i++) {
    tlsf->_sl_bitmap[i] = 0;
    for(int j = 0; j < SL_INDEX; j++) {
      tlsf->_blocks[i][j] = nullptr;
    }
  }

  uintptr_t aligned_initial_block = TLSFUtil::align_up(initial_pool + sizeof(TLSF), ALIGN_SIZE);
  TLSFBlockHeader *blk = reinterpret_cast<TLSFBlockHeader *>(aligned_initial_block);

  size_t aligned_block_size = TLSFUtil::align_down(pool_size - sizeof(TLSF) - BLOCK_HEADER_LENGTH, MIN_BLOCK_SIZE);

  // The pool size is shrinked to the initial aligned block size. This wastes at maximum (MIN_BLOCK_SIZE - 1) bytes.
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

  // TODO: Check alignment
  return (void *)((uintptr_t)blk + BLOCK_HEADER_LENGTH);
}

void TLSF::free(void *address) {
  TLSFBlockHeader *blk = (TLSFBlockHeader *)((uintptr_t)address - BLOCK_HEADER_LENGTH);

  // Try to merge with adjacent blocks.
  // TODO: Need to check if they are free using their free bits.
  TLSFBlockHeader *prev_blk = blk->prev_phys_block;
  TLSFBlockHeader *next_blk = get_next_phys_block(blk);

  if(prev_blk != nullptr) { // && free bit is set
    blk = coalesce_blocks(prev_blk, blk);
  }

  if(next_blk != nullptr) { // && free bit is set
    blk = coalesce_blocks(blk, next_blk);
  }

  insert_block(blk);
}

void TLSF::print_phys_blocks() {
  TLSFBlockHeader *current = (TLSFBlockHeader *)TLSFUtil::align_up(_mempool + sizeof(TLSF), ALIGN_SIZE);

  while(current != nullptr) {
    print_blk(current);
    current = get_next_phys_block(current);
  }
}

void TLSF::insert_block(TLSFBlockHeader *blk) {
  TLSFMapping m = get_mapping(blk->size);

  TLSFBlockHeader *head = _blocks[m.fl][m.sl];

  // Insert the block into its corresponding free-list
  if(head != nullptr) {
    head->prev = blk;
  }
  blk->next = head;
  blk->prev = nullptr;
  _blocks[m.fl][m.sl] = blk;

  // Indicate that the list has free blocks by updating bitmap.
  _fl_bitmap |= (1 << m.fl);
  _sl_bitmap[m.fl] |= (1 << m.sl);
}

TLSFBlockHeader *TLSF::coalesce_blocks(TLSFBlockHeader *blk1, TLSFBlockHeader *blk2) {
  // TODO: Implement

  // Make sure both blocks are marked as free.

  // Combine the blocks by adding the size of blk2 to block1 and also the block header size.

  // Mark the block as free.

  return nullptr;
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

  // If the block is the last one in the free-list we need to update
  if(_blocks[mapping.fl][mapping.sl] == target) {
    _blocks[mapping.fl][mapping.sl] = target->next;
  }

  // If the block is the last one in the free-list, we mark it as empty.
  if(target->next == nullptr) {
    _sl_bitmap[mapping.fl] &= ~(1 << mapping.sl);

    // We also need to check if the first-level is now empty as well.
    if(_sl_bitmap[mapping.fl] == 0) {
      _fl_bitmap &= ~(1 << mapping.fl);
    }
  }

  return target;
}

TLSFBlockHeader *TLSF::split_block(TLSFBlockHeader *blk, size_t size) {
  size_t remainder_size = blk->size - BLOCK_HEADER_LENGTH - size;
  blk->size = size;

  // Use a portion of blk's memory for the new block.
  TLSFBlockHeader *remainder_blk = (TLSFBlockHeader *)((uintptr_t)blk + BLOCK_HEADER_LENGTH + blk->size);
  remainder_blk->size = remainder_size;

  remainder_blk->prev_phys_block = blk;

  // Update the next phys block's previous block to remainder_blk block instead
  // of blk.
  TLSFBlockHeader *next_phys = get_next_phys_block(remainder_blk);
  if(next_phys != nullptr) {
    next_phys->prev_phys_block = remainder_blk;
  }

  return remainder_blk;
}

TLSFBlockHeader *TLSF::get_next_phys_block(TLSFBlockHeader *blk) {
  uintptr_t pool_end = _mempool + _pool_size;
  uintptr_t next_blk = (uintptr_t)blk + BLOCK_HEADER_LENGTH + blk->size;
  return next_blk < pool_end ? (TLSFBlockHeader *)next_blk : nullptr;
}

TLSFBlockHeader *TLSF::find_block(size_t size) {
  // Round up to MIN_BLOCK_SIZE and then to the nearest size class.
  // TODO: Should not exceed the _fl_bitmap size.
  size_t aligned_size = TLSFUtil::align_up(size, MIN_BLOCK_SIZE);
  size_t target_size = aligned_size + (1 << (TLSFUtil::ilog2(aligned_size) - SL_INDEX_LOG2)) - 1;

  // With the mapping we search for a free block.
  TLSFMapping mapping = get_mapping(target_size);

  uint32_t sl_map = _sl_bitmap[mapping.fl] & (~0UL << mapping.sl);
  if(sl_map == 0) {
    // No suitable block exists in the second level. Search in the next largest
    // first-level instead.
    uint32_t fl_map = _fl_bitmap & (~0UL << (mapping.fl + 1));
    if(fl_map == 0) {
      // No suitable block exists.
      return nullptr;
    }

    mapping.fl = TLSFUtil::ffs(fl_map);
    sl_map = _sl_bitmap[mapping.fl];
  }

  mapping.sl = TLSFUtil::ffs(sl_map);

  // By now we now that we have an available block to use.
  TLSFBlockHeader *blk = remove_block(nullptr, mapping);

  // If the block is larger than some threshold relative to the requested size
  // it should be split up to minimize internal fragmentation.
  if((blk->size - aligned_size) >= (MIN_BLOCK_SIZE + BLOCK_HEADER_LENGTH)) {
    TLSFBlockHeader *remainder_blk = split_block(blk, aligned_size);
    insert_block(remainder_blk);
  }

  return blk;
}

TLSFMapping TLSF::get_mapping(size_t size) {
  uint32_t fl = TLSFUtil::ilog2(size);
  uint32_t fl2 = (1 << fl);
  uint32_t sl = (size - fl2) * SL_INDEX / fl2;
  return {fl, sl};
}

int main() {
  TLSFMapping m = TLSF::get_mapping(460);
  std::cout << m.fl << ", " << m .sl << std::endl;
  std::cout << "Header length: " << BLOCK_HEADER_LENGTH << std::endl;

  void *pool = calloc(1, 1024 * 10);
  TLSF *tl = TLSF::create((uintptr_t)pool, 1024 * 10);

  TLSFBlockHeader *blk = tl->find_block(129);
  blk = tl->find_block(330);
  std::cout << sizeof(TLSFBlockHeader) << std::endl;
  tl->print_phys_blocks();
}

