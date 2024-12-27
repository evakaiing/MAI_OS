#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <dlfcn.h>
#include <sys/mman.h>

typedef struct Allocator {
    void* (*allocator_create)(void* addr, size_t size);
    void* (*allocator_alloc)(void* allocator, size_t size);
    void (*allocator_free)(void* allocator, void* ptr);
    void (*allocator_destroy)(void* allocator);
} Allocator;

void* standard_allocator_create(void* memory, size_t size) {
    (void)size;
    (void)memory;
    return memory;
}

void* standard_allocator_alloc(void* allocator, size_t size) {
    uint32_t* memory = mmap(NULL, size + sizeof(uint32_t), PROT_READ | PROT_WRITE,
                            MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (memory == MAP_FAILED) {
        return NULL;
    }
    *memory = (uint32_t)(size + sizeof(uint32_t));
    return memory + 1;
}

void standard_allocator_free(void* allocator, void* memory) {
    if (memory == NULL) {
        return;
    }
    uint32_t* mem = (uint32_t*)memory - 1;
    munmap(mem, *mem);
}

void standard_allocator_destroy(void* allocator) {
    (void)allocator;
}

Allocator* load_allocator(const char* library_path) {
    if (library_path == NULL || library_path[0] == '\0') {
        char message[] = "WARNING: failed to load shared library\n";
        write(STDERR_FILENO, message, sizeof(message) - 1);
        Allocator* allocator = malloc(sizeof(Allocator));
        allocator->allocator_create = standard_allocator_create;
        allocator->allocator_alloc = standard_allocator_alloc;
        allocator->allocator_free = standard_allocator_free;
        allocator->allocator_destroy = standard_allocator_destroy;
        return allocator;
    }

    void* library = dlopen(library_path, RTLD_LOCAL | RTLD_NOW);
    if (!library) {
        char message[] = "WARNING: failed to load shared library\n";
        const char* error_message = dlerror();
        fprintf(stderr, "ERROR: dlopen failed: %s\n", error_message);
        write(STDERR_FILENO, message, sizeof(message) - 1);
        Allocator* allocator = malloc(sizeof(Allocator));
        allocator->allocator_create = standard_allocator_create;
        allocator->allocator_alloc = standard_allocator_alloc;
        allocator->allocator_free = standard_allocator_free;
        allocator->allocator_destroy = standard_allocator_destroy;
        return allocator;
    }

    Allocator* allocator = malloc(sizeof(Allocator));
    allocator->allocator_create = dlsym(library, "allocator_create");
    allocator->allocator_alloc = dlsym(library, "allocator_alloc");
    allocator->allocator_free = dlsym(library, "allocator_free");
    allocator->allocator_destroy = dlsym(library, "allocator_destroy");

    if (!allocator->allocator_create || !allocator->allocator_alloc || 
        !allocator->allocator_free || !allocator->allocator_destroy) {
        const char msg[] = "Error: failed to load all allocator functions\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        free(allocator);
        dlclose(library);
        return NULL;
    }

    return allocator;
}

int main(int argc, char** argv) {
    const char* library_path = (argc > 1) ? argv[1] : NULL;
    Allocator* allocator_api = load_allocator(library_path);
    if (!allocator_api) {
        char message[] = "Failed to load allocator API\n";
        write(STDERR_FILENO, message, sizeof(message) - 1);
        return EXIT_FAILURE;
    }

    size_t size = 512;
    void* addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (addr == MAP_FAILED) {
        char message[] = "mmap failed\n";
        write(STDERR_FILENO, message, sizeof(message) - 1);
        free(allocator_api);
        return EXIT_FAILURE;
    }

    void* allocator = allocator_api->allocator_create(addr, size);
    if (!allocator) {
        char message[] = "Failed to initialize allocator\n";
        write(STDERR_FILENO, message, sizeof(message) - 1);
        munmap(addr, size);
        free(allocator_api);
        return EXIT_FAILURE;
    }

    char start_message[] = "Allocator initialized\n";
    write(STDOUT_FILENO, start_message, sizeof(start_message) - 1);

    void* block1 = allocator_api->allocator_alloc(allocator, 64);
    void* block2 = allocator_api->allocator_alloc(allocator, 40);

    if (block1 == NULL || block2 == NULL) {
        char alloc_fail_message[] = "Memory allocation failed\n";
        write(STDERR_FILENO, alloc_fail_message, sizeof(alloc_fail_message) - 1);
    } else {
        char alloc_success_message[] = "Memory allocated successfully\n";
        write(STDOUT_FILENO, alloc_success_message, sizeof(alloc_success_message) - 1);
    }

    char buffer[64];
    snprintf(buffer, sizeof(buffer), "Block 1 address: %p\n", block1);
    write(STDOUT_FILENO, buffer, strlen(buffer));
    snprintf(buffer, sizeof(buffer), "Block 2 address: %p\n", block2);
    write(STDOUT_FILENO, buffer, strlen(buffer));

    allocator_api->allocator_free(allocator, block1);
    allocator_api->allocator_free(allocator, block2);

    char free_message[] = "Memory freed\n";
    write(STDOUT_FILENO, free_message, sizeof(free_message) - 1);

    allocator_api->allocator_destroy(allocator);
    free(allocator_api);
    munmap(addr, size);

    char exit_message[] = "Program exited successfully\n";
    write(STDOUT_FILENO, exit_message, sizeof(exit_message) - 1);

    return EXIT_SUCCESS;
}
