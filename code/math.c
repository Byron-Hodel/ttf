typedef struct {
	union {
		struct {
			f32 x, y;
		};
		f32 v[2];
	};
} Vec2;

typedef struct {
	union {
		struct {
			f32 x, y, z;
		};
		f32 v[4];
	};
} Vec3;

typedef struct {
	union {
		struct {
			f32 x, y, z, w;
		};
		f32 v[4];
	};
} Vec4;

typedef struct {
	union {
		struct {
			f32 x, y, w, h;
		};
		struct {
			Vec2 pos;
			Vec2 size;
		};
		Vec4 v4;
		f32  v[4];
	};
} Rect;

function Vec2 vec2_add(Vec2 a, Vec2 b) {
	Vec2 result;
	result.x = a.x + b.x;
	result.y = a.y + b.y;
	return result;
}

function Vec3 vec3_add(Vec3 a, Vec3 b) {
	Vec3 result;
	result.x = a.x + b.x;
	result.y = a.y + b.y;
	result.z = a.z + b.z;
	return result;
}

function Vec4 vec4_add(Vec4 a, Vec4 b) {
	Vec4 result;
	result.x = a.x + b.x;
	result.y = a.y + b.y;
	result.z = a.z + b.z;
	result.w = a.w + b.w;
	return result;
}

function Vec2 vec2_sub(Vec2 a, Vec2 b) {
	Vec2 result;
	result.x = a.x - b.x;
	result.y = a.y - b.y;
	return result;
}

function Vec3 vec3_sub(Vec3 a, Vec3 b) {
	Vec3 result;
	result.x = a.x - b.x;
	result.y = a.y - b.y;
	result.z = a.z - b.z;
	return result;
}

function Vec4 vec4_sub(Vec4 a, Vec4 b) {
	Vec4 result;
	result.x = a.x - b.x;
	result.y = a.y - b.y;
	result.z = a.z - b.z;
	result.w = a.w - b.w;
	return result;
}

function Vec2 vec2_cmul(Vec2 a, Vec2 b) {
	Vec2 result;
	result.x = a.x * b.x;
	result.y = a.y * b.y;
	return result;
}

function Vec3 vec3_cmul(Vec3 a, Vec3 b) {
	Vec3 result;
	result.x = a.x * b.x;
	result.y = a.y * b.y;
	result.z = a.z * b.z;
	return result;
}

function Vec4 vec4_cmul(Vec4 a, Vec4 b) {
	Vec4 result;
	result.x = a.x * b.x;
	result.y = a.y * b.y;
	result.z = a.z * b.z;
	result.w = a.w * b.w;
	return result;
}

function Vec2 vec2_smul(Vec2 v, f32 s) {
	Vec2 result;
	result.x = v.x * s;
	result.y = v.y * s;
	return result;
}

function Vec3 vec3_smul(Vec3 v, f32 s) {
	Vec3 result;
	result.x = v.x * s;
	result.y = v.y * s;
	result.z = v.z * s;
	return result;
}

function Vec4 vec4_smul(Vec4 v, f32 s) {
	Vec4 result;
	result.x = v.x * s;
	result.y = v.y * s;
	result.z = v.z * s;
	result.w = v.w * s;
	return result;
}

function Vec2 vec2_cdiv(Vec2 a, Vec2 b) {
	Vec2 result;
	result.x = a.x / b.x;
	result.y = a.y / b.y;
	return result;
}

function Vec3 vec3_cdiv(Vec3 a, Vec3 b) {
	Vec3 result;
	result.x = a.x / b.x;
	result.y = a.y / b.y;
	result.z = a.z / b.z;
	return result;
}

function Vec4 vec4_cdiv(Vec4 a, Vec4 b) {
	Vec4 result;
	result.x = a.x / b.x;
	result.y = a.y / b.y;
	result.z = a.z / b.z;
	result.w = a.w / b.w;
	return result;
}

function Vec2 vec2_sdiv(Vec2 v, f32 s) {
	Vec2 result;
	result.x = v.x / s;
	result.y = v.y / s;
	return result;
}

function Vec3 vec3_sdiv(Vec3 v, f32 s) {
	Vec3 result;
	result.x = v.x / s;
	result.y = v.y / s;
	result.z = v.z / s;
	return result;
}

function Vec4 vec4_sdiv(Vec4 v, f32 s) {
	Vec4 result;
	result.x = v.x / s;
	result.y = v.y / s;
	result.z = v.z / s;
	result.w = v.w / s;
	return result;
}

function f32 vec2_dot(Vec2 a, Vec2 b) {
	return a.x * b.x + a.y * b.y;
}

function f32 vec3_dot(Vec3 a, Vec3 b) {
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

function f32 vec4_dot(Vec4 a, Vec4 b) {
	return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

function Vec2 vec2_normalize(Vec2 v) {
	return vec2_sdiv(v, sqrtf(vec2_dot(v, v)));
}

function Vec3 vec3_normalize(Vec3 v) {
	return vec3_sdiv(v, sqrtf(vec3_dot(v, v)));
}

function Vec4 vec4_normalize(Vec4 v) {
	return vec4_sdiv(v, sqrtf(vec4_dot(v, v)));
}


function Rect rect_scale(Rect rect, Vec2 scale) {
	Rect result;
	result.pos = rect.pos;
	result.size = vec2_cmul(rect.size, scale);
	return result;
}

// padding = (Vec4) { left, top, right, bottom }
function Rect pad_rect(Rect rect, Vec4 padding) {
	Rect result;
	result.x = rect.x + padding.x;
	result.y = rect.y + padding.y;
	result.w = rect.w - 2 * padding.z;
	result.h = rect.h - 2 * padding.w;
	return rect;
}
