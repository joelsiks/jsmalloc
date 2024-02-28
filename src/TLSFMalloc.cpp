
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "TLSF.hpp"

static const size_t MEMPOOL_SIZE = 1024 * 1000 * 2000;

void *mempool = nullptr;
static TLSF *tlsf_allocator = nullptr;
static int log_file_fd = 0;

extern "C" {

  void log_allocation_to_file(size_t size) {
    const int max_len = 11;
    char buffer[max_len];
    int len = snprintf(buffer, max_len, "%lu\n", (unsigned long)size);

    if(len >= 0 && len < max_len) {
      if(write(log_file_fd, buffer, len) == -1) {
        perror("failed to log");
      }
    }
  }

  void initialize_tlsf() {
    mempool = static_cast<void *>(mmap(nullptr, MEMPOOL_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    if(mempool == MAP_FAILED) {
      perror("mmap failed");
      exit(1);
    }

    tlsf_allocator = TLSF::create(mempool, MEMPOOL_SIZE);

    const char *log_file_name = getenv("LOG_ALLOC");
    if(log_file_name) {
      log_file_fd = creat(log_file_name, 0644);
    }
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

    if(log_file_fd != 0) {
      log_allocation_to_file(size);
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
