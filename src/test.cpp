
// Author: Joel Sikström

#include <assert.h>
#include <iostream>
#include <chrono>
#include <cstring>
#include <sys/mman.h>

#include <map>

#include "TLSF.hpp"

uint8_t *mmap_allocate(size_t pool_size) {
  return static_cast<uint8_t *>(mmap(nullptr, pool_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
}

void basic_test() {
  const size_t pool_size = sizeof(TLSF) + 1024 * 1000;
  uint8_t *pool = mmap_allocate(pool_size);
  TLSF *t = TLSF::create((uintptr_t)pool, pool_size);

  void *ptr1 = t->allocate(1);
  assert(ptr1 != nullptr);

  void *ptr2 = t->allocate(1);
  assert(ptr2 != nullptr);

  void *ptr3 = t->allocate(1);
  assert(ptr3 != nullptr);

  void *ptr4 = t->allocate(1);
  assert(ptr4 != nullptr);

  t->free(ptr1);
  t->free(ptr3);
}

void constructor_test() {
  const size_t pool_size = 32 * 16 + 16;
  uint8_t *pool = mmap_allocate(pool_size);
  TLSF t((uintptr_t)pool, pool_size);

  void *a = t.allocate(1);
  t.allocate(1);
  void *b = t.allocate(1);
  t.allocate(1);

  t.free(a);
  t.free(b);

  t.print_phys_blks();
  t.print_free_lists();
}

std::map<void *, size_t> size_map;
size_t size_mapping(void *address) {
  return size_map[address];
}

void free_range_test() {
  const size_t pool_size = 128;
  uint8_t *pool = mmap_allocate(pool_size);

  ZPageOptimizedTLSF t((uintptr_t)pool, pool_size, size_mapping, true);
  t.print_phys_blks();
  std::cout << "---------------\n";

  t.free_range((void *)((uintptr_t)pool + 32), 32);
  t.print_phys_blks();
  //t.free_range((void *)((uintptr_t)pool + 48), 16);
  //std::cout << "---------------\n";

  //t.print_phys_blks();
}

void deferred_coalescing_test() {
  const size_t pool_size = 16 * 16 + 8;
  uint8_t *pool = mmap_allocate(pool_size);
  ZPageOptimizedTLSF t((uintptr_t)pool, pool_size, size_mapping, false);

  void *obj1 = t.allocate(1);
  memset(obj1, 0, 16);
  size_map[obj1] = 16;

  void *obj2 = t.allocate(1);
  memset(obj2, 0, 16);
  size_map[obj2] = 16;

  void *a = t.allocate(1);
  size_map[a] = 16;

  void *obj3 = t.allocate(1);
  memset(obj3, 0, 16);
  size_map[obj3] = 16;

  void *obj4 = t.allocate(1);
  memset(obj4, 0, 16);
  size_map[obj4] = 16;

  t.free(obj1, 16);
  size_map.erase(obj1);
  t.free(obj2, 16);
  size_map.erase(obj2);
  t.free(obj3, 16);
  size_map.erase(obj3);
  t.free(obj4, 16);
  size_map.erase(obj4);

  std::cout << "Deferred coalescing:\n";
  t.print_phys_blks();
  t.print_free_lists();

  t.aggregate();

  std::cout << "------------------\n";
  t.print_phys_blks();
  t.print_free_lists();
}

void CUnit_initialize_test() {
  size_t pool_size = 10000 * 1024;
  uint8_t *pool = mmap_allocate(pool_size);
  TLSF *tl = TLSF::create((uintptr_t)pool, 10000 * 1024);

  tl->allocate(3000000000000);
  tl->allocate(72704);
  tl->allocate(128);
  tl->allocate(1024);
  tl->allocate(9488880);
}

void optimized_test() {
  size_t pool_size = 1024;
  uint8_t *pool = mmap_allocate(pool_size);
  ZPageOptimizedTLSF t((uintptr_t)pool, pool_size, size_mapping, false);

  int *arr = (int *)t.allocate(128);
  size_map[arr] = 128;
  for(int i = 0; i < 4; i++) {
    arr[i] = 13;
  }
}

void benchmark_comparison_untimed() {
  size_t pool_size = 20000 * 1024;
  uint8_t *pool1 = mmap_allocate(pool_size);
  TLSF alloc((uintptr_t)pool1, pool_size);
  
  for(int i = 0; i < 400000; i++) {
    void *a = alloc.allocate(32);
    alloc.free(a);
  }

  uint8_t *pool2 = mmap_allocate(pool_size);
  ZPageOptimizedTLSF zalloc((uintptr_t)pool2, pool_size, size_mapping, false);

  for(int i = 0; i < 400000; i++) {
    void *a = zalloc.allocate(32);
    zalloc.free(a, 32);
  }
}

void benchmark_comparison() {
  size_t pool_size = 2000 * 1024;
  uint8_t *pool1 = mmap_allocate(pool_size);
  TLSF alloc((uintptr_t)pool1, pool_size);
  
  auto start_time = std::chrono::high_resolution_clock::now();
  void *a = alloc.allocate(32);
  alloc.free(a);
  auto end_time = std::chrono::high_resolution_clock::now();

  auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time);
  std::cout << "General " << duration.count() << std::endl;;

  uint8_t *pool2 = mmap_allocate(pool_size);
  ZPageOptimizedTLSF zalloc((uintptr_t)pool2, pool_size, size_mapping, false);

  start_time = std::chrono::high_resolution_clock::now();
  a = zalloc.allocate(32);
  zalloc.free(a, 32);
  end_time = std::chrono::high_resolution_clock::now();

  duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time);
  std::cout << "Optimized " << duration.count() << std::endl;;
}

void zero_test() {
  size_t pool_size = 1024;
  uint8_t *pool = mmap_allocate(pool_size);
  TLSF alloc((uintptr_t)pool, pool_size);

  void *addr = alloc.allocate(0);
  std::cout << "Got: " << addr << std::endl;
}

int main() {
  basic_test();
  constructor_test();
  free_range_test();
  deferred_coalescing_test();
  CUnit_initialize_test();
  optimized_test();
  benchmark_comparison_untimed();
  benchmark_comparison();
  zero_test();
}
