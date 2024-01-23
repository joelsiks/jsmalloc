
#include <assert.h>
#include <iostream>

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
  assert(ptr3 == nullptr);
}

int main() {
  //basic_test();
  
  const size_t pool_size = sizeof(TLSF) + 32 * 8;
  uint8_t pool[pool_size];
  TLSF *t = TLSF::create((uintptr_t)&pool, pool_size);

  t->print_phys_blocks();
  t->print_flatmap();

  //t->allocate(1);


  /*
  uint8_t pool[1000 * 1024];
  TLSF *tl = TLSF::create((uintptr_t)pool, 1024 * 1000);

  void *a = tl->allocate(3000000000000);
  a = tl->allocate(72704);
  void *b = tl->allocate(128);
  a = tl->allocate(1024);
  a = tl->allocate(9488880);

  tl->print_phys_blocks();

  tl->free(b);

  std::cout << sizeof(TLSFBlockHeader) << std::endl;
  std::cout << BLOCK_HEADER_LENGTH << std::endl;
  */
}
