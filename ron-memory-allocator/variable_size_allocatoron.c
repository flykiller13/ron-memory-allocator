/**
 * A simple variable-size memory allocator using a doubly linked list.
 *
 * - Statically allocated pool
 * - Best-fit allocation strategy to minimize fragmentation
 * - Bidirectional coalescing on deallocation
 * - In-place realloc when possible (shrink/expand)
 * - O(n) allocation (traverses list), O(1) deallocation
 *
 * Operates on a fixed memory pool without calling malloc/free.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define POOL_SIZE 256 // Size of the memory pool in bytes

/**
 * Memory pool - Aligned to 8 bytes.
 * The pool is initialized by vs_init_allocator.
 *
 * 8-byte alignment ensures compatibility with all data types:
 * - Satisfies alignment requirements for 64-bit pointers and doubles
 * - Prevents crashes on strict-alignment architectures (ARM, SPARC)
 * - Avoids performance penalties on x86/x64 from misaligned access
 *
 * Uses 'char' type (exactly 1 byte) to enable byte-level pointer arithmetic.
 */
static char memory_pool[POOL_SIZE] __attribute__((aligned(8)));

/**
 * Represents a memory block within the allocator.
 *
 * Each block is doubly linked to other blocks.
 * This structure allows efficient allocation, deallocation, and block coalescing.
 *
 * Fields:
 * - `size`: The size of the memory block in bytes.
 * - `used`: Indicates whether the block is in use (1 for used, 0 for free).
 * - `prev`: A pointer to the previous memory block in the linked list.
 * - `next`: A pointer to the next memory block in the linked list.
 */
typedef struct MemBlock
{
    size_t size;
    int used;
    struct MemBlock* prev;
    struct MemBlock* next;
} MemBlock;

/**
 * Head to a doubly linked list of memory blocks
 */
MemBlock* head = NULL; // Head of memory block list

/**
 * Initializes the variable size allocator.
 *
 * One free memory block is created that spans across the memory pool.
 */
void vs_init_allocator()
{
    printf("Initializing Allocator\n");
    // Initialize the pool as one big free block
    head = (MemBlock*)memory_pool;
    head->size = POOL_SIZE;
    head->used = 0;
    head->prev = NULL;
    head->next = NULL;
}

/**
 * Allocates a block of memory.
 *
 * @param size The size of the memory to allocate. A 'best-fit' allocation strategy is used.
 * @return A pointer to the allocated usable memory.
 */
void* vs_malloc(size_t size)
{
    // Best-fit search: Finds the smallest free block that can satisfy the size.
    // Minimizes wasted space and reduces external fragmentation.
    MemBlock* curr = head;
    MemBlock* best = NULL;
    while (curr)
    {
        // The block must be free, large enough and smaller than the current best
        if ((!curr->used) && (!best || (curr->size < best->size) && (curr->size >= size)))
        {
            best = curr;
        }
        curr = curr->next;
    }

    // No suitable block found, or allocation would exceed pool bounds
    if (!best || (char*)best + sizeof(MemBlock) + size > memory_pool + POOL_SIZE)
    {
        printf("Out of memory\n");
        return NULL;
    }

    // Split block if the block can fit the payload and block header
    if (best->size >= size + sizeof(MemBlock))
    {
        // Split - Allocate the requested size and create a free block from the remainder
        MemBlock* next = best->next; // Preserve the next block before splitting
        best->next = (MemBlock*)((char*)best + sizeof(MemBlock) + size);

        // Update the next block
        best->next->used = 0;
        best->next->size = best->size - size - sizeof(MemBlock);
        best->next->prev = best;
        best->next->next = next;
    }

    best->used = 1; // Mark the block as used
    best->size = size; // Update the size

    return (char*)best + sizeof(MemBlock);
}

/**
 * Frees an allocated block of memory.
 *
 * @param ptr A pointer to the memory that should be freed
 */
void vs_free(void* ptr)
{
    if (!ptr)
    {
        printf("Invalid pointer\n");
        return;
    }

    // Rewind pointer to access header that precedes the payload
    MemBlock* block = (MemBlock*)((char*)ptr - sizeof(MemBlock));

    // The block must exist, be marked used, and be within pool bounds
    if (!block || !block->used || block < head || (char*)block > memory_pool + POOL_SIZE)
    {
        printf("Invalid pointer\n");
        return;
    }

    block->used = 0; // Mark the block as free

    // Coalescing - Merge with adjacent free blocks to reduce fragmentation.
    // The next block is merged first, then the previous to maintain pointers.

    // Merge with the NEXT block
    if (block->next && !block->next->used)
    {

        block->size += block->next->size + sizeof(MemBlock);
        block->next = block->next->next;
        if (block->next)
            block->next->prev = block;
    }

    // Merge with the PREVIOUS block (Current block is absorbed)
    if (block->prev && !block->prev->used)
    {
        block->prev->size += block->size + sizeof(MemBlock);
        block->prev->next = block->next;
        if (block->next)
            block->next->prev = block->prev;
    }
}

