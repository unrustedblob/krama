// This is the implementation of the Arena interface. A single arena with
// chaining blocks.

#include "./arena.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

constexpr size_t KB = 1024;
constexpr size_t MB = KB * 1024;
constexpr size_t BLOCK_INIT_CAP = 64 * KB;
constexpr size_t BLOCK_MAX_CAP = 5 * MB;
constexpr size_t GROWTH_FACTOR = 2;

// A simple list node for a block of data
struct Block_ {
        struct Block_ *next;
        unsigned char *buffer;
        size_t cap;
};

// This is the header for the arena. `cursor` and `available` both refer to the
// block that head points to i.e, the current buffer being used for allocation
struct Arena {
        struct Block_ *head;
        unsigned char *cursor;
        size_t available;
};

[[noreturn]] static void fail_alloc();
static struct Arena *arena_create_(size_t cap);
static struct Block_ *block_create_(size_t cap);
static bool check_alignment_(size_t align);
static void move_cursor_(struct Arena *restrict arena, size_t size);
static void add_block_(struct Arena *restrict arena, size_t min_sz);
static void make_align_(struct Arena *restrict arena, size_t align);
static void *arena_alloc_(struct Arena *restrict arena, size_t size, size_t align);

// TODO: Should go into its own module
[[noreturn]] static void fail_alloc()
{
        fprintf(stderr, "Unable to allocate memory.\n");
        exit(1);
}

struct Arena *arena_create(void)
{
        return arena_create_(BLOCK_INIT_CAP);
}

// Creates the arena header, the block header and the buffer in that order.
// This should be called only once per arena needed.
static struct Arena *arena_create_(size_t cap)
{
        assert(cap <= BLOCK_MAX_CAP);
        struct Arena *arena = malloc(sizeof(struct Arena));
        if (!arena) {
                fail_alloc();
        }
        struct Block_ *block = block_create_(cap);
        arena->head = block;
        arena->cursor = block->buffer;
        arena->available = block->cap;

        return arena;
}

// Creates a Block_ on the heap and its associated buffer
static struct Block_ *block_create_(size_t cap)
{
        struct Block_ *block = malloc(sizeof(struct Block_));
        if (!block) {
                fail_alloc();
        }

        void *buffer = malloc(cap);
        if (!buffer) {
                fail_alloc();
        }

        block->next = nullptr;
        block->buffer = buffer;
        block->cap = cap;

        return block;
}

// TODO: This should eventually be deprecated/removed, callers should pass in
// requried alignment.
void *arena_alloc(struct Arena *restrict arena, size_t size)
{
        return arena_alloc_(arena, size, alignof(max_align_t));
}

// TODO; Eventually should become the exposed API not a helper.
static void *arena_alloc_(struct Arena *restrict arena, size_t size, size_t align)
{
        assert(check_alignment_(align)); // Must be power of 2
        make_align_(arena, align);
        if (arena->available < size) {
                add_block_(arena, size);
        }
        void *p = arena->cursor;
        move_cursor_(arena, size);
        return p;
}

// Adds a block to the block list. Computes a feasible size for the new block
// based on the requested size, this stops at BLOCK_MAX_CAP. The new block is
// prepended to the existing one.
static void add_block_(struct Arena *restrict arena, size_t min_sz)
{
        if (min_sz > BLOCK_MAX_CAP) {
                fail_alloc();
        }

        // Find the required size for new buffer to malloc beforehand, caps off
        // at BLOCK_MAX_CAP
        size_t next_cap = arena->head->cap * 2;
        while (next_cap < min_sz) {
                next_cap *= 2;
        }
        size_t final_cap = (next_cap > BLOCK_MAX_CAP) ? BLOCK_MAX_CAP : next_cap;

        struct Block_ *block = block_create_(final_cap);

        block->next = arena->head->next;
        arena->head = block;
        arena->cursor = block->buffer;
        arena->available = block->cap;
}

// TODO: Implement this function perhaps as part of milestone 2.
// This will be commented out in the header.
void *arena_reset(/* struct Arena *arena */)
{
        return nullptr;
}

// Frees the arena, its contained chained blocks and their respective
// buffers in reverse order.
nullptr_t arena_destroy(struct Arena *restrict arena)
{
        while (arena->head) {
                free(arena->head->buffer);
                void *p = arena->head;
                arena->head = arena->head->next;
                free(p);
        }
        arena->cursor = nullptr;
        free(arena);
        return nullptr;
}

static bool check_alignment_(size_t align)
{
        return (align & (align - 1)) == 0;
}

static void make_align_(struct Arena *restrict arena, size_t align)
{
        size_t padding = (0 - (uintptr_t)arena->cursor) & (align - 1);
        if (padding > arena->available) {
                add_block_(arena, padding);
        } else {
                move_cursor_(arena, padding);
        }
}

static void move_cursor_(struct Arena *restrict arena, size_t size)
{
        assert(size <= arena->available); // Not enough space to move cursor in block
        arena->cursor += size;
        arena->available -= size;
}

#ifdef FUNC_TEST_SUITE

const unsigned char *arena_get_cursor(const struct Arena *const arena)
{
        return arena->cursor;
}

const unsigned char *arena_get_buffer(const struct Arena *const arena)
{
        return arena->head->buffer;
}

size_t arena_get_available(const struct Arena *const arena)
{
        return arena->available;
}

size_t arena_get_capacity(const struct Arena *const arena)
{
        return arena->head->cap;
}

size_t arena_get_max_cap(void)
{
        return BLOCK_MAX_CAP;
}

size_t arena_get_init_cap(void)
{
        return BLOCK_INIT_CAP;
}

size_t arena_get_growth_factor(void)
{
        return GROWTH_FACTOR;
}
#endif // FUNC_TEST_SUITE
