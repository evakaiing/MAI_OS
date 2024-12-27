#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

typedef struct BlockNode {
    size_t block_size;          
    int is_free;                
    struct BlockNode* left;     
    struct BlockNode* right;    
} BlockNode;

typedef struct Allocator {
    BlockNode* root;            
    void* memory_start;         
    size_t total_memory;        
    size_t current_position;    
} Allocator;

int is_power_of_two(size_t n) {
    if (n == 0) {
        return 0; 
    }
    while (n > 1) {
        if (n % 2 != 0) {
            return 0; 
        }
        n /= 2; 
    }
    return 1; 
}

BlockNode* create_block_node(Allocator* allocator, size_t block_size) {
    if (allocator->current_position + sizeof(BlockNode) > allocator->total_memory) {
        return NULL;
    }

    BlockNode* node = (BlockNode*)((char*)allocator->memory_start + allocator->current_position);
    allocator->current_position += sizeof(BlockNode);
    node->block_size = block_size;
    node->is_free = 1;
    node->left = node->right = NULL;
    return node;
}

Allocator* allocator_create(void* memory, const size_t size) {
    if (!is_power_of_two(size)) {
        const char* error_msg = "Allocator requires memory size to be a power of two\n";
        write(STDERR_FILENO, error_msg, strlen(error_msg));
        return NULL;
    }

    Allocator* allocator = (Allocator*)memory;
    allocator->memory_start = (char*)memory + sizeof(Allocator);
    allocator->total_memory = size - sizeof(Allocator);
    allocator->current_position = 0;

    allocator->root = create_block_node(allocator, size);
    if (!allocator->root) {
        return NULL;
    }

    return allocator;
}

void split_block(Allocator* allocator, BlockNode* node) {
    size_t half_size = node->block_size / 2;

    node->left = create_block_node(allocator, half_size);
    node->right = create_block_node(allocator, half_size);
}

BlockNode* recursive_allocation(Allocator* allocator, BlockNode* node, size_t size) {
    if (!node || node->block_size < size || !node->is_free) {
        return NULL;
    }

    if (node->block_size == size) {
        node->is_free = 0;
        return node;
    }

    if (!node->left) {
        split_block(allocator, node);
    }

    BlockNode* allocated_node = recursive_allocation(allocator, node->left, size);
    if (!allocated_node) {
        allocated_node = recursive_allocation(allocator, node->right, size);
    }

    node->is_free = (node->left && node->left->is_free) || (node->right && node->right->is_free);
    return allocated_node;
}

void* allocator_alloc(Allocator* allocator, const size_t size) {
    if (!allocator || size == 0) {
        return NULL;
    }

    size_t aligned_size = size;
    while (!is_power_of_two(aligned_size)) {
        aligned_size++;
    }

    BlockNode* block = recursive_allocation(allocator, allocator->root, aligned_size);
    return block ? (void*)block : NULL;
}

void allocator_free(Allocator* allocator, void* memory) {
    if (!allocator || !memory) {
        return;
    }

    BlockNode* block = (BlockNode*)memory;
    block->is_free = 1;

    if (block->left && block->left->is_free && block->right->is_free) {
        allocator_free(allocator, block->left);
        allocator_free(allocator, block->right);
        block->left = block->right = NULL;
    }
}

void allocator_destroy(Allocator* allocator) {
    if (!allocator) {
        return;
    }

    allocator_free(allocator, allocator->root);
    if (munmap(allocator, allocator->total_memory + sizeof(Allocator)) != 0) {
        const char* error_msg = "Failed to unmap memory\n";
        write(STDERR_FILENO, error_msg, strlen(error_msg));
        exit(EXIT_FAILURE);
    }
}
