#ifndef MEMORY_AREANA_H
#define MEMORY_AREANA_H

#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <stdbool.h>

typedef struct Arena Arena;
typedef void (*ArenaOverflowFn)(Arena* arena, size_t requested_size);

struct Arena {
	unsigned char*  memory;
	size_t          capacity;
	size_t          offset;
	const char*     name;
	ArenaOverflowFn on_overflow;
};

typedef struct ArenaTemp {
	Arena* arena;
	size_t offset;
} ArenaTemp;

Arena arena_create(size_t capacity);
Arena arena_create_named(size_t capacity, const char* name);
void  arena_destroy(Arena* arena);

void arena_set_overflow_handler(Arena* arena, ArenaOverflowFn fn);
void arena_print_stats(const Arena* arena);

void* arena_push(Arena* arena, size_t size);
void* arena_push_aligned(Arena* arena, size_t size, size_t align);
void* arena_push_zero(Arena* arena, size_t size);
char* arena_push_str(Arena* arena, const char* str);
char* arena_sprintf(Arena* a, const char* format, ...)
	__attribute__((format(printf, 2, 3)));
char* arena_vsprintf(Arena* a, const char* format, va_list args);

void arena_reset(Arena* arena);
void arena_pop_to(Arena* arena, size_t offset);

void* arena_memdup(Arena* a, const void* src, size_t size);
void* arena_realloc(Arena* a, void* oldptr, size_t oldsz, size_t newsz);

ArenaTemp arena_temp_begin(Arena* arena);
void      arena_temp_end(ArenaTemp temp);

// Some macors:
#define ARENA_PUSH_TYPE(arena, T) (T*)arena_push_zero((arena), sizeof(T))
#define ARENA_PUSH_ARRAY(arena, T, count) \
	(T*)arena_push_zero((arena), sizeof(T) * (size_t)(count))

#define KB(x) ((size_t)(x) << 10)
#define MB(x) ((size_t)(x) << 20)
#define GB(x) ((size_t)(x) << 30)

#define ArrayCount(a) (sizeof(a) / sizeof((a)[0]))

// macros.h
#define MA_KB(x) ((size_t)(x) << 10)
#define MA_MB(x) ((size_t)(x) << 20)
#define MA_GB(x) ((size_t)(x) << 30)

#define MA_ArrayCount(a) (sizeof(a) / sizeof((a)[0]))

#define MA_Min(a, b) ((a) < (b) ? (a) : (b))
#define MA_Max(a, b) ((a) > (b) ? (a) : (b))
#define MA_ClampTop(a, x) MA_Min(a, x)
#define MA_ClampBot(a, x) MA_Max(a, x)
#define MA_Clamp(x, lo, hi) ((x) < (lo) ? (lo) : (x) > (hi) ? (hi) : (x))

#define MA_PushArray(arena, type, count) \
	(type*)arena_push((arena), sizeof(type) * (count))
#define MA_PushArrayZero(arena, type, count) \
	(type*)arena_push_zero((arena), sizeof(type) * (count))
#define MA_PushStruct(arena, type) MA_PushArray(arena, type, 1)
#define MA_PushStructZero(arena, type) MA_PushArrayZero(arena, type, 1)

