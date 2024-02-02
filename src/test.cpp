
// Author: Joel Sikström

#include <assert.h>
#include <iostream>
#include <chrono>

#include "TLSF.hpp"

void basic_test() {
  std::cout << "sizeof(TLSF): " << sizeof(TLSF) << std::endl;
  const size_t pool_size = sizeof(TLSF) + 1024 * 1000;
  uint8_t pool[pool_size];
  TLSF *t = TLSF::create((uintptr_t)&pool, pool_size);

  void *ptr1 = t->allocate(1);
  assert(ptr1 != nullptr);

  void *ptr2 = t->allocate(1);
  assert(ptr2 != nullptr);

  void *ptr3 = t->allocate(1);
  assert(ptr3 != nullptr);

  void *ptr4 = t->allocate(1);
  assert(ptr3 != nullptr);

  t->free(ptr1);
  t->free(ptr3);
}

void constructor_test() {
  const size_t pool_size = 32 * 16 + 16;
  uint8_t pool[pool_size];
  TLSF t((uintptr_t)&pool, pool_size);

  void *a = t.allocate(1);
  t.allocate(1);
  void *b = t.allocate(1);
  t.allocate(1);

  t.free(a);
  t.free(b);

  t.print_phys_blocks();
  t.print_free_lists();
}

void free_range_test() {
  const size_t pool_size = 32 * 16;
  uint8_t pool[pool_size];
  ZPageOptimizedTLSF t((uintptr_t)&pool, pool_size);
  t.clear(true);
  t.print_phys_blocks();
  t.free_range((void *)((uintptr_t)&pool + 64), 64);
  std::cout << "---------------\n";
  t.print_phys_blocks();
}

void deferred_coalescing_test() {
  const size_t pool_size = 16 * 16 + 8;
  uint8_t pool[pool_size];
  ZPageOptimizedTLSF t((uintptr_t)&pool, pool_size);

  void *obj1 = t.allocate(1);
  void *obj2 = t.allocate(1);
  t.allocate(1);
  void *obj3 = t.allocate(1);
  void *obj4 = t.allocate(1);

  t.free(obj1);
  t.free(obj2);
  t.free(obj3);
  t.free(obj4);
  std::cout << "Deferred coalescing:\n";
  t.print_phys_blocks();
  t.coalesce_blocks();
  std::cout << "------------------\n";
  t.print_phys_blocks();
}

uint8_t cupool[10000 * 1024];

void CUnit_initialize_test() {
  TLSF *tl = TLSF::create((uintptr_t)cupool, 10000 * 1024);

  void *a = tl->allocate(3000000000000);
  a = tl->allocate(72704);
  void *b = tl->allocate(128);
  a = tl->allocate(1024);
  a = tl->allocate(9488880);
}

void optimized_test() {
  uint8_t pool[500 * 1024];
  ZPageOptimizedTLSF t((uintptr_t)&pool, 500 * 1024);

  t.allocate(1);
}

void TLSF_test4() {
  size_t pool_size = 2000 * 1024;
  uint8_t pool1[2000 * 1024];
  TLSF alloc((uintptr_t)pool1, pool_size);
  
  auto start_time = std::chrono::high_resolution_clock::now();
  alloc.allocate(40);
  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time);
  std::cout << "General " << duration.count() << std::endl;;

  uint8_t pool2[2000 * 1024];
  ZPageOptimizedTLSF zalloc((uintptr_t)pool2, pool_size);
  start_time = std::chrono::high_resolution_clock::now();
  zalloc.allocate(40);
  end_time = std::chrono::high_resolution_clock::now();
  duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time);
  std::cout << "Optimized " << duration.count() << std::endl;;
}

int main() {
  constructor_test();
  basic_test();
  free_range_test();
  deferred_coalescing_test();
  optimized_test();
  CUnit_initialize_test();
  //TLSF_test4();
}
