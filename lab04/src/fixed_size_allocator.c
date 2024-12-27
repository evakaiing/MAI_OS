#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MIN_BLOCK_SIZE 16      
#define MAX_BLOCK_SIZE 1024    

typedef struct FreeBlock {
    struct FreeBlock* next; 
} FreeBlock;

typedef struct Allocator {
    FreeBlock* free_lists[10]; 
    void* memory_start;       
    size_t total_memory;       
    size_t current_position;    
} Allocator;

int get_free_list_index(size_t size) {
    int index = 0;
    size_t current_size = MIN_BLOCK_SIZE;
    while (current_size < size && current_size <= MAX_BLOCK_SIZE) {
        current_size *= 2;
        index++;
    }

    return index;
}

Allocator* allocator_create(void* const memory, const size_t size) {
    if (size < MIN_BLOCK_SIZE || size > MAX_BLOCK_SIZE) {
        fprintf(stderr, "Error: Memory size must be between %d and %d bytes.\n",
                MIN_BLOCK_SIZE, MAX_BLOCK_SIZE);
        return NULL;
    }

    Allocator* allocator = (Allocator*)memory;
    allocator->memory_start = (char*)memory + sizeof(Allocator);
    allocator->total_memory = size - sizeof(Allocator);
    allocator->current_position = 0; 

    memset(allocator->free_lists, 0, sizeof(allocator->free_lists));
    return allocator;
}


void allocator_destroy(Allocator* const allocator) {
    if (!allocator) {
        return;
    }
    allocator->memory_start = NULL;
    allocator->total_memory = 0;

    memset(allocator->free_lists, 0, sizeof(allocator->free_lists));
}

void* allocator_alloc(Allocator* const allocator, const size_t size) {
    if (!allocator || size == 0 || size > MAX_BLOCK_SIZE) {
        return NULL;
    }

    int index = get_free_list_index(size);

    if (!allocator->free_lists[index]) {
        
        size_t block_size = MIN_BLOCK_SIZE << index;
        if (allocator->current_position + block_size > allocator->total_memory) {
            fprintf(stderr, "Error: Not enough memory in allocator.\n");
            return NULL;
        }

        void* block = (char*)allocator->memory_start + allocator->current_position;
        allocator->current_position += block_size;

        return block;
    }
    FreeBlock* block = allocator->free_lists[index];
    allocator->free_lists[index] = block->next;

    return (void*)block;
}


void allocator_free(Allocator* const allocator, void* const memory) {
    if (!allocator || !memory) {
        return;
    }

    size_t size = ((FreeBlock*)memory)->next ? MAX_BLOCK_SIZE : MIN_BLOCK_SIZE;
    int index = get_free_list_index(size);

    FreeBlock* block = (FreeBlock*)memory;
    block->next = allocator->free_lists[index];
    allocator->free_lists[index] = block;
}
