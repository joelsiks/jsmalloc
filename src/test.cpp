
// Author: Joel Sikström

#include <assert.h>
#include <iostream>
#include <chrono>
#include <cstring>
#include <sys/mman.h>

#include <x86intrin.h>

#include <map>

#include "JSMalloc.hpp"

uint8_t *mmap_allocate(size_t pool_size) {
  return static_cast<uint8_t *>(mmap(nullptr, pool_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
}

void basic_test() {
  const size_t pool_size = sizeof(JSMalloc) + 1024 * 1000;
  uint8_t *pool = mmap_allocate(pool_size);
  JSMalloc *t = JSMalloc::create(pool, pool_size);

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
  JSMalloc t(pool, pool_size);

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

  JSMallocZ t(pool, pool_size, true);
  t.print_phys_blks();
  std::cout << "---------------\n";

  size_map[(void *)pool] = 32;
  t.free_range((void *)((uintptr_t)pool + 32), 32);
  size_map[(void *)((uintptr_t)pool + 64)] = 64;

  t.print_phys_blks();
  //t.free_range((void *)((uintptr_t)pool + 48), 16);
  //std::cout << "---------------\n";

  //t.print_phys_blks();
}

void coalescing_test() {
  const size_t pool_size = 1024;
  uint8_t *pool = mmap_allocate(pool_size);
  JSMalloc alloc(pool, pool_size);

  void *obj1 = alloc.allocate(32);
  memset(obj1, 0, 32);

  void *obj2 = alloc.allocate(32);
  memset(obj2, 0, 32);

  void *obj3 = alloc.allocate(32);
  memset(obj3, 0, 32);
  
  void *obj4 = alloc.allocate(32);
  memset(obj4, 0, 32);

  alloc.print_phys_blks();
  //alloc.print_free_lists();
  std::cout << "---\n";

  alloc.free(obj1);
  alloc.print_free_lists();

  std::cout << "---\n";
  alloc.free(obj4);
  alloc.print_phys_blks();
  alloc.print_free_lists();
}

void deferred_coalescing_test() {
  const size_t pool_size = 16 * 16 + 8;
  uint8_t *pool = mmap_allocate(pool_size);
  JSMallocZ t(pool, pool_size, false);

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
  JSMalloc *tl = JSMalloc::create(pool, 10000 * 1024);

  tl->allocate(3000000000000);
  tl->allocate(72704);
  tl->allocate(128);
  tl->allocate(1024);
  tl->allocate(9488880);
}

void optimized_test() {
  size_t pool_size = 1024;
  uint8_t *pool = mmap_allocate(pool_size);
  JSMallocZ t(pool, pool_size, false);

  int *arr = (int *)t.allocate(128);
  size_map[arr] = 128;
  for(int i = 0; i < 4; i++) {
    arr[i] = 13;
  }
}

void benchmark_comparison_untimed() {
  size_t pool_size = 20000 * 1024;
  uint8_t *pool1 = mmap_allocate(pool_size);
  JSMalloc alloc(pool1, pool_size);
  
  for(int i = 0; i < 400000; i++) {
    void *a = alloc.allocate(32);
    alloc.free(a);
  }

  uint8_t *pool2 = mmap_allocate(pool_size);
  JSMallocZ zalloc(pool2, pool_size, false);

  for(int i = 0; i < 400000; i++) {
    void *a = zalloc.allocate(32);
    zalloc.free(a, 32);
  }
}

void benchmark_comparison() {
  size_t pool_size = 2000 * 1024;
  uint8_t *pool1 = mmap_allocate(pool_size);
  JSMalloc alloc(pool1, pool_size);
  
  auto start_time = std::chrono::high_resolution_clock::now();
  void *a = alloc.allocate(32);
  alloc.free(a);
  auto end_time = std::chrono::high_resolution_clock::now();

  auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time);
  std::cout << "General " << duration.count() << std::endl;;

  uint8_t *pool2 = mmap_allocate(pool_size);
  JSMallocZ zalloc(pool2, pool_size, false);

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
  JSMalloc alloc(pool, pool_size);

  void *addr = alloc.allocate(0);
  std::cout << "Got: " << addr << std::endl;
}

void rdtsc_test() {
  size_t pool_size = 1024 * 10000;
  uint8_t *pool = mmap_allocate(pool_size);
  JSMallocZ alloc(pool, pool_size, false);

  int iters = 100;
  uint64_t start = 0, avg = 0;
  for(int i = 0; i < iters; i++) {
    start = __rdtsc();
    void *addr = alloc.allocate(131072);
    avg += __rdtsc() - start;
  }
  std::cout << "allocate(0) cycles: " << avg / iters << std::endl;
}

int main() {
  //basic_test();
  //constructor_test();
  //free_range_test();
  coalescing_test();
  //deferred_coalescing_test();
  //CUnit_initialize_test();
  //optimized_test();
  //benchmark_comparison_untimed();
  //benchmark_comparison();
  //zero_test();
  //rdtsc_test();
}
