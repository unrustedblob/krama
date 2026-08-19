#include "../src/arena.h"

#include <stdio.h>

#define CHECK(cond, ...)                              \
        do {                                          \
                if (!(cond)) {                        \
                        fprintf(stderr, "[FAIL] ");   \
                        fprintf(stderr, __VA_ARGS__); \
                        fprintf(stderr, "\n");        \
                } else {                              \
                        printf("[OK] ");              \
                        printf(__VA_ARGS__);          \
                        printf("\n");                 \
                }                                     \
        } while (false)

int main()
{
        fprintf(stdout, "[INFO] Block maximum capacity: %zu\n", BLOCK_MAX_CAP);

        // 1 - The simple case
        size_t request_cap = 10;
        size_t request_space = 2;

        struct Arena *arena = arena_create(request_cap);

        void *p = arena_alloc(arena, request_space);
        void *cursor = arena_get_cursor(arena);

        CHECK(((uintptr_t)p != (uintptr_t)cursor),
              "After alloc, pointer returned and current Arena cursor position are not the same");

        CHECK((((uintptr_t)cursor - (uintptr_t)p) == request_space),
              "After alloc, Arena cursor is ahead of pointer returned by exactly requested space "
              "%zu",
              request_space);

        size_t arena_cap = arena_get_capacity(arena);

        CHECK((arena_cap == request_cap),
              "Arena capacity is the same as the requested capacity. Expected %zu | Got %zu",
              arena_cap, request_cap);

        size_t arena_available = arena_get_available(arena);

        CHECK((arena_available == (arena_cap - request_space)),
              "Available store in the arenas buffer is exactly capacity - requested space. "
              "Expected %zu | Got %zu",
              (arena_cap - request_space), arena_available);

        arena = arena_destroy(arena);
        void *np = nullptr;

        CHECK((!arena), "Arena has been destroyed successfully. Expected %p | Got %p", np,
              (void *)arena);
}
