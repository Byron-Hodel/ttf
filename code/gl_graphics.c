typedef u32 Color;

typedef struct {
	Vec2  pos;
	Color color;
} Graphics_Vertex;

typedef struct {
	u32             num_verts;
	u32             num_tris;
	u32             vao;
	u32             vertex_buffer;
	u32             triangle_buffer;
	Graphics_Vertex *mapped_verts;
	u32             *mapped_tris;
	struct {
		u32 handle;
		i32 view_proj_loc;
	} program;
} GL_Graphics_Context;

global const char *VERT_SRC = " \
#version 330 core\n \
in vec3 a_pos; \
in vec4 a_color; \
out vec4 v_color; \
uniform mat4 u_view_proj; \
void main() { \
	gl_Position = u_view_proj * vec4(a_pos, 1.0); \
	v_color = a_color; \
}";

global const char *FRAG_SRC = " \
#version 330 core\n \
in vec4 v_color; \
out vec4 f_color; \
void main() { \
	f_color = v_color; \
}";

global const u32 MAX_VERTICES = 149796;
global const u32 MAX_TRIANGLES = 6*149796;
global GL_Graphics_Context gl_ctx;

global const Color WHITE         = 0xFFFFFFFF;
global const Color LIGHT_GRAY    = 0xFFBFBFBF;
global const Color GRAY          = 0xFF808080;
global const Color DARK_GRAY     = 0xFF404040;
global const Color BLACK         = 0xFF000000;

global const Color RED           = 0xFF0000FF;
global const Color LIGHT_RED     = 0xFF8080FF;
global const Color DARK_RED      = 0xFF000080;

global const Color GREEN         = 0xFF00FF00;
global const Color LIGHT_GREEN   = 0xFF80FF80;
global const Color DARK_GREEN    = 0xFF008000;

global const Color BLUE          = 0xFFFF0000;
global const Color LIGHT_BLUE    = 0xFFFF8080;
global const Color DARK_BLUE     = 0xFF800000;

global const Color YELLOW        = 0xFF00FFFF;
global const Color LIGHT_YELLOW  = 0xFF80FFFF;
global const Color DARK_YELLOW   = 0xFF008080;

global const Color CYAN          = 0xFFFFFF00;
global const Color LIGHT_CYAN    = 0xFFFFFF80;
global const Color DARK_CYAN     = 0xFF808000;

global const Color MAGENTA       = 0xFFFF00FF;
global const Color LIGHT_MAGENTA = 0xFFFF80FF;
global const Color DARK_MAGENTA  = 0xFF800080;

global const Color ORANGE        = 0xFF00A5FF;
global const Color LIGHT_ORANGE  = 0xFF80C8FF;
global const Color DARK_ORANGE   = 0xFF005280;

global const Color TRANSPARENT   = 0x00000000;
global const Color HALF_WHITE    = 0x80FFFFFF;
global const Color HALF_BLACK    = 0x80000000;

