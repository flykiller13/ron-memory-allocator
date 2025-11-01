/**
 *
 * A simple fixed-size memory allocator using a free list.
 *
 * - Statically allocated pool
 * - O(1) allocation and deallocation
 * - LIFO free list (last freed block is allocated first)
 * - No fragmentation (all blocks are identical size)
 * - Wasteful for allocations smaller than BLOCK_SIZE
 *
 * Operates on a fixed memory pool without calling malloc/free.
 */

#include <stdio.h> // Used for printf
#include <stdlib.h> // Used for size_t

#define BLOCK_SIZE 32 // Size of each block in bytes (must be >= sizeof(FreeBlock) and 8-byte aligned)
#define BLOCK_COUNT 8 // Number of blocks in the pool
#define POOL_SIZE (BLOCK_SIZE * BLOCK_COUNT) // Total pool size in bytes

/**
 * Memory pool - Aligned to 8 bytes.
 * The pool is initialized by fs_init_allocator.
 *
 * 8-byte alignment ensures compatibility with all data types:
 * - Satisfies alignment requirements for 64-bit pointers and doubles
 * - Prevents crashes on strict-alignment architectures (ARM, SPARC)
 * - Avoids performance penalties on x86/x64 from misaligned access
 *
 * Uses 'char' type (exactly 1 byte) to enable byte-level pointer arithmetic.
 */
char memory_pool[POOL_SIZE] __attribute__((aligned(8)));

/**
 * Represents a free block of memory.
 * Each block points to the next free block in the free list.
 *
 * The "used" flag increases header size but enables O(1) double-free detection.
 * Alternative approaches:
 * - No flag: Would require O(n) traversal of free_list to detect double-free
 * - Separate bitset: Would save in-block space but add external overhead
 */
typedef struct FreeBlock
{
    int used;
    struct FreeBlock* next;
} FreeBlock;

/**
 * A linked list of free blocks.
 * On allocation free blocks are removed from the list.
 * When a block is freed it is added to the list.
 */
FreeBlock* free_list = NULL;

/**
 * Initializes the fixed memory allocator.
 * The pool is pre-allocated with BLOCK_COUNT free blocks that are added to the free list.
 * The allocator checks that all sizes are valid and sets limits for its memory space.
 * @return 0 on successful initialization. 1 Otherwise.
 */
int fs_init_allocator()
{
    // Walks over the memory pool and initializes the free list
    FreeBlock* curr = (FreeBlock*)memory_pool;
    free_list = curr;

    if (POOL_SIZE < BLOCK_SIZE || BLOCK_SIZE <= 0 ||
        BLOCK_COUNT < 0 || BLOCK_SIZE % 8 != 0 ||
        BLOCK_SIZE <= sizeof(FreeBlock))
    {
        printf("Invalid pool size\n");
        printf("Block size must be aligned to 8\n");
        printf("Block size must be at least %zu bytes - size of FreeBlock\n", sizeof(FreeBlock));

        return 1;
    }

    // Initialize all blocks except the last one
    for (int i = 0; i < BLOCK_COUNT - 1; i++)
    {
        curr->used = 0;
        // Advance by BLOCK_SIZE bytes to the next block.
        curr->next = (FreeBlock*)(memory_pool + BLOCK_SIZE * (i + 1));
        curr = curr->next;
    }

    // Initialize last block
    curr->used = 0;
    curr->next = NULL;

    return 0;
}

/**
 * Allocates a block in the memory pool.
 * Allocation is done simply by removing a block from the free list.
 * @param size The size of the block to allocate. Since this is a fixed size memory allocator,
 * the size is used to make sure that the user doesn't try to allocate more than the block size.
 * @return A pointer to the usable allocated memory
 */
void* fs_malloc(size_t size)
{
    if (free_list == NULL) // Out of memory
    {
        printf("Out of memory\n");
        return NULL;
    }
    if (size > BLOCK_SIZE) // Too big
    {
        printf("The fixed size allocator can't allocate more than %d\n", BLOCK_SIZE);
        return NULL;
    }

    FreeBlock* block = free_list; // Get the head of the free list
    free_list = free_list->next; // Advance the head
    block->used = 1; // Mark as used
    block->next = NULL; // Clear next pointer

    return (void*)block;
}

/**
 * Frees a used block of memory.
 * Freeing a block is done by adding it back to the free list.
 * This is done in a LIFO manner where the freshly freed block is the new head of the free list.
 * @param ptr A pointer to the used memory
 */
void fs_free(void* ptr)
{
    // Validate pointer: must be non-NULL, within pool bounds, and block-aligned
    if (!ptr || (char*)ptr < memory_pool || (char*)ptr >= (memory_pool + POOL_SIZE)
        || ((char*)ptr - memory_pool) % BLOCK_SIZE != 0)
    {
        printf("Invalid pointer\n");
        return;
    }

    FreeBlock* block = (FreeBlock*)ptr;
    if (!block->used) // Double free detection
    {
        printf("Block already free\n");
        return;
    }

    block->used = 0; // Mark as free
    block->next = free_list; // Insert the new free block
    free_list = block; // Advance the head

}

/**
 * Prints a list of blocks, their sizes and free/used status
 */
void dump_memory()
{
    printf("Memory Dump:\n");

    for (int i = 0; i < BLOCK_COUNT; i++)
    {
        FreeBlock* block = (FreeBlock*)(memory_pool + BLOCK_SIZE * i);
        printf("\tBlock at %p, size %d, used %d\n", block, BLOCK_SIZE, block->used);
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
 */
int main()
{
    // Initialize allocator
    if (fs_init_allocator())
    {
        printf("ERROR: Failed to initialize allocator\n");
        return 1;
    }
    printf("Initial State:\n");
    dump_memory();

    printf("\nTest: Allocate until out of memory\n");
    void* blocks[BLOCK_COUNT + 1];
    // One extra iteration to trigger 'Out of memory'
    for (int i = 0; i <= BLOCK_COUNT; i++) {
        blocks[i] = fs_malloc(8);
        if (blocks[i])
            printf("\tAllocated block %d: %p\n", i, blocks[i]);
    }
    dump_memory(); // Should print full memory

    printf("\nTest: Free all blocks\n");
    for (int i = 0; i <= BLOCK_COUNT; i++) {
        fs_free(blocks[i]); // Last free should print 'Invalid pointer'
    }
    dump_memory(); // Should print empty memory

    printf("\nTest: Double-free\n");
    void* ptr = fs_malloc(8);
    fs_free(ptr);
    fs_free(ptr); // Should print "Block already free"

    printf("\nTest: Invalid pointers\n");
    fs_free(NULL);
    fs_free((void*)(memory_pool + 7)); // Unaligned
    fs_free((void*)(memory_pool + POOL_SIZE + 1)); // Out of bounds

    return 0;
}
