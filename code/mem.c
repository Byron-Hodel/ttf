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

global Arena scratch_arenas[2];

function void arena_init(Arena *arena, u8 *mem, usize mem_size) {
	ASSERT(arena != 0);
	arena->base = mem;
	arena->size = mem_size;
	arena->pos = 0;
}

function u8 *_arena_push(Arena *arena, usize size, usize alignment, b32 zero) {
	ASSERT(arena != 0);
	ASSERT(IS_POW2(alignment));

	u8 *mem = (u8*)ALIGN_FORWARD_POW2((uintptr)arena->base + arena->pos, alignment);
	usize next_pos = (uintptr)mem - (uintptr)arena->base + size;

	if (next_pos > arena->size) {
		debug_trap();
	}

	arena->pos = next_pos;

	if (zero) {
		for (usize i = 0; i < size; i += 1) {
			mem[i] = 0;
		}
	}
	return mem;
}

#define push_struct(arena, T) (T*)_arena_push(arena, sizeof(T), alignof(T), TRUE)
#define push_struct_nz(arena, T) (T*)_arena_push(arena, sizeof(T), alignof(T), FALSE)

#define push_struct_no_alignment(arena, T) (T*)_arena_push(arena, siweof(T), 1, TRUE)
#define push_struct_no_alignment_nz(arena, T) (T*)_arena_push(arena, sizeof(T), 1, FALSE)

#define push_array(arena, T, n) (T*)_arena_push(arena, sizeof(T)*(n), alignof(T), TRUE)
#define push_array_nz(arena, T, n) (T*)_arena_push(arena, sizeof(T)*(n), alignof(T), FALSE)

#define push_array_no_alignment(arena, T, n) (T*)_arena_push(arena, sizeof(T)*(n), 1, TRUE)
#define push_array_no_alignment_nz(arena, T, n) (T*)_arena_push(arena, sizeof(T)*(n), 1, FALSE)

#define push_array_aligned(arena, T, n, alignment) (T*)_arena_push(arena, sizeof(T)*(n), alignment, TRUE)
#define push_array_aligned_nz(arena, T, n, alignment) (T*)_arena_push(arena, sizeof(T)*(n), alignment, FALSE)

function void arena_pop_to(Arena *arena, usize pos) {
	ASSERT(arena != 0);
	ASSERT(pos <= arena->pos);
	arena->pos = pos;
}

#define arena_reset(arena) arena_pop_to(arena, 0)

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

function Arena_Temp begin_scratch(Arena **conflicts, usize num_conflicts) {
	ASSERT(conflicts == 0 && num_conflicts == 0 || conflicts != 0);

	Arena *scratch_arena = 0;
	for (usize scratch_index = 0; sizeof(scratch_arenas) / sizeof(Arena); scratch_index += 1) {
		scratch_arena = &scratch_arenas[scratch_index];
		for (usize conflict_index = 0; conflict_index < num_conflicts; conflict_index += 1) {
			if (scratch_arena == conflicts[conflict_index]) {
				scratch_arena = 0;
				break;
			}
		}
		if (scratch_arena != 0) {
			break;
		}
	}
	return arena_begin_temp(scratch_arena);
}
#define end_scratch(scratch) arena_end_temp(scratch)