function void gl_init(void) {
	glGenBuffers(1, &gl_ctx.vertex_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, gl_ctx.vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(Graphics_Vertex) * MAX_VERTICES, 0, GL_DYNAMIC_DRAW);

	glGenBuffers(1, &gl_ctx.triangle_buffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gl_ctx.triangle_buffer);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(u32) * MAX_TRIANGLES, 0, GL_DYNAMIC_DRAW);

	glGenVertexArrays(1, &gl_ctx.vao);

	glBindVertexArray(gl_ctx.vao);
	glBindBuffer(GL_ARRAY_BUFFER, gl_ctx.vertex_buffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gl_ctx.triangle_buffer);

	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);

	glVertexAttribPointer(0, 2, GL_FLOAT, FALSE, sizeof(Graphics_Vertex), 0);
	glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, TRUE, sizeof(Graphics_Vertex), (void*)8);

	glBindVertexArray(0);

	{
		u32 vert_shader = glCreateShader(GL_VERTEX_SHADER);
		glShaderSource(vert_shader, 1, &VERT_SRC, 0);
		glCompileShader(vert_shader);

		u32 frag_shader = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(frag_shader, 1, &FRAG_SRC, 0);
		glCompileShader(frag_shader);

		i32 frag_compile_status, vert_compile_status;
		glGetShaderiv(frag_shader, GL_COMPILE_STATUS, &frag_compile_status);
		glGetShaderiv(vert_shader, GL_COMPILE_STATUS, &vert_compile_status);

		if (vert_compile_status == 0) {
    	Arena_Temp scratch = begin_scratch(0, 0);

    	i32 len;
    	glGetShaderiv(vert_shader, GL_INFO_LOG_LENGTH, &len);

    	if (len > 0) {
        u8 *log_str = push_array(scratch.arena, u8, (u32)len + 1);
        glGetShaderInfoLog(vert_shader, len + 1, &len, (char*)log_str);
        printf("Vertex Shader Error:\n%s\n", log_str);
    	}

    	end_scratch(scratch);
		}

		if (frag_compile_status == 0) {
    	Arena_Temp scratch = begin_scratch(0, 0);

    	i32 len;
    	glGetShaderiv(frag_shader, GL_INFO_LOG_LENGTH, &len);

    	if (len > 0) {
        u8 *log_str = push_array(scratch.arena, u8, (u32)len + 1);
        glGetShaderInfoLog(frag_shader, len + 1, &len, (char*)log_str);
        printf("Fragment Shader Error:\n%s\n", log_str);
    	}

    	end_scratch(scratch);
		}

		if (vert_compile_status != 0 && frag_compile_status != 0) {
			u32 program = glCreateProgram();
			glAttachShader(program, vert_shader);
			glAttachShader(program, frag_shader);
			glLinkProgram(program);

			i32 program_link_status;
			glGetProgramiv(program, GL_LINK_STATUS, &program_link_status);
			if (program_link_status == 0) {
				printf("Program Linking failed.\n");
				glDeleteProgram(program);
				program = 0;
			}

			glBindAttribLocation(program, 0, "a_pos");
			glBindAttribLocation(program, 1, "a_color");

			gl_ctx.program.handle = program;
			gl_ctx.program.view_proj_loc = glGetUniformLocation(program, "u_view_proj");	
		}	

		glDeleteShader(vert_shader);
		glDeleteShader(frag_shader);
	}
}

function Color color_rgba(u8 r, u8 g, u8 b, u8 a) {
	// assumes little endian
	Color color = (u32)a << 24;
	color |= (u32)b << 16;
	color |= (u32)g << 8;
	color |= (u32)r << 0;
	return color;
}

function void begin_drawing(i32 screen_width, i32 screen_height, i32 fb_width, i32 fb_height) {
	glViewport(0, 0, fb_width, fb_height);
	glScissor(0, 0, fb_width, fb_height);

	glEnable(GL_FRAMEBUFFER_SRGB);

	glClearColor(0.1f, 0.1f, 0.1f, 1.0);
	glClear(GL_COLOR_BUFFER_BIT);

	glUseProgram(gl_ctx.program.handle);

	f32 l = 0.0f;
	f32 r = (f32)screen_width;
	f32 t = 0.0f;
	f32 b = (f32)screen_height;
	f32 n = -1.0f;
	f32 f =  1.0f;

	f32 view_proj[16] = {
		2.0f/(r-l), 0,           0,           0,
		0,          2.0f/(t-b),  0,           0,
		0,          0,          -2.0f/(f-n),  0,
		-(r+l)/(r-l), -(t+b)/(t-b), -(f+n)/(f-n), 1
	};

	glUniformMatrix4fv(gl_ctx.program.view_proj_loc, 1, FALSE, view_proj);


	glBindBuffer(GL_ARRAY_BUFFER, gl_ctx.vertex_buffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gl_ctx.triangle_buffer);
	gl_ctx.num_verts = 0;
	gl_ctx.num_tris = 0;
	gl_ctx.mapped_verts = (Graphics_Vertex*)glMapBufferRange(GL_ARRAY_BUFFER, 0,
	                                                         sizeof(Graphics_Vertex)*MAX_VERTICES,
	                                                         GL_MAP_INVALIDATE_RANGE_BIT |
	                                                         GL_MAP_WRITE_BIT);

	gl_ctx.mapped_tris = (u32*)glMapBufferRange(GL_ELEMENT_ARRAY_BUFFER, 0,
	                                            sizeof(u32)*MAX_TRIANGLES,
	                                            GL_MAP_INVALIDATE_RANGE_BIT |
	                                            GL_MAP_WRITE_BIT);
}

function void end_drawing(void) {
	glUnmapBuffer(GL_ARRAY_BUFFER);
	glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);

	glBindVertexArray(gl_ctx.vao);
	glDrawElements(GL_TRIANGLES, (i32)gl_ctx.num_tris, GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);
}

