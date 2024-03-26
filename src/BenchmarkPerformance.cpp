
#include <chrono>
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>
#include <map>

#include <sys/mman.h>

#include "JSMalloc.hpp"
#include "JSMallocUtil.inline.hpp"

//const std::string filename = "./distributions/dacapo_h2_total.txt";
const std::string filename = "./distributions/first.txt";

struct Operation {
  char type;
  size_t id;
  size_t size;
};

struct Allocation {
  void *addr;
  size_t size;
};

std::map<size_t, Allocation> heap;
std::vector<Operation> operations;

uint8_t *mmap_allocate(size_t pool_size) {
  return static_cast<uint8_t *>(mmap(nullptr, pool_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
}

void process_file(const std::string &filename) {
  std::ifstream file(filename);

  if(!file.is_open()) {
    std::cerr << "Failed to open the file: " << filename << std::endl;
    exit(1);
  }

  char type;
  size_t id, size;
  std::string line;

  while(std::getline(file, line)) {
    std::istringstream iss(line);
    iss >> type >> id >> size;
    operations.push_back({type, id, size});
  }

  file.close();
}

void apply_distribution(JSMalloc &allocator) {
  size_t counter = 0;
  for(const Operation &op : operations) {
    counter++;
    if(op.type == 'a') {
      void *addr = allocator.allocate(op.size);
      if(addr == nullptr) {
      }
      heap[op.id] = {addr, JSMallocUtil::align_up(op.size, 16)};
    } else {
      //allocator.free(heap[op.id].addr, heap[op.id].size);
      allocator.free(heap[op.id].addr);
      heap.erase(op.id);
    }
  }
}

int main() {
  size_t pool_size = 2097152;
  void *pool = mmap_allocate(pool_size);
  JSMalloc allocator(pool, pool_size, false);

  process_file(filename);

  auto start_time = std::chrono::high_resolution_clock::now();
  apply_distribution(allocator);

  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = end_time - start_time;
  const double seconds = std::chrono::duration<double>(duration).count();
  std::cout << seconds << std::endl;

  return 0;
}
