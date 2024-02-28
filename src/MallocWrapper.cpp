
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "JSMalloc.hpp"

static const size_t MEMPOOL_SIZE = 1024 * 1000 * 2000;

void *mempool = nullptr;
static JSMalloc *jsmalloc = nullptr;
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

  void initialize_jsmalloc() {
    mempool = static_cast<void *>(mmap(nullptr, MEMPOOL_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    if(mempool == MAP_FAILED) {
      perror("mmap failed");
      exit(1);
    }

    jsmalloc = JSMalloc::create(mempool, MEMPOOL_SIZE);

    const char *log_file_name = getenv("LOG_ALLOC");
    if(log_file_name) {
      log_file_fd = creat(log_file_name, 0644);
    }
  }

  void *calloc(size_t nmemb, size_t size) {
    if(jsmalloc == nullptr) {
      initialize_jsmalloc();
    }

    void *ptr = malloc(nmemb * size);
    if(ptr != nullptr) {
      memset(ptr, 0, nmemb * size);
    }

    return ptr;
  }

  void *malloc(size_t size) {
    if(jsmalloc == nullptr) {
      initialize_jsmalloc();
    }

    if(log_file_fd != 0) {
      log_allocation_to_file(size);
    }

    void *addr = jsmalloc->allocate(size);

    if(addr == nullptr) {
      errno = ENOMEM;
    }

    return addr;
  }

  void free(void *addr) {
    if(jsmalloc == nullptr) {
      initialize_jsmalloc();
    }

    jsmalloc->free(addr);
  }

  void *realloc(void *ptr, size_t size) {
    if(jsmalloc == nullptr) {
      initialize_jsmalloc();
    }

    if(ptr == NULL) {
      return malloc(size);
    } else if(ptr == NULL && size == 0) {
      return nullptr;
    }

    void *newalloc = jsmalloc->allocate(size);
    if(newalloc == nullptr) {
      return nullptr;
    }

    size_t old_size = jsmalloc->get_allocated_size(ptr);
    size_t copy_size = old_size < size ? old_size : size;

    memcpy(newalloc, ptr, copy_size);
    free(ptr);

    return newalloc;
  }
}
