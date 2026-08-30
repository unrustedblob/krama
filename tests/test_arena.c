#include "../src/arena.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

struct TestCounter {
        size_t passed;
        size_t total;
};

static_assert((sizeof(struct TestCounter) <= 16), "struct size should be register-passable");

static struct TestCounter test_arena(void);

// TODO(D-025): Possible function
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
        // [TEST 0] - Create the arena
        // ---------------------------------------------------

        struct Arena *arena = arena_create();

        printf("\n[TEST 0] - Create Arena\n---------------------------------------------\n");

        CHECK((arena != nullptr), counter, "Creation of arena should return non-NULL pointer");

        // ---------------------------------------------------
        // [TEST 1] - Arena Initial State
        // Verify the initial state is consistent
        // ---------------------------------------------------

        const unsigned char *cursor = arena_get_cursor(arena);
        const unsigned char *buffer = arena_get_buffer(arena);
        size_t arena_cap = arena_get_capacity(arena);
        size_t arena_available = arena_get_available(arena);

        printf("\n[TEST 1] - Arena Iniital State\n---------------------------------------------\n");

        CHECK(((arena_cap == block_init_cap) && (arena_cap == arena_available)
               && (cursor == buffer)),
              counter, "Created arena is initialized correctly");

        // ---------------------------------------------------
        // [TEST 2] - The usual case
        // Requeted allocation is within range.
        // ---------------------------------------------------

        // The last item in request_sapce is to force the cursor into an aligned
        // address, this is required for correctly testing [Test 3]
        size_t request_space[] = { 100, 18, 205, 103, 256, 1, alignof(max_align_t) };
        const unsigned char *p = nullptr;

        printf("\n[TEST 2] - The usuaal Case\n---------------------------------------------\n");

        for (size_t i = 0; i < sizeof request_space / sizeof(size_t); ++i) {
                printf("Run %zu\n", i + 1);

                p = arena_alloc(arena, request_space[i]);
                cursor = arena_get_cursor(arena);
                arena_available = arena_get_available(arena);

                CHECK(((uintptr_t)p % alignof(max_align_t) == 0), counter,
                      "Pointer returned is aligned. Pointer: %p, Align: %zu", (void *)p,
                      alignof(max_align_t));

                bool cursor_moved = (p < cursor);

                CHECK((cursor_moved), counter, "After alloc, the cursor has moved ahead");

                // If the cursor hasn't moved we need to fail the next test
                ptrdiff_t distance = cursor_moved ? cursor - p : 0;

                CHECK(((size_t)distance == request_space[i]), counter,
                      "After alloc, the cursor is ahead of pointer returned by exactly requested "
                      "space %zu",
                      request_space[i]);

                CHECK((arena_available <= (arena_cap - request_space[i])), counter,
                      "Available arena buffer storage is bounded by the remaining capacity "
                      "Available (%zu) <= Max Possible (%zu)",
                      arena_available, (arena_cap - request_space[i]));
        }

        // -----------------------------------------------------
        // [TEST 3] - Tight fit
        // Requested allocation is for the exact available space
        // NOTE: Cursor must be aligned for this test
        // ------------------------------------------------------

        assert(((uintptr_t)cursor % alignof(max_align_t) == 0));

        printf("\n[TEST 3] - Tight Fit\n---------------------------------------------\n");

        p = arena_alloc(arena, arena_available);
        cursor = arena_get_cursor(arena);
        arena_available = arena_get_available(arena);

        CHECK((arena_available == 0), counter,
              "On \"exact fit\", for an aligned cursor, the available store in arena should be 0 | "
              "Got: %zu",
              arena_available);

        // -----------------------------------------------------
        // [TEST 4] - Chain Arena Block
        // Request allocation to chain a new block.
        // Dep: [TEST 3] must have completely passed for this
        // ------------------------------------------------------

        assert(counter.passed == counter.total && arena_available == 0);

        printf("\n[TEST 4] - Grow Arena\n---------------------------------------------\n");

        size_t old_cap = arena_cap;
        size_t arena_growth_factor = arena_get_growth_factor();

        p = arena_alloc(arena, 1);
        arena_cap = arena_get_capacity(arena);

        CHECK(((arena_cap / old_cap) == arena_growth_factor), counter,
              "Arena should grow by growth factor. (Previous Capacity %zu | Current Capacity %zu) "
              "Expected %zu | Got %zu",
              old_cap, arena_cap, arena_growth_factor, (arena_cap / old_cap));

        // -----------------------------------------------------
        // [TEST 5] - Max Fit
        // Request allocation of maximum possible capacity.
        // ------------------------------------------------------

        printf("\n[TEST 5] - Max Fit\n---------------------------------------------\n");

        p = arena_alloc(arena, block_max_cap);
        arena_cap = arena_get_capacity(arena);

        CHECK((arena_cap == block_max_cap), counter,
              "Arena capacity should be the same as allowed maximum block capacity Expected %zu | "
              "Got %zu",
              block_max_cap, arena_cap);

        // -----------------------------------------------------
        // [TEST 6] - Arena Destroy
        // ------------------------------------------------------

        printf("\n[TEST 6] - Arena Destroy\n---------------------------------------------\n");

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
