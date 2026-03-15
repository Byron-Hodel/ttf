#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GLAD_GL_IMPLEMENTATION
#include <gl.h>
#include <GLFW/glfw3.h>

#include "base.h"
#include "mem.c"
#include "math.c"

#include "ttf.c"
#include "gl_graphics.c"

typedef enum {
	View_List,
	View_Inspect,
} View;

function U8_Slice read_entire_file(Arena *arena, const char *path) {
	U8_Slice file_data = {0};
	FILE *file = fopen(path, "rb");

	if (file != 0) {
		fseek(file, 0, SEEK_END);
		file_data.len = (usize)ftell(file);
		fseek(file, 0, SEEK_SET);

		file_data.base = push_array(arena, u8, file_data.len);
		fread(file_data.base, 1, file_data.len, file);
		fclose(file);
	}
	
	return file_data;
}

function b32 point_is_in_rect(f32 x, f32 y, Rect rect) {
	return x >= rect.x && x <= rect.x + rect.w &&
	       y >= rect.y && y <= rect.y + rect.h;
}

global Vec2 scroll_delta;
function void mouse_scroll_cb(GLFWwindow *window, f64 xoffset, f64 yoffset) {
	scroll_delta.x = (f32)xoffset;
	scroll_delta.y = (f32)yoffset;
}

global const char *DEFAULT_FONT_PATH = "./Cormorant_Garamond/CormorantGaramond-VariableFont_wght.ttf";

