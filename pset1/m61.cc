#include "m61.hh"
#include <cstdlib>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <cinttypes>
#include <cassert>
#include <sys/mman.h>
#include <map>


struct m61_memory_buffer {
    char* buffer;
    size_t pos = 0;
    size_t size = 8 << 20; /* 8 MiB */

    m61_memory_buffer();
    ~m61_memory_buffer();
};

static m61_memory_buffer default_buffer;

static m61_statistics myStats = {0,0,0,0,0,0,0,0};
static std::map<void*, size_t> allocation_pool;


m61_memory_buffer::m61_memory_buffer() {
    void* buf = mmap(nullptr,    // Place the buffer at a random address
        this->size,              // Buffer should be 8 MiB big
        PROT_WRITE,              // We want to read and write the buffer
        MAP_ANON | MAP_PRIVATE, -1, 0);
                                 // We want memory freshly allocated by the OS
    assert(buf != MAP_FAILED);
    this->buffer = (char*) buf;
}

m61_memory_buffer::~m61_memory_buffer() {
    munmap(this->buffer, this->size);
}


/// m61_malloc(sz, file, line)
///    Returns a pointer to `sz` bytes of freshly-allocated dynamic memory.
///    The memory is not initialized. If `sz == 0`, then m61_malloc may
///    return either `nullptr` or a pointer to a unique allocation.
///    The allocation request was made at source code location `file`:`line`.

void* m61_malloc(size_t sz, const char* file, int line) {
    (void) file, (void) line;   // avoid uninitialized variable warnings
    // Your code here.
    if ((double) default_buffer.pos + (double) sz > default_buffer.size) {
        // Not enough space left in default buffer for allocation
        myStats.nfail++;
        myStats.fail_size += sz;
        return nullptr;
    }

    // Otherwise there is enough space; claim the next `sz` bytes
    void* ptr = &default_buffer.buffer[default_buffer.pos];
    allocation_pool[ptr] = sz;
    if(sz < alignof(std::max_align_t)) {
        default_buffer.pos += alignof(std::max_align_t);

    } else {
        default_buffer.pos += sz;
    }


        
    myStats.nactive++;
    myStats.active_size += sz;
    myStats.ntotal++;
    myStats.total_size += sz;
    if (!myStats.heap_min || myStats.heap_min > (uintptr_t) ptr) {
        myStats.heap_min = (uintptr_t) ptr;
    }
    if (!myStats.heap_max || myStats.heap_max < (uintptr_t) ptr + sz) {
        myStats.heap_max = (uintptr_t) ptr + sz;
    }
    
    return ptr;
}


/// m61_free(ptr, file, line)
///    Frees the memory allocation pointed to by `ptr`. If `ptr == nullptr`,
///    does nothing. Otherwise, `ptr` must point to a currently active
///    allocation returned by `m61_malloc`. The free was called at location
///    `file`:`line`.

void m61_free(void* ptr, const char* file, int line) {
    // avoid uninitialized variable warnings
    (void) ptr, (void) file, (void) line;

    // Your code here. The handout code does nothing!
    if (ptr == nullptr) {

        return ;
    }

    size_t sz = allocation_pool[ptr];

    myStats.nactive--;
    myStats.active_size -= sz;
    if (!myStats.heap_min || myStats.heap_min > (uintptr_t) ptr) {
        myStats.heap_min = (uintptr_t) ptr;
    }
    if (!myStats.heap_max || myStats.heap_max < (uintptr_t) ptr + sz) {
        myStats.heap_max = (uintptr_t) ptr + sz;
    }
    // myStats.ntotal++;
    // myStats.total_size += sz;

}


/// m61_calloc(count, sz, file, line)
///    Returns a pointer a fresh dynamic memory allocation big enough to
///    hold an array of `count` elements of `sz` bytes each. Returned
///    memory is initialized to zero. The allocation request was at
///    location `file`:`line`. Returns `nullptr` if out of memory; may
///    also return `nullptr` if `count == 0` or `size == 0`.

void* m61_calloc(size_t count, size_t sz, const char* file, int line) {
    // Your code here (not needed for first tests).
    size_t total = count * sz;
    if(count == 0 || sz == 0 || (sz != 0 && total/sz != count)) {
        myStats.nfail++;
        myStats.fail_size += total;
        return nullptr;
    }
    void* ptr = m61_malloc(count * sz, file, line);
    if (ptr) {
        memset(ptr, 0, count * sz);
    }
    return ptr;
}


/// m61_get_statistics()
///    Return the current memory statistics.

m61_statistics m61_get_statistics() {
    // Your code here.
    // The handout code sets all statistics to enormous numbers.
    // m61_statistics stats;
    // memset(&stats, 255, sizeof(m61_statistics));
    return myStats;
}


/// m61_print_statistics()
///    Prints the current memory statistics.

void m61_print_statistics() {
    m61_statistics stats = m61_get_statistics();
    printf("alloc count: active %10llu   total %10llu   fail %10llu\n",
           stats.nactive, stats.ntotal, stats.nfail);
    printf("alloc size:  active %10llu   total %10llu   fail %10llu\n",
           stats.active_size, stats.total_size, stats.fail_size);
}


/// m61_print_leak_report()
///    Prints a report of all currently-active allocated blocks of dynamic
///    memory.

void m61_print_leak_report() {
    // Your code here.
}
