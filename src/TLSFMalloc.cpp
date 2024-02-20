
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <sys/mman.h>
#include <cerrno>

#include "TLSF.hpp"

static const size_t MEMPOOL_SIZE = 1024 * 1000 * 400;

uint8_t *mempool = nullptr;
static TLSF *tlsf_allocator = nullptr;

extern "C" {

  void initialize_tlsf() {
    mempool = static_cast<uint8_t *>(mmap(nullptr, MEMPOOL_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    if(mempool == MAP_FAILED) {
      perror("mmap failed");
      exit(1);
    }

    tlsf_allocator = TLSF::create((uintptr_t)mempool, MEMPOOL_SIZE);
  }

  void *calloc(size_t nmemb, size_t size) {
    if(tlsf_allocator == nullptr) {
      initialize_tlsf();
    }

    void *ptr = malloc(nmemb * size);
    if(ptr != nullptr) {
      memset(ptr, 0, nmemb * size);
    }

    return ptr;
  }

  void *malloc(size_t size) {
    if(tlsf_allocator == nullptr) {
      initialize_tlsf();
    }

    void *addr = tlsf_allocator->allocate(size);

    if(addr == nullptr) {
      errno = ENOMEM;
    }

    return addr;
  }

  void free(void *addr) {
    if(tlsf_allocator == nullptr) {
      initialize_tlsf();
    }

    tlsf_allocator->free(addr);
  }

  void *realloc(void *ptr, size_t size) {
    if(tlsf_allocator == nullptr) {
      initialize_tlsf();
    }

    if(ptr == NULL) {
      return malloc(size);
    } else if(ptr == NULL && size == 0) {
      return nullptr;
    }

    void *newalloc = tlsf_allocator->allocate(size);
    if(newalloc == nullptr) {
      return nullptr;
    }

    size_t old_size = tlsf_allocator->get_allocated_size(ptr);
    size_t copy_size = old_size < size ? old_size : size;

    memcpy(newalloc, ptr, copy_size);
    free(ptr);

    return newalloc;
  }
}
