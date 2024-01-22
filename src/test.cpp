
#include <assert.h>

#include "TLSF.hpp"

void basic_test() {
  const size_t pool_size = sizeof(TLSF) + 32 * 4;
  uint8_t pool[pool_size];
  TLSF *t = TLSF::create((uintptr_t)&pool, pool_size);

  void *ptr1 = t->allocate(1);
  assert(ptr1 != nullptr);

  void *ptr2 = t->allocate(1);
  assert(ptr2 != nullptr);

  void *ptr3 = t->allocate(1);
  assert(ptr3 != nullptr);

  void *ptr4 = t->allocate(1);
  assert(ptr4 == nullptr);
}

/*
int main() {
  basic_test();
}
*/