int main(void) {
	// allocate memory arenas
	Arena font_arena;
	arena_init(&font_arena, malloc(2*MiB), 2*MiB);
	arena_init(&scratch_arenas[0], malloc(2*MiB), 2*MiB);
	arena_init(&scratch_arenas[1], malloc(2*MiB), 2*MiB);

	U8_Slice font_data = read_entire_file(&font_arena, DEFAULT_FONT_PATH);
	
	TTF_Font font = font_make(font_data);

	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	GLFWwindow *window = glfwCreateWindow(800, 450, "font stuff", 0, 0);
	glfwSetScrollCallback(window, mouse_scroll_cb);
	glfwMakeContextCurrent(window);
	glfwSwapInterval(1);
	gladLoadGL(glfwGetProcAddress);

	gl_init();

	struct {
		View current_view;
		View next_view;

		f32 list_scroll;

		u16 glyph_index;
	} ui = {0};
	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();

		i32 screen_width, screen_height;
		glfwGetWindowSize(window, &screen_width, &screen_height);
		i32 framebuffer_width, framebuffer_height;
		glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);
		Vec2 mouse_pos;
		{
			f64 x, y;
			glfwGetCursorPos(window, &x, &y);
			mouse_pos.x = (f32)x;
			mouse_pos.y = (f32)y;
		}
		f32 screen_to_fb_scale_w = (f32)framebuffer_width / (f32)screen_width;
		f32 screen_to_fb_scale_h = (f32)framebuffer_height / (f32)screen_height;

		begin_drawing(screen_width, screen_height, framebuffer_width, framebuffer_height);

		View next_view = ui.current_view;
		switch (ui.current_view) {
		//case View_Details: {
		//} break;
		case View_List: {
			const u16 GRID_CELL_SIZE = 100;
			const u16 GRID_CELL_PADDING = 3;		

			// reduce size if we have a scroll bar	
			Rect scroll_container;
			scroll_container.x = 0;
			scroll_container.y = 0;
			scroll_container.w = (f32)screen_width;
			scroll_container.h = (f32)screen_height;

			u32 num_columns = (u32)scroll_container.w / GRID_CELL_SIZE;
			ASSERT(num_columns > 0);
			u32 num_rows = (u32)(font.num_glyphs + num_columns - 1) / num_columns;

			f32 content_height = (f32)(GRID_CELL_SIZE * num_rows);

			ui.list_scroll -= scroll_delta.y * 75;
			if (ui.list_scroll < 0) {
				ui.list_scroll = 0;
			}
			if (ui.list_scroll > content_height - scroll_container.h) {
				ui.list_scroll = content_height - scroll_container.h;
			}

			if (content_height > scroll_container.h) {
				scroll_container.w -= 15;
				Rect scroll_bar;
				scroll_bar.x = scroll_container.x + scroll_container.w;
				scroll_bar.y = scroll_container.y;
				scroll_bar.w = 15;
				scroll_bar.h = scroll_container.h;
				draw_rect(scroll_bar, DARK_GRAY);

				Rect scroll_knob;
				scroll_knob.x = scroll_bar.x;
				scroll_knob.y = scroll_bar.y + ui.list_scroll * scroll_container.h / content_height;
				scroll_knob.w = 15;
				scroll_knob.h = scroll_container.h * scroll_container.h /
				                (f32)(GRID_CELL_SIZE * num_rows);
				if (scroll_knob.h <= 10) {
					scroll_knob.h = 10;
				}
				draw_rect(scroll_knob, GRAY);
			}

			Rect scroll_content;
			scroll_content.x = scroll_container.x;
			scroll_content.y = scroll_container.y - ui.list_scroll;
			scroll_content.w = scroll_container.w;
			scroll_content.h = content_height;

			// set the scissor rect
			{
				i32 x = (i32)(scroll_container.x * screen_to_fb_scale_w);
				i32 y = (i32)(scroll_container.y * screen_to_fb_scale_h);
				i32 w = (i32)(scroll_container.x * screen_to_fb_scale_w + 0.5f);
				i32 h = (i32)(scroll_container.y * screen_to_fb_scale_h + 0.5f);
				set_scissor_rect(x, y, w, h);
			}
			
			u32 first_visible_row = (u32)(ui.list_scroll / GRID_CELL_SIZE);
			u32 last_visible_row = (u32)((f32)first_visible_row + scroll_container.h / GRID_CELL_SIZE);
			u32 start_glyph = first_visible_row * num_columns;
			u32 end_glyph = (last_visible_row + 1) * num_columns - 1;
			if (end_glyph > font.num_glyphs) {
				end_glyph = font.num_glyphs - 1;
			}

			for (u32 i = start_glyph; i <= end_glyph; i += 1) {
				u32 row = i / num_columns;
				u32 col = i % num_columns;

				Rect cell = scroll_content;
				cell.x += (f32)(col * GRID_CELL_SIZE) + GRID_CELL_PADDING;
				cell.y += (f32)(row * GRID_CELL_SIZE) + GRID_CELL_PADDING;
				cell.w = GRID_CELL_SIZE - 2 * GRID_CELL_PADDING;
				cell.h = GRID_CELL_SIZE - 2 * GRID_CELL_PADDING;

				draw_rect(cell, LIGHT_GRAY);

				// draw glyph points
				Arena_Temp scratch = arena_begin_temp(&font_arena);
				// this is slow
				Glyph_Outline outline = font_unpack_glyph_outline(scratch.arena, font, (u16)i);
				
				for (usize vert_index = 0; vert_index < outline.num_verts; vert_index += 1) {
					i32 outline_width = (outline.x_max - outline.x_min) / 64;
					i32 outline_height = (outline.y_max - outline.y_min) / 64;
					f32 scale = outline_width > outline_height ? (f32)outline_width : (f32)outline_height;
					scale = (3.0f * cell.w / 4.0f) / scale;

					f32 vert_x = (f32)(outline.verts[vert_index].x - outline.x_min) / 64.0f;
					f32 vert_y = (f32)(outline.verts[vert_index].y - outline.y_min) / 64.0f;
					// convert glyph coordinates to ui coordinates
					vert_x *= scale;
					vert_y *= -scale;
					vert_y += (f32)outline_height * scale;

					vert_x += cell.w / 2.0f - (f32)outline_width * scale / 2.0f;
					vert_y += cell.h / 2.0f - (f32)outline_height * scale / 2.0f;

					Rect vert_rect;
					vert_rect.x = cell.x + vert_x;
					vert_rect.y = cell.y + vert_y;
					vert_rect.w = 4;
					vert_rect.h = 4;

					draw_rect(vert_rect, BLACK);
				}

				arena_end_temp(scratch);
			}
		} break;
		case View_Inspect: {
		} break;
		}
		ui.current_view = next_view;

		end_drawing();

		glfwSwapBuffers(window);

		scroll_delta = (Vec2) {0};
	}

	glfwTerminate();
	return 0;
}
