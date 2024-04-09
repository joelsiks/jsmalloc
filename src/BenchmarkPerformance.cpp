
#include <chrono>
#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <vector>
#include <map>

#include <sys/mman.h>

#include "JSMalloc.hpp"
#include "JSMallocUtil.inline.hpp"

#include "TLSF.hpp"
#include "TLSFUtil.inline.hpp"

static void print_bits(uint64_t n) {
    for (int i = 63; i >= 0; --i) {
        std::cout << ((n >> i) & 1);
    }
    std::cout << std::endl;
}

struct Operation {
  char type;
  std::string id;
  size_t size;
};

static const size_t PAGE_SIZE = 2097152;
static const size_t BLOCK_HEADER_SIZE = 0;

std::map<std::string, JSMallocAlloc> heap;
std::map<void *, size_t> allocmap;
std::vector<Operation> operations;

std::vector<bool> collect_heap(void *heap_start) {
  uintptr_t hstart = (uintptr_t)heap_start;
  std::vector<bool> partitions(PAGE_SIZE / 16, false);

  // Fill heap table.
  for(const auto &obj : heap) {
    const JSMallocAlloc& a = obj.second;
    size_t ostart = (((uintptr_t)a.addr) - hstart) / 16;
    for(size_t i = 0; i < (BLOCK_HEADER_SIZE / 16); i++) {
      partitions[ostart - 1 - i] = true;
    }

    for(size_t i = 0; i < (a.size / 16); i++) {
      partitions[ostart + i] = true;
    }
  }

  return partitions;
}

std::vector<size_t> collect_heap_holes(void *heap_start) {
  std::vector<size_t> holes;
  std::vector<bool> partitions = collect_heap(heap_start);

  size_t counter = 0;
  for(size_t i = 0; i < partitions.size(); i++) {
    if (partitions[i] || i == (partitions.size() - 1)) {
      if(i != 0 && counter > 0) {
        holes.push_back(counter);
      }
      counter = 0;
    } else {
      counter += 16;
    }
  }

  return holes;
}

void dump_heap_state(void *heap_start) {
  std::map<size_t, size_t> counts;
  std::vector<size_t> holes = collect_heap_holes(heap_start);
  size_t last_free = 0;

  for(size_t i = 0; i < holes.size(); i++) {
    if(i == holes.size() - 1) {
      last_free = holes[i];
    } else {
      counts[holes[i]]++;
    }
  }

  for(const auto& pair : counts) {
    std::cout << std::setw(7) << pair.second << " " << pair.first << std::endl;
  }

  std::cout << "END " << last_free << std::endl;
}

void print_free_blocks(void *heap_start) {
  std::vector<size_t> holes = collect_heap_holes(heap_start);

  for(size_t hole : holes) {
    std::cout << hole << std::endl;
  }
}

void print_heap(void *heap_start) {
  std::vector<bool> partitions = collect_heap(heap_start);

  for(size_t i = 0; i < partitions.size(); i++) {
    if (partitions[i]) std::cout << '1';
    else std::cout << "\033[41m \033[0m";

    if(i % 256 == 0) std::cout << std::endl;
  }
}

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
  std::string id;
  size_t size;
  std::string line;

  while(std::getline(file, line)) {
    std::istringstream iss(line);
    iss >> type >> id >> size;
    operations.push_back({type, id, size});
  }

  file.close();
}

void apply_distribution(TLSF *allocator, void *heap_start) {
  size_t counter = 0;
  char last_op = 'a';

  for(const Operation &op : operations) {
    if(op.type == 'a' && last_op == 'f') {
      //allocator.coalesce(allocmap);
    }

    if(op.type == 'a') {
      //JSMallocAlloc alloc = allocator.debug_allocate(op.size);
      //JSMallocAlloc alloc = allocator.allocate(op.size);
      JSMallocAlloc alloc = allocator->allocate(op.size);

      if(alloc.addr != nullptr) {
        heap[op.id] = alloc;
        allocmap.insert({alloc.addr, alloc.size});
      } else {
        std::cout << "Failed to allocate: " << op.size << std::endl;
      }
    } else if(op.type == 'f') {
      if(heap.find(op.id) != heap.end()) {
        allocator->free(heap[op.id].addr);
        //allocator.free(heap[op.id].addr);
        //allocator.free(heap[op.id].addr, heap[op.id].size);

        allocmap.erase(heap[op.id].addr);
        heap.erase(op.id);
      }
    }

    counter++;
    last_op = op.type;
  }
}

int main(int argc, char **argv) {
  if(argc < 2) {
    std::cout << "Usage: ./" << argv[0] << " <filename>" << std::endl;
    std::cout << "The provided file should describe an allocation/free pattern on the following form:" << std::endl;
    std::cout << "Allocation:\t 'a <id> <size>'" << std::endl;
    std::cout << "Free:\t\t 'f <id> <size>'" << std::endl;
    exit(1);
  }

  std::string filename = argv[1];

  void *pool1 = mmap_allocate(PAGE_SIZE + sizeof(TLSF));
  TLSF *tlsf = TLSF::create((uintptr_t)pool1, PAGE_SIZE + sizeof(TLSF));

  void *pool2 = mmap_allocate(PAGE_SIZE);
  JSMalloc jsm(pool2, PAGE_SIZE, false);

  void *pool3 = mmap_allocate(PAGE_SIZE);
  JSMallocZ jsmz(pool3, PAGE_SIZE, false);


  process_file(filename);

  auto start_time = std::chrono::high_resolution_clock::now();
  apply_distribution(tlsf, pool1);
  auto end_time = std::chrono::high_resolution_clock::now();

  auto duration = end_time - start_time;
  const double seconds = std::chrono::duration<double>(duration).count();
  std::cout << seconds << std::endl;

  return 0;
}
