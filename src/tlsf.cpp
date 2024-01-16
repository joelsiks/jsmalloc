
#include <cmath>
#include <iostream>

#include "tlsf.hpp"


struct TLSFBlockHeader {
  size_t size;

  TLSFBlockHeader *prev_phys_block;

  // next and prev are only used in free (unused) blocks.
  TLSFBlockHeader *next_free;
  TLSFBlockHeader *prev_free;
};

#define BLOCK_HEADER_LENGTH offsetof(TLSFBlockHeader, next_free)

// Contains first- and second-level index to TLSF segregated lists.
struct TLSFMapping { uint32_t fl, sl; };

TLSF *TLSF::create(uintptr_t initial_pool, size_t pool_size) {
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

  // Insert initial block to the corresponding free-list
  TLSFBlockHeader *blk = reinterpret_cast<TLSFBlockHeader *>(initial_pool + sizeof(TLSF));
  blk->size = pool_size - sizeof(TLSF) - BLOCK_HEADER_LENGTH;
  blk->prev_phys_block = nullptr;
  insert_block(blk);

  return tlsf;
}

static TLSFMapping mapping(size_t size) {
    // TODO: Implement useful strategy calculation from paper.
    uint32_t fl = (uint32_t)std::log2(size); // TODO: Bit fiddling
    uint32_t fl2 = (1 << fl);
    uint32_t sl = (size - fl2) * SL_INDEX / fl2;
    return {fl, sl};
}

void TLSF::destroy() {}

void TLSF::insert_block(TLSFBlockHeader *blk) {
    TLSFMapping m = mapping(blk->size);

    TLSFBlockHeader *head = _blocks[m.fl][m.sl];

    // TODO: Insert block into free list
    if(head == nullptr) {
        // Free-list emtpy, insert block as new head.
        _blocks[m.fl][m.sl] = blk;
    } else {
        // Free-list not emtpy, insert block before(?) current head and redirect head-> prev to new block.
    }

    // Update bitmap
    _fl_bitmap |= (1 << m.fl);
    _sl_bitmap[m.fl] |= (1 << m.sl);
}

void TLSF::find_block(size_t bytes) {
}

int main() {
    TLSFMapping m = mapping(460);
    std::cout << m.fl << ", " << m .sl << std::endl;
    std::cout << "Header length: " << BLOCK_HEADER_LENGTH << std::endl;
}
