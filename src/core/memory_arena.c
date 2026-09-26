#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <sys/mman.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "memory_arena.h"

#define ARENA_DEFAULT_ALIGNMENT 16

static inline bool is_power_of_two(uintptr_t x) {
	return (x != 0) && ((x & (x - 1)) == 0);
}

static uintptr_t align_up(uintptr_t n, size_t align) {
	assert(is_power_of_two(align));
	assert(n <= UINTPTR_MAX - (align - 1));

	return (n + (uintptr_t)align - 1) & ~((uintptr_t)align - 1);
}

static unsigned char* reserve_memory(size_t size) {
	void* mem = mmap(NULL, size, PROT_READ | PROT_WRITE,
					 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

	return (mem == MAP_FAILED) ? NULL : (unsigned char*)mem;
}

static void release_memory(unsigned char* mem, size_t size) {
	if (!mem) return;

	munmap(mem, size);
}

void fatal_on_overflow_default(Arena* arena, size_t requested) {
	fprintf(stderr,
			"arena '%s' overflow: requested %zu, capacity %zu, used %zu\n",
			arena->name ? arena->name : "", requested, arena->capacity,
			arena->offset);
	abort();
}

static inline void* handle_arena_overflow(Arena* arena, size_t size) {
	if (arena->on_overflow) {
		arena->on_overflow(arena, size);
		return NULL;
	}
	assert(0 && "arena overflow (no overflow handler set)");
	return NULL;
}

Arena arena_create(size_t capacity) {
	return arena_create_named(capacity, NULL);
}

Arena arena_create_named(size_t capacity, const char* name) {
	Arena a;
	a.memory = reserve_memory(capacity);
	assert(a.memory && "arena_create: failed to reserve memory");
	a.capacity = a.memory ? capacity : 0;
	a.offset = 0;
	a.name = name;
	a.on_overflow = fatal_on_overflow_default;
	return a;
}

void arena_set_overflow_handler(Arena* arena, ArenaOverflowFn fn) {
	arena->on_overflow = fn ? fn : fatal_on_overflow_default;
}

void arena_print_stats(const Arena* arena) {
	const char* name = arena->name ? arena->name : "";

	fprintf(stderr, "[arena%s%s] capacity=%zu used=%zu (%.1f%%)\n",
			*name ? " " : "", name, arena->capacity, arena->offset,
			arena->capacity
				? (100.0 * (double)arena->offset / (double)arena->capacity)
				: 0.0);
}

void arena_destroy(Arena* arena) {
	release_memory(arena->memory, arena->capacity);
	arena->memory = NULL;
	arena->capacity = 0;
	arena->offset = 0;
}

void* arena_push_aligned(Arena* arena, size_t size, size_t align) {
	if (size == 0) return NULL;

	if (align == 0) {
		align = ARENA_DEFAULT_ALIGNMENT;
	}

	if (!is_power_of_two(align)) {
		assert(0 && "Alignment must be a power of two");
		return NULL;
	}

	size_t    remaining = arena->capacity - arena->offset;
	uintptr_t current_ptr = (uintptr_t)(arena->memory + arena->offset);
	uintptr_t aligned_ptr = align_up(current_ptr, align);
	size_t    padding = aligned_ptr - current_ptr;

	if (padding > remaining || size > remaining - padding) {
		return handle_arena_overflow(arena, size);
	}

	arena->offset += padding + size;
	return (void*)aligned_ptr;
}

void* arena_push(Arena* arena, size_t size) {
	return arena_push_aligned(arena, size, ARENA_DEFAULT_ALIGNMENT);
}

void* arena_push_zero(Arena* arena, size_t size) {
	void* ptr = arena_push(arena, size);
	if (ptr) memset(ptr, 0, size);
	return ptr;
}

void* arena_realloc(Arena* arena, void* oldptr, size_t oldsize,
					size_t newsize) {
	if (!oldptr) return arena_push(arena, newsize);

	uintptr_t base = (uintptr_t)arena->memory;
	uintptr_t p = (uintptr_t)oldptr;

	bool valid = (p >= base) && (p - base <= arena->offset) &&
				 (oldsize <= arena->offset - (p - base));

	assert(valid && "arena_realloc: oldptr is stale, out of bounds, or from a "
					"different arena");
	if (!valid) return NULL;

	if (newsize <= oldsize)
		return oldptr; // validated now, even on shrink/equal path

	size_t ptr_offset = (size_t)(p - base);

	if (ptr_offset + oldsize == arena->offset) {
		size_t diff = newsize - oldsize;
		if (diff <= arena->capacity - arena->offset) {
			arena->offset += diff;
			return oldptr;
		}
	}

	void* newptr = arena_push(arena, newsize);
	if (newptr) memcpy(newptr, oldptr, oldsize);
	return newptr;
}

void* arena_memdup(Arena* arena, const void* src, size_t size) {
	if (!src || size == 0) return NULL;
	void* copy = arena_push(arena, size);
	if (copy) memcpy(copy, src, size);
	return copy;
}

char* arena_push_str(Arena* arena, const char* str) {
	if (!str) return NULL;
	size_t len = strlen(str) + 1;
	char*  copy = (char*)arena_push_aligned(arena, len, 1);
	if (copy) memcpy(copy, str, len);
	return copy;
}

char* arena_vsprintf(Arena* arena, const char* fmt, va_list args) {
	if (!fmt) return NULL;

	va_list args_copy;
	va_copy(args_copy, args);
	int len = vsnprintf(NULL, 0, fmt, args_copy);
	va_end(args_copy);

	if (len < 0) return NULL;

	size_t size = (size_t)len + 1;
	char*  str = (char*)arena_push_aligned(arena, size, 1);
	if (str) {
		vsnprintf(str, size, fmt, args);
	}
	return str;
}

char* arena_sprintf(Arena* arena, const char* fmt, ...) {
	if (!fmt) return NULL;
	va_list args;
	va_start(args, fmt);
	char* str = arena_vsprintf(arena, fmt, args);
	va_end(args);
	return str;
}

void arena_reset(Arena* arena) {
	arena->offset = 0;
}

void arena_pop_to(Arena* arena, size_t offset) {
	assert(offset <= arena->offset && "arena_pop_to: offset is in the future");
#ifndef NDEBUG
	memset(arena->memory + offset, 0xDD, arena->offset - offset);
#endif
	arena->offset = offset;
}

ArenaTemp arena_temp_begin(Arena* arena) {
	ArenaTemp t;
	t.arena = arena;
	t.offset = arena->offset;
	return t;
}

void arena_temp_end(ArenaTemp temp) {
	temp.arena->offset = temp.offset;
}
