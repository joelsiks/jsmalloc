
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "TLSF.hpp"

#define MEMPOOL_SIZE (1024 * 10000)

uint8_t mempool[MEMPOOL_SIZE];
static TLSF *tlsf_allocator = nullptr;

extern "C" {

  void initialize_tlsf() {
    tlsf_allocator = TLSF::create((uintptr_t)mempool, MEMPOOL_SIZE);
  }

  void *calloc(size_t nmemb, size_t size) {
    if(tlsf_allocator == nullptr) {
      initialize_tlsf();
    }

    void *ptr = malloc(nmemb * size);
    memset(ptr, 0, nmemb * size);
    return ptr;
  }

  void *malloc(size_t size) {
    if(tlsf_allocator == nullptr) {
      initialize_tlsf();
    }

    return tlsf_allocator->allocate(size);
  }

  void free(void *addr) {
    if(tlsf_allocator == nullptr) {
      initialize_tlsf();
    }

    tlsf_allocator->free(addr);
  }
}
