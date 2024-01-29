
// Author: Joel Sikström

#include <assert.h>
#include <iostream>

#include "TLSF.hpp"

void basic_test() {
  const size_t pool_size = sizeof(TLSF) + 32 * 16;
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
  TLSF t((uintptr_t)&pool, pool_size);
  t.clear(true);
  t.print_phys_blocks();
  t.free_range((void *)((uintptr_t)&pool + 64), 64);
  std::cout << "---------------\n";
  t.print_phys_blocks();
}

void deferred_coalescing_test() {
  const size_t pool_size = 24 * 16;
  uint8_t pool[pool_size];
  TLSF t((uintptr_t)&pool, pool_size, true);

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

void CUnit_initialize_test() {
  uint8_t pool[1000 * 1024];
  TLSF *tl = TLSF::create((uintptr_t)pool, 1024 * 1000);

  void *a = tl->allocate(3000000000000);
  a = tl->allocate(72704);
  void *b = tl->allocate(128);
  a = tl->allocate(1024);
  a = tl->allocate(9488880);
}

int main() {
  basic_test();
  constructor_test();
  free_range_test();
  deferred_coalescing_test();
}
