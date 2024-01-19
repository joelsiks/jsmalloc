# Two Level Segregated Fit (TLSF)

Reference:
```
M. Masmano, I. Ripoll, A. Crespo and J. Real
TLSF: a new dynamic memory allocator for real-time systems
Proceedings. 16th Euromicro Conference on Real-Time Systems, 2004. ECRTS 2004., Catania, Italy, 2004, pp. 79-88, doi: 10.1109/EMRTS.2004.1311009.
```

The implementation is written in C++14 for 64-bit machines exclusively and uses some compiler intrinsics (`__builtin_ffsl`, `__builtin_clzl`).

## TODO

Implement free/last bits inside block length.

Implement some helper functions for masking out the F/T bits from the size to get the actual size of blocks.

Check alignment when in allocate()

Implement coalesce_blocks()

## Author
Joel Sikström
