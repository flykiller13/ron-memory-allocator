Ron Memory Allocator
====================

Two custom memory allocators implemented in C:

- Fixed-size pool allocator (`ron-memory-allocator/fixed_size_allocatoron.c`)
- Variable-size allocator with splitting, coalescing, and best-fit (`ron-memory-allocator/variable_size_allocatoron.c`)

Both allocators operate over a statically allocated, 8-byte-aligned memory pool and do not call system allocators.

Contents
--------

- Overview
- Build
- Run
- Memory model and invariants
- Limitations
- Personal Key Takeaways

Overview
--------

The goal is to explore allocator design by re-implementing the core behaviors of `malloc`, `free`,
and `realloc` on top of a fixed-size buffer.

- Fixed-size allocator: O(1) allocate/free using a singly-linked free list of equal-sized blocks.
- Variable-size allocator: best-fit allocation, block splitting, bidirectional coalescing, and a
`realloc` that can shrink/expand in-place with a copy fallback.

Build
-----

Prereqs: CMake ≥ 3.28, a C11 compiler.

On Windows (from project root):

```bash
cmake -S . -B cmake-build-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build cmake-build-debug --config Debug
```

Outputs:

- `cmake-build-debug/fixed_size_allocatoron`
- `cmake-build-debug/variable_size_allocatoron`

Run
---

Each executable runs a self-test and prints allocator state transitions to stdout.

```bash
cmake-build-debug/fixed_size_allocatoron
cmake-build-debug/variable_size_allocatoron
```

Memory model and invariants
---------------------------

- Pools are static arrays aligned to 8 bytes.
- Fixed-size: `BLOCK_SIZE` divides `POOL_SIZE`. Allocations must not exceed `BLOCK_SIZE`.
- Variable-size: block headers precede payloads.
Best-fit search, split on surplus, coalesce with adjacent free blocks on `free`.
- All client pointers must originate from the allocator. Alignment and bounds are validated on `free`.

Limitations
-----------

- Pools are fixed at compile time. No growth via `sbrk`/`mmap`.
- No thread safety.
- Simulators intended for learning/testing. Not drop-in replacements for libc allocators.

Personal Key Takeaways
---------------------

Working on these allocators taught me that memory management, like many things in life, is all about trade-offs.
Every step of the way, I had to make trade-offs between speed and memory efficiency.

> #### Allocator design is contextual:
>
> The fixed-size allocator is ideal when allocation sizes are predictable,
> minimizing wasted space and avoiding fragmentation.
> 
> The variable-size allocator offers flexibility but requires careful handling.
> Reckless allocation can lead to fragmentation.

> #### Metadata is part of the trade-off:
>
> What the header stores — size, flags, links — affects both speed and memory efficiency.
> 
> Adding more fields can save processing time but consumes precious bytes in embedded environments.
> 
> In this project, I added a 'used' flag to the header to avoid iterating over the free list
> to check if a block is free.

> #### Memory is fragile:
>
> Writing this helped me truly understand how easily memory corruption or buffer overflows can occur.
> Debugging showed how overwritten bytes distort neighboring headers,
> which clarified the effect of data corruption in real systems.

