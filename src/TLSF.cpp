
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

// Contains first- and second-level index to segregated lists.
struct TLSFMapping { uint32_t fl, sl; };

#ifndef NODEBUG

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
  tlsf->_pool_size = pool_size;

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

  blk->size = aligned_block_size;
  blk->prev_phys_block = nullptr;

  // Insert initial block to the corresponding free-list
  tlsf->insert_block(blk);

  return tlsf;
}

void TLSF::destroy() {}

void *TLSF::allocate(size_t size) { return malloc(size); }

void TLSF::free(void *address) { (void)address; }

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

    // Indicate that the list have free blocks by updating bitmap.
    _fl_bitmap |= (1 << m.fl);
    _sl_bitmap[m.fl] |= (1 << m.sl);
}

void *TLSF::remove_block(TLSFBlockHeader *blk, TLSFMapping mapping) {
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
    if(blk->next == nullptr) {
        _sl_bitmap[mapping.fl] &= ~(1 << mapping.sl);

        // We also need to check if the first-level is now empty as well.
        if(_sl_bitmap[mapping.fl] == 0) {
            _fl_bitmap &= ~(1 << mapping.fl);
        }
    }

    return target;
}

void *TLSF::find_block(size_t size) {

    // TODO: Round up size?

    TLSFMapping mapping = get_mapping(size);

    uint32_t sl_map = _sl_bitmap[mapping.fl] & (~0UL << mapping.sl);

    return nullptr;
}

TLSFMapping TLSF::get_mapping(size_t size) {
    // TODO: Implement useful strategy (fls using clz etc..) calculation from paper.
    uint32_t fl = (uint32_t)std::log2(size);
    uint32_t fl2 = (1 << fl);
    uint32_t sl = (size - fl2) * SL_INDEX / fl2;
    return {fl, sl};
}

int main() {
    TLSFMapping m = TLSF::get_mapping(460);
    std::cout << m.fl << ", " << m .sl << std::endl;
    std::cout << "Header length: " << BLOCK_HEADER_LENGTH << std::endl;

    void *pool = calloc(1, 1024 * 10);
    TLSF *tl = TLSF::create((uintptr_t)pool, 1024 * 10 + 1);
}
