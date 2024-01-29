# Two Level Segregated Fit (TLSF)

Reference:
```
M. Masmano, I. Ripoll, A. Crespo and J. Real
TLSF: a new dynamic memory allocator for real-time systems
Proceedings. 16th Euromicro Conference on Real-Time Systems, 2004. ECRTS 2004., Catania, Italy, 2004, pp. 79-88, doi: 10.1109/EMRTS.2004.1311009.
```

The implementation is written in C++14 for 64-bit machines exclusively and uses some compiler intrinsics (`__builtin_ffsl`, `__builtin_clzl`).

## Real-World Testing

To easily test the allocator in practice using real-world programs we have provided a wrapper around malloc/calloc and free (no realloc, strdup, etc..).
To compile a shared library which can be preloaded when running a program, run:
```bash
make sharedlib # Produces a file called libtlsf.so
LD_PRELOAD=./libtlsf.so ./<some program>
```

## TODO

Print free-lists

Fragmentation metrics
    - Overhead due to block header
    - Internal fragmentation (wasted space du to alignment)
    - External fragmentation [calc](https://stackoverflow.com/questions/4586972/how-to-calculate-fragmentation).


## Author
Joel Sikström
