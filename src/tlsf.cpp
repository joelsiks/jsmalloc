
#include "tlsf.hpp"

// Default value for the minimum block size.
#define TLSF_MBS 16

struct TLSFBlockHeader {
  size_t size;

  TLSFBlockHeader *prev_phys_block;

  // next and prev are only used in free (unused) blocks.
  TLSFBlockHeader *next_free;
  TLSFBlockHeader *prev_free;
};

TLSF *TLSF::create(uintptr_t mempool, size_t pool_size, size_t SLI, size_t MBS) {
  TLSF *tlsf = reinterpret_cast<TLSF *>(mempool);

  tlsf->_mempool = mempool;
  tlsf->_pool_size = pool_size;

  return tlsf;
}

void TLSF::destroy() {}
