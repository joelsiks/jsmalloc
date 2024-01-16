
#include <cstdlib>

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

TLSF::TLSF(size_t pool_size, size_t SLI, size_t MBS) {
  // TODO: pool_size should be a nice number.

  // Allocate memory for the initial block.
  m_mempool = (uintptr_t)calloc(1, pool_size);
}

TLSF::~TLSF() { free((void *)m_mempool); }
