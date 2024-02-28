
# jsmalloc

jsmalloc is both a general purpose malloc(3) implementation and optimized implementation, tailored to be used in ZGC, a garbage collector in the [HotSpot JVM]("https://en.wikipedia.org/wiki/HotSpot_(virtual_machine)"). jsmalloc is largely based on the Two-Level Segregated Fit (TLSF) allocator by M. Masmano et al, especially in the general purpose version of the allocator. However, the optimized version for ZGC differs in several ways from the base TLSF design.

TLSF Reference:
```
M. Masmano, I. Ripoll, A. Crespo and J. Real
TLSF: a new dynamic memory allocator for real-time systems
Proceedings. 16th Euromicro Conference on Real-Time Systems, 2004. ECRTS 2004., Catania, Italy, 2004, pp. 79-88, doi: 10.1109/EMRTS.2004.1311009.
```

The implementation is written in C++14 for 64-bit machines exclusively and uses some compiler intrinsics (`__builtin_ffsl`, `__builtin_clzl`).

The name jsmalloc is not associated with JavaScript in any way but is taken from the author's initials.

## Real-World Testing

To easily test the allocator in practice using real-world programs we provide a wrapper around malloc/calloc/realloc/free that can be compiled to a shared library.
```bash
make sharedlib # Produces the file libjsmalloc.so
LD_PRELOAD=./libjsmalloc.so ./<some program>
```

In some cases it might also be interested/useful to log the distribution of allocation requests. This can be done by setting the `LOG_ALLOC` environment variable to a file in which the allocations should be written to.
```bash
LOG_ALLOC=output.txt LD_PRELOAD=./libjsmalloc.so ./<some program>
```

## Author
Joel Sikström
