// API for the Arena implementation

#ifndef KRAMA_ARENA_H
#define KRAMA_ARENA_H

#include <stddef.h>

struct Arena;

// Create and initialize the arena. The created arena consists of a block with
// an implementation defined initial capacity.
//
// Lifetime:
//   Owned, release using `arena_destroy`.
//
// NOTE:
//   - Will exit() if memory for the block could not be allocated by the system.
struct Arena *arena_create(void);

// Allocates memory on the arena and returns a pointer to the allocated store.
//
// Lifetime:
//   Arena — valid until teardown, never freed individually.
//
// Preconditions:
//   - `arena` should be non-null
//   - `size` should be greater than 0
//
// NOTE:
//   This may cause the arena to grow
void *arena_alloc(struct Arena *arena, size_t size);

// Destroys the arena and returns nullptr.
//
// Preconditions:
//   - arena was created using `arena_create()`
//   - `arena` must be non-null
nullptr_t arena_destroy(struct Arena *arena);

#ifdef KRAMA_TEST_SUITE

const unsigned char *arena_get_cursor(const struct Arena *arena);
const unsigned char *arena_get_buffer(const struct Arena *arena);
size_t arena_get_available(const struct Arena *arena);
size_t arena_get_capacity(const struct Arena *arena);
size_t arena_get_max_cap(void);
size_t arena_get_init_cap(void);
size_t arena_get_growth_factor(void);

#endif // KRAMA_TEST_SUITE
#endif // KRAMA_ARENA_H
