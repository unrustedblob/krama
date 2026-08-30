// This is the implementation of the Arena interface. A single arena with
// chaining blocks.

#include "arena.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "./fatal.h"

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

static struct Arena *create_arena(size_t cap);
static struct Block_ *create_block(size_t cap);
static bool check_alignment(size_t align);
static void move_cursor(struct Arena *arena, size_t size);
static void add_block(struct Arena *arena, size_t min_sz);
static void make_align(struct Arena *arena, size_t align);
static void *allocate(struct Arena *arena, size_t size, size_t align);

struct Arena *arena_create(void)
{
        static_assert(BLOCK_INIT_CAP < BLOCK_MAX_CAP,
                      "Capacity request higher than BLOCK_MAX_CAP is invalid");
        return create_arena(BLOCK_INIT_CAP);
}

// Creates the arena header, the block header and the buffer in that order.
// This should be called only once per arena needed.
static struct Arena *create_arena(size_t cap)
{
        struct Arena *arena = malloc(sizeof(struct Arena));
        FATAL(arena == nullptr, FATAL_PATH_EXIT, "Unable to allocate memory");

        struct Block_ *block = create_block(cap);
        *arena = (struct Arena){
                .head = block,
                .cursor = block->buffer,
                .available = block->cap,
        };
        return arena;
}

// Creates a Block_ on the heap and its associated buffer
static struct Block_ *create_block(size_t cap)
{
        struct Block_ *block = malloc(sizeof(struct Block_));
        FATAL(block == nullptr, FATAL_PATH_EXIT, "Unable to allocate memory");

        void *buffer = malloc(cap);
        FATAL(buffer == nullptr, FATAL_PATH_EXIT, "Unable to allcoate memory");

        *block = (struct Block_){
                .next = nullptr,
                .buffer = buffer,
                .cap = cap,
        };
        return block;
}

// TODO: This should eventually be deprecated/removed, callers should pass in
// requried alignment.
void *arena_alloc(struct Arena *arena, size_t size)
{
        return allocate(arena, size, alignof(max_align_t));
}

// TODO; Eventually should become the exposed API not a helper.
static void *allocate(struct Arena *arena, size_t size, size_t align)
{
        FATAL(!check_alignment(align), FATAL_PATH_ABORT, "Alignment must be a power of 2. Got %zu",
              align);
        make_align(arena, align);
        if (arena->available < size) {
                add_block(arena, size);
        }
        void *p = arena->cursor;
        move_cursor(arena, size);
        return p;
}

// Adds a block to the block list. Computes a feasible size for the new block
// based on the requested size, capped at BLOCK_MAX_CAP. The new block is
// prepended to the existing one.
static void add_block(struct Arena *arena, size_t min_sz)
{
        FATAL(min_sz > BLOCK_MAX_CAP, FATAL_PATH_ABORT,
              "Requested size (%zu) greater than allowed maximum capacity (%zu)", min_sz,
              BLOCK_MAX_CAP);

        // Find the required size for new buffer to malloc beforehand, caps off
        // at BLOCK_MAX_CAP
        size_t next_cap = arena->head->cap * GROWTH_FACTOR;
        while (next_cap < min_sz) {
                next_cap *= 2;
        }
        size_t final_cap = (next_cap > BLOCK_MAX_CAP) ? BLOCK_MAX_CAP : next_cap;
        struct Block_ *block = create_block(final_cap);
        block->next = arena->head;
        *arena = (struct Arena){
                .head = block,
                .cursor = block->buffer,
                .available = block->cap,
        };
}

// TODO(D-026): Implement this function - expected past milestone 2.
void *arena_reset(struct Arena *arena);

// Frees the arena, its contained chained blocks and their respective
// buffers in reverse order.
nullptr_t arena_destroy(struct Arena *arena)
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

static bool check_alignment(size_t align)
{
        return (align & (align - 1)) == 0;
}

static void make_align(struct Arena *arena, size_t align)
{
        size_t padding = (0 - (uintptr_t)arena->cursor) & (align - 1);
        if (padding > arena->available) {
                add_block(arena, padding);
        } else {
                move_cursor(arena, padding);
        }
}

static void move_cursor(struct Arena *arena, size_t size)
{
        FATAL(size > arena->available, FATAL_PATH_ABORT,
              "Not enough space to move cursor. Requested: %zu | Available: %zu", size,
              arena->available);
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
