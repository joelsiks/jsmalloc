
#include <array>
#include <chrono>
#include <cstddef>
#include <fstream>
#include <functional>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <sys/mman.h>

#include "JSMalloc.hpp"

const int n_runs = 1;
const int n_threads = 100;
const size_t max_allocs = -1;
const bool should_free = true;
const std::string filename = "distributions/dacapo_h2_1.txt";

std::array<std::vector<std::pair<void *, size_t>>, n_threads> allocations;

uint8_t *mmap_allocate(size_t pool_size) {
  return static_cast<uint8_t *>(mmap(nullptr, pool_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
}

void allocate_run(JSMallocZ &allocator, std::vector<size_t> &allocation_sizes, int i) {
  size_t num_allocations = 0;
  for (size_t size : allocation_sizes) {
    if (num_allocations++ >= max_allocs) {
      break;
    }

    void *p = allocator.allocate(size);

    if (should_free) {
      allocations[i].push_back(std::make_pair(p, size));
    }
  }
}

void process_file(const std::string &filename, std::vector<size_t> &allocation_sizes) {
  std::ifstream file(filename);

  if (!file.is_open()) {
    std::cerr << "Failed to open the file: " << filename << std::endl;
    exit(1);
  }

  std::string line;
  while (std::getline(file, line)) {
    try {
      const int number = std::stoi(line);
      allocation_sizes.push_back(number);
    } catch (const std::invalid_argument &ia) {
      std::cerr << "Invalid argument: " << ia.what() << std::endl;
    } catch (const std::out_of_range &oor) {
      std::cerr << "Out of range: " << oor.what() << std::endl;
    }
  }

  file.close();
}

int main() {
  size_t pool_size = 10 * 1000 * 1024;
  void *pool = mmap_allocate(pool_size);
  JSMallocZ allocator(pool, pool_size, false);

  std::vector<std::thread> threads(n_threads);

  std::vector<size_t> allocation_sizes;

  process_file(filename, allocation_sizes);

  auto start_time = std::chrono::high_resolution_clock::now();

  for (int i = 0; i < n_runs; i++) {

    for (int j = 0; j < n_threads; j++) {
      threads[j] =
          std::thread(allocate_run, std::ref(allocator), std::ref(allocation_sizes), j);
    }

    for (auto &t : threads) {
      t.join();
    }

    if (should_free) {
      for (auto &vec : allocations) {
        for (const auto &p : vec) {
          allocator.free(p.first, p.second);
        }
        vec.clear();
      }
    }
  }

  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = end_time - start_time;
  const double seconds = std::chrono::duration<double>(duration).count();
  std::cout << "Concurrent took: " << seconds << " seconds" << std::endl;
}
