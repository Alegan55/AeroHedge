#include "aerohedge/memory_allocator.hpp"
#include <cstdlib>
#include <cstdio>
#include <new>

void* operator new(std::size_t size) {
    if (aerohedge::MemoryEnforcer::is_locked()) {
        std::fprintf(stderr, "FATAL: Heap allocation of %zu bytes on critical path! Aborting.\n", size);
        std::abort();
    }
    
    void* ptr = std::malloc(size);
    if (!ptr) {
        throw std::bad_alloc{};
    }
    return ptr;
}

void* operator new[](std::size_t size) {
    if (aerohedge::MemoryEnforcer::is_locked()) {
        std::fprintf(stderr, "FATAL: Heap array allocation of %zu bytes on critical path! Aborting.\n", size);
        std::abort();
    }
    
    void* ptr = std::malloc(size);
    if (!ptr) {
        throw std::bad_alloc{};
    }
    return ptr;
}

void operator delete(void* ptr) noexcept {
    std::free(ptr);
}

void operator delete(void* ptr, std::size_t /*size*/) noexcept {
    std::free(ptr);
}

void operator delete[](void* ptr) noexcept {
    std::free(ptr);
}

void operator delete[](void* ptr, std::size_t /*size*/) noexcept {
    std::free(ptr);
}
