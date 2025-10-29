
#include <stdlib.h>
#include <stdio.h>

#define POOL_SIZE (16 * 3)
#define BLOCK_SIZE 16

#define BLOCK_COUNT (POOL_SIZE / BLOCK_SIZE)

typedef struct FreeBlock
{
    int used;
    struct FreeBlock* next;
} FreeBlock;

char memory_pool[POOL_SIZE] __attribute__((aligned(8)));
FreeBlock* free_list = (FreeBlock*)memory_pool;

void fs_init_allocator()
{
    // Walks over the memory pool and initializes the free list
    FreeBlock* curr_block = free_list;

    if (POOL_SIZE % BLOCK_SIZE != 0 || POOL_SIZE < BLOCK_SIZE || BLOCK_SIZE <= 0 || BLOCK_COUNT <= 0 || BLOCK_SIZE % 8 != 0)
    {
        printf("Invalid pool size\n");
        printf("Block size must be aligned to 8\n");
        return;
    }

    for (int i = 0; i < BLOCK_COUNT - 1; i++)
    {
        curr_block->next = (FreeBlock*)(memory_pool + BLOCK_SIZE * (i + 1));
        curr_block = curr_block->next;
    }
    curr_block->used = 0;
    curr_block->next = NULL;
}

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
    block->used = 1; // Mark as used
    free_list = free_list->next; // Advance the head

    return (void*)block;
}

void fs_free(void* ptr)
{
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

// Prints a list of blocks, their sizes and free/used status
void dump_memory()
{
    printf("Memory Dump:\n");

    for (int i = 0; i < BLOCK_COUNT; i++)
    {
        FreeBlock* block = (FreeBlock*)(memory_pool + BLOCK_SIZE * i);
        printf("Block at %p, size %d, used %d\n", block, BLOCK_SIZE, block->used);
    }
}

int main()
{
    // Initialize allocator
    fs_init_allocator();
    printf("Initial State:\n");
    dump_memory();

    printf("\nTest: Allocate until out of memory\n");
    void* blocks[BLOCK_COUNT + 1];
    // One extra iteration to trigger 'Out of memory'
    for (int i = 0; i <= BLOCK_COUNT; i++) {
        blocks[i] = fs_malloc(8);
        if (blocks[i])
            printf("Allocated block %d: %p\n", i, blocks[i]);
    }
    dump_memory(); // Should print full memory
    printf("\nTest: Free all blocks\n");
    for (int i = 0; i <= BLOCK_COUNT; i++) {
        fs_free(blocks[i]);
    }
    dump_memory(); // Should print empty memory

    printf("\nTest: Double-free\n");
    void* ptr = fs_malloc(8);
    fs_free(ptr);
    fs_free(ptr); // Should print "Block already free"
    dump_memory();

    printf("\nTest: Invalid pointers\n");
    fs_free(NULL);
    fs_free((void*)(memory_pool + 7)); // Unaligned
    fs_free((void*)(memory_pool + POOL_SIZE + 1)); // Out of bounds

    return 0;
}
