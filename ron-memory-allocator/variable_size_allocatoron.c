#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define POOL_SIZE (64 * 4)
static char memory_pool[POOL_SIZE] __attribute__((aligned(8)));

// Header for a memory block
typedef struct MemBlock
{
    size_t size;
    int used;
    struct MemBlock* prev;
    struct MemBlock* next;
} MemBlock;

MemBlock* head = NULL; // Head of memory block list

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

void* vs_malloc(size_t size)
{
    // Find best fit free block to reduce fragmentation
    MemBlock* curr = head;
    MemBlock* best = NULL;
    while (curr)
    {
        if ((!curr->used) && (!best || (curr->size < best->size) && (curr->size >= size)))
        {
            best = curr;
        }
        curr = curr->next;
    }

    if (!best || (char*)best + sizeof(MemBlock) + size > memory_pool + POOL_SIZE)
    {
        printf("Out of memory\n");
        return NULL;
    }

    if (best->size >= size + sizeof(MemBlock)) // Enough space for split
    {
        // Split block into a block of the requested size and a free block
        MemBlock* next = best->next; // Preserve the next block before splitting
        best->next = (MemBlock*)((char*)best + sizeof(MemBlock) + size);

        best->next->used = 0;
        best->next->size = best->size - size - sizeof(MemBlock);
        best->next->prev = best;
        best->next->next = next;
    }

    best->used = 1;
    best->size = size;

    return (char*)best + sizeof(MemBlock);
}

void vs_free(void* ptr)
{
    if (!ptr)
    {
        printf("Invalid pointer\n");
        return;
    }
    MemBlock* block = (MemBlock*)((char*)ptr - sizeof(MemBlock));
    if (!block || !block->used || block < head || (char*)block > memory_pool + POOL_SIZE)
    {
        printf("Invalid pointer\n");
        return;
    }
    block->used = 0; // Mark the block as free

    // Merge with the next block if possible - Coalescing
    if (block->next && !block->next->used)
    {
        block->size += block->next->size + sizeof(MemBlock);
        block->next = block->next->next;
        if (block->next)
            block->next->prev = block;
    }

    // Merge with the previous block if possible
    if (block->prev && !block->prev->used)
    {
        block->prev->size += block->size + sizeof(MemBlock);
        block->prev->next = block->next;
        if (block->next)
            block->next->prev = block->prev;
    }
}

void* vs_realloc(void* ptr, size_t new_size)
{
    if (!ptr)
    {
        return vs_malloc(new_size);
    }
    if (new_size <= 0)
    {
        vs_free(ptr); // POSIX behavior
        return NULL;
    }

    MemBlock* block = (MemBlock*)(ptr - (void*)sizeof(MemBlock));

    if (new_size < block->size) // Shrink in place
    {
        if (block->size - new_size >= sizeof(MemBlock))
        {
            // Split remainder
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

    if (block->next && !block->next->used &&
        block->size + block->next->size + sizeof(MemBlock) >= new_size) // Expand in place
    {
        // Expand to create 1 big block
        size_t excess = block->next->size + sizeof(MemBlock);
        block->size += excess;
        block->next = block->next->next;
        if (block->next)
            block->next->prev = block;

        // Split block
        MemBlock* rem = (MemBlock*)((char*)block + sizeof(MemBlock) + new_size);
        rem->size = block->size - new_size - sizeof(MemBlock);
        rem->used = 0;
        rem->prev = block;
        rem->next = block->next;
        if (rem->next)
            rem->next->prev = rem;

        block->next = rem;
        block->size = new_size;
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

// Prints a list of blocks, their sizes and free/used status
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

int main()
{
    // Initialize allocator
    vs_init_allocator();
    printf("Initial State:\n");
    dump_memory();

    void* blocks[10000]; // store pointers so we can free them later if needed
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

    printf("\nTest: Realloc\n");
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
