#include "stdio.h"
#include "stdlib.h"
#include "string.h"

#include "base.h"
#include "mem.c"
#include "ttf.c"

int main(void) {
	Arena arena;
	arena_init(&arena, malloc(2*MiB), 2*MiB);
	arena_init(&scratch_arenas[0], malloc(2*MiB), 2*MiB);
	arena_init(&scratch_arenas[1], malloc(2*MiB), 2*MiB);

	const char *TEST_FILE_PATH = "./Cormorant_Garamond/CormorantGaramond-VariableFont_wght.ttf";

	Arena_Temp scratch = begin_scratch(0, 0);

	u8 *font_data = 0;
	usize font_data_size = 0;
	{
		FILE *font_file = fopen(TEST_FILE_PATH, "rb");
		ASSERT(font_file != 0);

		fseek(font_file, 0, SEEK_END);
		font_data_size = (usize)ftell(font_file);
		fseek(font_file, 0, SEEK_SET);

		font_data = push_array(scratch.arena, u8, font_data_size);
		fread(font_data, 1, font_data_size, font_file);
		fclose(font_file);
	}
	
	Font font = font_make(&arena, font_data, font_data_size);
	end_scratch(scratch);

	u8 lookup_char = 'E';
	u16 glyph_index = font_lookup_glyph_index(font, lookup_char);
	printf("glyph index for '%c' is: %d\n", lookup_char, glyph_index);
	//unpack_glyph_outline(font, glyph_index);

	return 0;
}