/**
 * Reallocates an allocated block of memory to a new size.
 * The allocator tries to shrink/expand the memory in-place to save time.
 * If it can't, it copies the data using memcpy and frees the old block.
 *
 * @param ptr A pointer to the memory that will be reallocated.
 * @param new_size The new size of the allocated memory.
 * @return A pointer to the usable reallocated memory.
 */
void* vs_realloc(void* ptr, size_t new_size)
{
    // Pointer validation
    if (!ptr)
    {
        return vs_malloc(new_size);
    }
    if (new_size <= 0)
    {
        vs_free(ptr); // POSIX behavior
        return NULL;
    }

    // Rewind pointer to access header that precedes the payload
    MemBlock* block = (MemBlock*)(ptr - (void*)sizeof(MemBlock));

    // Shrink in place
    if (new_size < block->size)
    {
        // A remainder block is created only if its large enough to be useful
        if (block->size - new_size >= sizeof(MemBlock))
        {
            // Split off remaining excess space as a new free block
            MemBlock* rem = (MemBlock*)((char*)block + sizeof(MemBlock) + new_size);
            rem->size = block->size - new_size - sizeof(MemBlock);
            rem->used = 0;
            rem->prev = block;
            rem->next = block->next;
            if (rem->next)
                rem->next->prev = rem;
            block->next = rem;
        }
        block->size = new_size;
        return ptr;
    }

    // Expand - Try in-place if the next block is free and large enough
    if (block->next && !block->next->used &&
        block->size + block->next->size + sizeof(MemBlock) >= new_size)
    {
        // Expand to create 1 big block
        size_t excess = block->next->size + sizeof(MemBlock);
        block->size += excess;
        block->next = block->next->next;
        if (block->next)
            block->next->prev = block;

        // Split off remaining excess space as a new free block
        MemBlock* rem = (MemBlock*)((char*)block + sizeof(MemBlock) + new_size);
        rem->size = block->size - new_size - sizeof(MemBlock);
        rem->used = 0;
        rem->prev = block;
        rem->next = block->next;
        if (rem->next)
            rem->next->prev = rem;
        block->next = rem;

        // Update the block to the requested size
        block->size = new_size;
        block->used = 1;

        return ptr;
    }

    // Fallback - Copy data and free old block
    void* new_ptr = vs_malloc(new_size);
    if (new_ptr)
    {
        memcpy(new_ptr, ptr, block->size); // Copy the data
        vs_free(ptr); // Free the old block
    }

    return new_ptr;
}

/**
 * Prints a list of blocks, their sizes and free/used status
 */
void dump_memory()
{
    printf("Memory Dump:\n");
    MemBlock* curr = head;
    while (curr)
    {
        printf("\tBlock at %p, size %zu, used %d\n", curr, curr->size, curr->used);
        curr = curr->next;
    }
    printf("End Memory Dump\n");
}

/**
 * The main function initializes the allocator and acts as a test suite.
 *
 * The implemented tests are:
 * - Allocation until out of memory
 * - Freeing all memory
 * - Double Free
 * - Invalid pointers
 * - Coalescing
 * - Realloc
 */
int main()
{
    // Initialize allocator
    vs_init_allocator();
    printf("Initial State:\n");
    dump_memory();

    printf("\nTest: Allocate until out of memory\n");
    void* blocks[10000]; // store pointers so we can free them later
    size_t count = 0;
    while (1)
    {
        void* ptr = vs_malloc(16);
        if (!ptr)
        {
            printf("Out of memory after %zu successful allocations\n", count);
            break;
        }
        blocks[count++] = ptr;
    }
    dump_memory(); // Should print full memory

    printf("\nTest: Free all blocks\n");
    for (size_t i = 0; i < count; i++)
    {
        vs_free(blocks[i]);
    }
    dump_memory(); // Should print empty memory

    printf("\nTest: Double-free\n");
    void* ptr = vs_malloc(8);
    vs_free(ptr);
    vs_free(ptr); // Should print "Block already free"
    dump_memory();

    printf("\nTest: Invalid pointers\n");
    vs_free(NULL);
    vs_free((void*)(memory_pool + 7)); // Unaligned
    vs_free((void*)(memory_pool + POOL_SIZE + 1)); // Out of bounds

    printf("\nTest: Coalescing\n");
    void* a = vs_malloc(8);
    void* b = vs_malloc(16);
    void* c = vs_malloc(48);
    vs_free(a);
    vs_free(c);
    dump_memory(); // Should print 3 blocks
    vs_free(b);
    dump_memory(); // Should print 1 free block

    printf("\nTest: Reallocate memory\n");
    void* d = vs_realloc(NULL, 16); // Should allocate
    void* e = vs_realloc(d, 48); // Should expand
    if (!e)
    {
        printf("Realloc failed\n");
    }
    dump_memory();
    void* f = vs_realloc(e, 8); // Should shrink
    dump_memory();
    vs_realloc(f, 0); // Should free
    dump_memory();

    return 0;
}
