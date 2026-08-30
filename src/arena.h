// API for the Arena implementation

#ifndef FUNC_ARENA_H
#define FUNC_ARENA_H

#include <stddef.h>

struct Arena;

// Create and initialize the arena. Caller owns the arena and is responsible
// for releasing it using `arena_destroy`. The created arena consists of a block
// with an implementation defined initial capacity.
// - Will abort() if (somehow) this initial capacity is greater than the maximum
//   allowed capacity.
// - Will exit() if memory for the block could not be allcoated by the system.
struct Arena *arena_create(void);

// Allocates memory on the arena and returns a pointer to the allocated store
// NOTE: This may cause the arena to grow
void *arena_alloc(struct Arena *arena, size_t size);

// Destroys the arena and returns nullptr. This should only be called if the
// arena was created using `arena_create()`
nullptr_t arena_destroy(struct Arena *arena);

#ifdef FUNC_TEST_SUITE

const unsigned char *arena_get_cursor(const struct Arena *arena);
const unsigned char *arena_get_buffer(const struct Arena *arena);
size_t arena_get_available(const struct Arena *arena);
size_t arena_get_capacity(const struct Arena *arena);
size_t arena_get_max_cap(void);
size_t arena_get_init_cap(void);
size_t arena_get_growth_factor(void);

#endif // FUNC_TEST_SUITE
#endif // FUNC_ARENA_H