// Do NOT `break` inside this block — arena_temp_end is only invoked via the
// loop increment, which `break` skips, leaking the temp allocation.
#define MA_TempScope(arena)                                     \
	for (ArenaTemp MA_tmp_##__LINE__ = arena_temp_begin(arena), \
				   *MA_i_##__LINE__ = 0;                        \
		 !MA_i_##__LINE__;                                      \
		 MA_i_##__LINE__ = (void*)1, arena_temp_end(MA_tmp_##__LINE__))

// dynamic_array.h
#define MA_DynArray(type) \
	struct {              \
		type*  data;      \
		size_t count;     \
		size_t capacity;  \
	}

#define ma_da_push(arena, da, item)                                    \
	do {                                                               \
		__typeof__(da) MA_da_ = (da);                                  \
		if (MA_da_->count >= MA_da_->capacity) {                       \
			size_t MA_old_cap_ = MA_da_->capacity;                     \
			size_t MA_new_cap_ = MA_old_cap_ ? MA_old_cap_ * 2 : 8;    \
			size_t MA_elem_size_ = sizeof(*MA_da_->data);              \
			MA_da_->data = arena_realloc((arena), MA_da_->data,        \
										 MA_old_cap_ * MA_elem_size_,  \
										 MA_new_cap_ * MA_elem_size_); \
			MA_da_->capacity = MA_new_cap_;                            \
		}                                                              \
		MA_da_->data[MA_da_->count++] = (item);                        \
	} while (0)

#define ma_da_reserve(arena, da, n)                                        \
	do {                                                                   \
		__typeof__(da) MA_da_ = (da);                                      \
		size_t         MA_target_cap_ = (n);                               \
		if (MA_target_cap_ > MA_da_->capacity) {                           \
			size_t MA_elem_size_ = sizeof(*MA_da_->data);                  \
			MA_da_->data = arena_realloc((arena), MA_da_->data,            \
										 MA_da_->capacity * MA_elem_size_, \
										 MA_target_cap_ * MA_elem_size_);  \
			MA_da_->capacity = MA_target_cap_;                             \
		}                                                                  \
	} while (0)

#define MA_PushArrayCopy(arena, T, src, count) \
	(T*)arena_memdup((arena), (src), sizeof(T) * (size_t)(count))

// string8.h
typedef struct MA_Str8 {
	char*  str;
	size_t size;
} MA_Str8;

#define MA_S8(lit) (MA_Str8){(char*)(lit), sizeof(lit) - 1}
#define MA_S8Fmt "%.*s"
#define MA_S8Arg(s) (int)(s).size, (s).str

static inline MA_Str8 ma_str8(char* str, size_t size) {
	return (MA_Str8){str, size};
}
static inline MA_Str8 ma_str8_cstr(char* cstr) {
	return (MA_Str8){cstr, strlen(cstr)};
}

static inline MA_Str8 ma_str8_copy(Arena* arena, MA_Str8 s) {
	char* mem = (char*)arena_push_aligned(arena, s.size + 1, 1);
	if (!mem) return (MA_Str8){0};
	memcpy(mem, s.str, s.size);
	mem[s.size] = 0;
	return (MA_Str8){mem, s.size};
}

static inline MA_Str8 ma_str8_cat(Arena* arena, MA_Str8 a, MA_Str8 b) {
	char* mem = (char*)arena_push_aligned(arena, a.size + b.size + 1, 1);
	if (!mem) return (MA_Str8){0};
	memcpy(mem, a.str, a.size);
	memcpy(mem + a.size, b.str, b.size);
	mem[a.size + b.size] = 0;
	return (MA_Str8){mem, a.size + b.size};
}

static inline bool ma_str8_eq(MA_Str8 a, MA_Str8 b) {
	return a.size == b.size && memcmp(a.str, b.str, a.size) == 0;
}

typedef struct MA_Str8Node {
	struct MA_Str8Node* next;
	MA_Str8             str;
} MA_Str8Node;

typedef struct MA_Str8List {
	MA_Str8Node* first;
	MA_Str8Node* last;
	size_t       node_count;
	size_t       total_size;
} MA_Str8List;

static inline void ma_str8_list_push(Arena* arena, MA_Str8List* list,
									 MA_Str8 str) {
	MA_Str8Node* n = MA_PushStruct(arena, MA_Str8Node);
	n->str = str;
	n->next = NULL;
	if (list->last)
		list->last->next = n;
	else
		list->first = n;
	list->last = n;
	list->node_count++;
	list->total_size += str.size;
}

static inline MA_Str8 ma_str8_list_join(Arena* arena, MA_Str8List* list) {
	char*  mem = (char*)arena_push_aligned(arena, list->total_size + 1, 1);
	size_t off = 0;
	for (MA_Str8Node* n = list->first; n; n = n->next) {
		memcpy(mem + off, n->str.str, n->str.size);
		off += n->str.size;
	}
	mem[off] = 0;
	return (MA_Str8){mem, list->total_size};
}

// 2D Grid matrix
#define MA_Grid(type) \
	struct {          \
		type*  data;  \
		size_t rows;  \
		size_t cols;  \
	}

#define MA_GridAlloc(arena, GridType, ElemType, r, c)                       \
	(GridType) {                                                            \
		.data = MA_PushArray((arena), ElemType, (size_t)(r) * (size_t)(c)), \
		.rows = (size_t)(r), .cols = (size_t)(c)                            \
	}

#define MA_GridAllocZero(arena, GridType, ElemType, r, c)                   \
	(GridType) {                                                            \
		.data =                                                             \
			MA_PushArrayZero((arena), ElemType, (size_t)(r) * (size_t)(c)), \
		.rows = (size_t)(r), .cols = (size_t)(c)                            \
	}

#define MA_GRID_CREATE_IMPL(FnName, GridType, ElemType)                     \
	static inline GridType FnName(Arena* arena, size_t rows, size_t cols) { \
		return MA_GridAllocZero(arena, GridType, ElemType, rows, cols);     \
	}

#define MA_GridInBounds(gptr, r, c) \
	((size_t)(r) < (gptr)->rows && (size_t)(c) < (gptr)->cols)

#define MA_GridAt(g, r, c) ((g).data[(size_t)(r) * (g).cols + (size_t)(c)])

#define MA_GridAtP(gptr, r, c) \
	((gptr)->data[(size_t)(r) * (gptr)->cols + (size_t)(c)])

#define MA_GridAtSafe(gptr, r, c) \
	(MA_GridInBounds((gptr), (r), (c)) ? &MA_GridAtP((gptr), (r), (c)) : NULL)

#define MA_GridCount(g) ((g).rows * (g).cols)

#define MA_GridFill(g, val)                                  \
	do {                                                     \
		size_t MA_total_ = MA_GridCount(g);                  \
		for (size_t MA_i_ = 0; MA_i_ < MA_total_; ++MA_i_) { \
			(g).data[MA_i_] = (val);                         \
		}                                                    \
	} while (0)

#define MA_GridCopy(arena, GridType, ElemType, src_gptr)              \
	(GridType) {                                                      \
		.data = MA_PushArrayCopy((arena), ElemType, (src_gptr)->data, \
								 MA_GridCount(*(src_gptr))),          \
		.rows = (src_gptr)->rows, .cols = (src_gptr)->cols            \
	}

#endif /* MEMORY_AREANA_H*/