// `x`, `y`, `w`, & `h` are in framebuffer coordinates
function void set_scissor_rect(i32 x, i32 y, i32 w, i32 h) {
	glScissor(x, y, w, h);
}

function void draw_rect(Rect rect, Color color) {
	ASSERT(gl_ctx.num_verts + 4 < MAX_VERTICES);
	ASSERT(gl_ctx.num_tris + 6 < MAX_TRIANGLES);
	gl_ctx.mapped_verts[gl_ctx.num_verts + 0].pos.x = rect.x;
	gl_ctx.mapped_verts[gl_ctx.num_verts + 0].pos.y = rect.y;
	gl_ctx.mapped_verts[gl_ctx.num_verts + 1].pos.x = rect.x;
	gl_ctx.mapped_verts[gl_ctx.num_verts + 1].pos.y = rect.y + rect.h;
	gl_ctx.mapped_verts[gl_ctx.num_verts + 2].pos.x = rect.x + rect.w;
	gl_ctx.mapped_verts[gl_ctx.num_verts + 2].pos.y = rect.y;
	gl_ctx.mapped_verts[gl_ctx.num_verts + 3].pos.x = rect.x + rect.w;
	gl_ctx.mapped_verts[gl_ctx.num_verts + 3].pos.y = rect.y + rect.h;	

	for (usize i = 0; i < 4; i += 1) {
		gl_ctx.mapped_verts[gl_ctx.num_verts + i].color = color;
	}

	gl_ctx.mapped_tris[gl_ctx.num_tris + 0] = gl_ctx.num_verts + 0;
	gl_ctx.mapped_tris[gl_ctx.num_tris + 1] = gl_ctx.num_verts + 1;
	gl_ctx.mapped_tris[gl_ctx.num_tris + 2] = gl_ctx.num_verts + 2;

	gl_ctx.mapped_tris[gl_ctx.num_tris + 3] = gl_ctx.num_verts + 1;
	gl_ctx.mapped_tris[gl_ctx.num_tris + 4] = gl_ctx.num_verts + 3;
	gl_ctx.mapped_tris[gl_ctx.num_tris + 5] = gl_ctx.num_verts + 2;

	gl_ctx.num_verts += 4;
	gl_ctx.num_tris += 6;
}

function void draw_line(Vec2 p0, Vec2 p1, f32 thickness, Color color) {
	Vec2 delta = vec2_sub(p1, p0);
	Vec2 offset;
	offset.x = -delta.y;
	offset.y = delta.x;
	offset = vec2_normalize(offset);
	offset = vec2_smul(offset, thickness / 2.0f);
	gl_ctx.mapped_verts[gl_ctx.num_verts + 0].pos = vec2_add(p0, offset);
	gl_ctx.mapped_verts[gl_ctx.num_verts + 1].pos = vec2_sub(p0, offset);
	gl_ctx.mapped_verts[gl_ctx.num_verts + 2].pos = vec2_add(p1, offset);
	gl_ctx.mapped_verts[gl_ctx.num_verts + 3].pos = vec2_sub(p1, offset);

	for (usize i = 0; i < 4; i += 1) {
		gl_ctx.mapped_verts[gl_ctx.num_verts + i].color = color;
	}

	gl_ctx.mapped_tris[gl_ctx.num_tris + 0] = gl_ctx.num_verts + 0;
	gl_ctx.mapped_tris[gl_ctx.num_tris + 1] = gl_ctx.num_verts + 1;
	gl_ctx.mapped_tris[gl_ctx.num_tris + 2] = gl_ctx.num_verts + 2;

	gl_ctx.mapped_tris[gl_ctx.num_tris + 3] = gl_ctx.num_verts + 2;
	gl_ctx.mapped_tris[gl_ctx.num_tris + 4] = gl_ctx.num_verts + 1;
	gl_ctx.mapped_tris[gl_ctx.num_tris + 5] = gl_ctx.num_verts + 3;

	gl_ctx.num_verts += 4;
	gl_ctx.num_tris += 6;
}

function void draw_quad_bezier(Vec2 p0, Vec2 p1, Vec2 p3, f32 thickness, Color color) {
	u32 num_segments = 10;
	for (usize i = 0; i < num_segments; i += 1) {
	}
}
