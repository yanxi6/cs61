#include "m61.hh"
#include <cstdlib>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <cinttypes>
#include <cassert>
#include <sys/mman.h>
#include <map>
#include <list>


struct m61_memory_buffer {
    char* buffer;
    size_t pos = 0;
    size_t size = 8 << 20; /* 8 MiB */

    m61_memory_buffer();
    ~m61_memory_buffer();
};


struct allocated_block {

    size_t pos = 0;
    size_t size = 0;

};

struct free_block {
    size_t pos = 0;
    size_t size = 0;

};

static m61_memory_buffer default_buffer;

static m61_statistics myStats = {0,0,0,0,0,0,0,0};
static std::map<void*, allocated_block> allocated_pool;

static std::list<free_block> free_pool(1, {0, 8 << 20} );



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


/// first_fit(sz, file, line)
size_t first_fit(size_t sz) {
    // tranverse free pool
    auto it = free_pool.begin();
    size_t block_size = std::max(sz, alignof(std::max_align_t));
    size_t ava_pos = -1;
    while(it != free_pool.end()) {
        if(block_size <= (*it).size) {
            break;
        }
        it++;

    }
    if(it == free_pool.end()) {
        return ava_pos;
    }

    ava_pos = (*it).pos;
    // update free_pool

    if(block_size == (*it).size) {
        free_pool.erase(it);
    } else {

        (*it).pos += block_size;
        (*it).size -= block_size;
    }
    return ava_pos;

}

// update 
void update_allocated_pool(void* ptr, size_t pos, size_t sz) {
    allocated_pool[ptr] = {pos, sz};

}

// update free_pool
// todo: have not updated allocated pool
void update_free_pool(void* ptr) {
    size_t pos = allocated_pool[ptr].pos;
    size_t sz = allocated_pool[ptr].size;
    auto it = free_pool.begin();

    while(it != free_pool.end()) {
        if(pos <= (*it).pos) {
            break;
        }
        it++;

    }
    
    if( it != free_pool.begin()) {
        auto prev = std::prev(it);
        if( (*prev).pos +  (*prev).size >= pos) {
            (*prev).size += sz;
        } else {
            free_pool.insert(it, {pos, sz});
        }
    } else {
        free_pool.insert(it, {pos, sz});
    }
    auto prev = std::prev(it);
    if((*prev).pos + (*prev).size >= (*it).pos) {
        (*it).pos = (*prev).pos;
        (*it).size += (*prev).size;
        free_pool.erase(prev);
    }
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
    // find ava pos
    size_t available_pos = first_fit(sz);
    if (available_pos == -1) {
        myStats.nfail++;
        myStats.fail_size += sz;
        return nullptr;
    }

    void* ptr = &default_buffer.buffer[available_pos];

    update_allocated_pool(ptr, available_pos, sz);
 
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

    update_free_pool(ptr);

    size_t sz = allocated_pool[ptr].size;

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
