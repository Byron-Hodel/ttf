typedef struct {
	u8    *base;
	usize size;
	usize pos;
} Arena;

typedef struct {
	Arena *arena;
	usize pos;
} Arena_Temp;

global const usize KiB = 1024;
global const usize MiB = 1024*1024;
global const usize GiB = 1024*1024*1024;

function void arena_init(Arena *arena, u8 *mem, usize mem_size) {
	ASSERT(arena != 0);
	arena->base = mem;
	arena->size = mem_size;
	arena->pos = 0;
}

function u8 *_arena_push(Arena *arena, usize size, usize alignment, b32 zero) {
	ASSERT(arena != 0);
	ASSERT(IS_POW2(alignment));

	u8 *mem = 0;
	if (size > 0) {
		u8 *aligned_mem = (u8*)ALIGN_FORWARD_POW2((usize)arena->base + arena->pos, alignment);
		usize next_pos = (usize)aligned_mem - (usize)arena->base + arena->pos + size;

		if (next_pos > arena->size) {
			debug_trap();
		}

		arena->pos = next_pos;
		mem = aligned_mem;
	}

	if (zero) {
		for (usize i = 0; i < size; i += 1) {
			mem[i] = 0;
		}
	}
	return mem;
}

#define push_struct(arena, T) _arena_push(arena, sizeof(T), alignof(T), TRUE)
#define push_struct_nz(arena, T) _arena_push(arena, sizeof(T), alignof(T), FALSE)

#define push_array(arena, T, n) _arena_push(arena, sizeof(T)*(n), alignof(T), TRUE)
#define push_array_nz(arena, T, n) _arena_push(arena, sizeof(T)*(n), alignof(T), FALSE)

#define push_array_aligned(arena, T, n, alignment) _arena_push(arena, sizeof(T)*(n), alignment, TRUE)
#define push_array_aligned_nz(arena, T, n, alignment) _arena_push(arena, sizeof(T)*(n), alignment, FALSE)

function void arena_pop_to(Arena *arena, usize pos) {
	ASSERT(arena != 0);
	ASSERT(pos <= arena->pos);
	arena->pos = pos;
}

function Arena_Temp arena_begin_temp(Arena *arena) {
	Arena_Temp temp = {0};
	temp.arena = arena;
	if (arena != 0) {
		temp.pos = arena->pos;
	}
	return temp;
}

function void arena_end_temp(Arena_Temp temp) {
	if (temp.arena != 0) {
		arena_pop_to(temp.arena, temp.pos);
	}
}
