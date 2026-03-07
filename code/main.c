#include "stdio.h"
#include "stdlib.h"
#include "string.h"

#include "raylib.h"

#include "base.h"
#include "mem.c"
#include "ttf.c"

int main(void) {
	// allocate memory arenas
	Arena arena;
	arena_init(&arena, malloc(2*MiB), 2*MiB);
	arena_init(&scratch_arenas[0], malloc(2*MiB), 2*MiB);
	arena_init(&scratch_arenas[1], malloc(2*MiB), 2*MiB);

	const char *TEST_FILE_PATH = "./Cormorant_Garamond/CormorantGaramond-VariableFont_wght.ttf";

	U8_Slice font_data = {0};
	{
		FILE *font_file = fopen(TEST_FILE_PATH, "rb");
		ASSERT(font_file != 0);

		fseek(font_file, 0, SEEK_END);
		font_data.len = (usize)ftell(font_file);
		fseek(font_file, 0, SEEK_SET);

		font_data.base = push_array(&arena, u8, font_data.len);
		fread(font_data.base, 1, font_data.len, font_file);
		fclose(font_file);
	}
	
	TTF_Font font = font_make(font_data);

	InitWindow(800, 450, "font stuff");	

	Arena_Temp scratch = begin_scratch(0, 0);

	Glyph_Outline glyph_outline = {0};
	while (!WindowShouldClose()) {
		i32 screen_width = GetScreenWidth();
		i32 screen_height = GetScreenHeight();

		BeginDrawing();
		ClearBackground(RAYWHITE);

		u32 last_input = 0;
		for (u32 c = (u32)GetCharPressed(); c != 0; c = (u32)GetCharPressed()) {
			last_input = (u32)c;
		}

		if (last_input != 0) {
			end_scratch(scratch);
			scratch = begin_scratch(0, 0);

			u16 glyph_index = font_glyph_index_from_codepoint(font, last_input);
			glyph_outline = font_unpack_glyph_outline(scratch.arena, font, glyph_index);
		}

		// setup ortho camera
		Camera2D cam;	
		cam.offset = (Vector2) { screen_width / 2.0f, screen_height / 2.0f };
    cam.target = (Vector2) {0};
    cam.rotation = 0.0f;
    cam.zoom = (f32)screen_height / 4.0f / (f32)font.units_per_em;
		BeginMode2D(cam);

		// draw axis
		{
			Vector2 p0 = { -32768, 0 };
			Vector2 p1 = { 32768 , 0 };
			DrawLineV(p0, p1, BLACK);

			Vector2 p2 = { 0, -32768 };
			Vector2 p3 = { 0, 32768 };
			DrawLineV(p2, p3, BLACK);
		}

		// draw em square
		{
			Vector2 p0 = { 0, 0 };
			Vector2 p1 = { 0, -font.units_per_em };
			Vector2 p2 = { font.units_per_em, -font.units_per_em };
			Vector2 p3 = { font.units_per_em, 0 };
			DrawLineV(p0, p1, RED);
			DrawLineV(p1, p2, RED);
			DrawLineV(p2, p3, RED);
			DrawLineV(p3, p0, RED);
		}

		// draw glyph points
		for (usize i = 0; i < glyph_outline.num_verts; i += 1) {
			f32 x =  (f32)glyph_outline.verts[i].x / 64.0f;
			f32 y = -(f32)glyph_outline.verts[i].y / 64.0f;
			DrawCircle(x, y, 20, BLACK);
		}

		EndMode2D();

		EndDrawing();
	}

	end_scratch(scratch);

	CloseWindow();

	return 0;
}
