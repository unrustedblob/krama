#include "../src/arena.h"

#include <stdio.h>

struct TestCounter {
        size_t passed;
        size_t total;
};

static_assert((sizeof(struct TestCounter) <= 16), "Struct size crossing pass-by-value limit");

static struct TestCounter test_arena(void);

#define CHECK(cond, test_counter, ...)                \
        do {                                          \
                if (!(cond)) {                        \
                        fprintf(stderr, "[FAIL] ");   \
                        fprintf(stderr, __VA_ARGS__); \
                        fprintf(stderr, "\n");        \
                } else {                              \
                        printf("[OK] ");              \
                        printf(__VA_ARGS__);          \
                        printf("\n");                 \
                        ++test_counter.passed;        \
                }                                     \
                ++test_counter.total;                 \
        } while (false)

static struct TestCounter test_arena(void)
{
        struct TestCounter counter = {};
        size_t block_max_cap = arena_get_max_cap();
        size_t block_init_cap = arena_get_init_cap();

        fprintf(stdout,
                "Testing: Arena Implementation\n"
                "  [INFO] Block initial capacity: %zu\n"
                "  [INFO] Block maximum capacity: %zu\n\n",
                block_init_cap, block_max_cap);

        // ---------------------------------------------------
        // 0 - Create the arena
        // ---------------------------------------------------

        struct Arena *arena = arena_create();

        CHECK((arena != nullptr), counter, "Creation of arena should return valid pointer");

        // ---------------------------------------------------
        // 1 - The usual case
        // Requeted allocation is within range.
        // ---------------------------------------------------

        size_t request_space = (size_t)(block_init_cap / 2);
        void *p = arena_alloc(arena, request_space);
        void *cursor = arena_get_cursor(arena);

        CHECK(((uintptr_t)p != (uintptr_t)cursor), counter,
              "After alloc, pointer returned and current Arena cursor position are not the same");

        CHECK((((uintptr_t)cursor - (uintptr_t)p) == request_space), counter,
              "After alloc, Arena cursor is ahead of pointer returned by exactly requested space "
              "%zu",
              request_space);

        size_t arena_cap = arena_get_capacity(arena);

        CHECK((arena_cap == block_init_cap), counter,
              "Arena capacity is the same as the fixed initial capacity. Expected %zu | Got %zu",
              arena_cap, block_init_cap);

        size_t arena_available = arena_get_available(arena);

        CHECK((arena_available == (arena_cap - request_space)), counter,
              "Available store in the arenas buffer is exactly capacity - requested space. "
              "Expected %zu | Got %zu",
              (arena_cap - request_space), arena_available);

        // -----------------------------------------------------
        // 2 - Tight fit
        // Requested allocation is for the exact available space
        // ------------------------------------------------------

        p = arena_alloc(arena, arena_available);
        cursor = arena_get_cursor(arena);
        arena_available = arena_get_available(arena);

        CHECK((arena_available == 0), counter,
              "On \"exact fit\" the available store in arena should be 0");

        arena = arena_destroy(arena);
        p = nullptr;

        CHECK((!arena), counter, "Arena has been destroyed successfully. Expected %p | Got %p", p,
              (void *)arena);

        return counter;
}

int main(void)
{
        struct TestCounter counter = test_arena();
        size_t failed = counter.total - counter.passed;
        printf("\n%zu / %zu Passed\n", counter.passed, counter.total);
        return failed != 0;
}
