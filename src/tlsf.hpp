
// Author: Joel Sikström

#ifndef TLSF_HPP
#define TLSF_HPP

#include <cstddef>
#include <cstdint>

class TLSF {
public:
  TLSF(size_t pool_size, size_t SLI, size_t MBS);

  ~TLSF();

  void *allocate(size_t bytes);
  void free(void *address);

private:
  uintptr_t m_mempool;
};

#endif // TLSF_HPP
