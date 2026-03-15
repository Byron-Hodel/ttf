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

Vec2 vec2_add(Vec2 a, Vec2 b) {
	Vec2 result;
	result.x = a.x + b.x;
	result.y = a.y + b.y;
	return result;
}

Vec3 vec3_add(Vec3 a, Vec3 b) {
	Vec3 result;
	result.x = a.x + b.x;
	result.y = a.y + b.y;
	result.z = a.z + b.z;
	return result;
}

Vec4 vec4_add(Vec4 a, Vec4 b) {
	Vec4 result;
	result.x = a.x + b.x;
	result.y = a.y + b.y;
	result.z = a.z + b.z;
	result.w = a.w + b.w;
	return result;
}

Vec2 vec2_sub(Vec2 a, Vec2 b) {
	Vec2 result;
	result.x = a.x - b.x;
	result.y = a.y - b.y;
	return result;
}

Vec3 vec3_sub(Vec3 a, Vec3 b) {
	Vec3 result;
	result.x = a.x - b.x;
	result.y = a.y - b.y;
	result.z = a.z - b.z;
	return result;
}

Vec4 vec4_sub(Vec4 a, Vec4 b) {
	Vec4 result;
	result.x = a.x - b.x;
	result.y = a.y - b.y;
	result.z = a.z - b.z;
	result.w = a.w - b.w;
	return result;
}

Vec2 vec2_cmul(Vec2 a, Vec2 b) {
	Vec2 result;
	result.x = a.x * b.x;
	result.y = a.y * b.y;
	return result;
}

Vec3 vec3_cmul(Vec3 a, Vec3 b) {
	Vec3 result;
	result.x = a.x * b.x;
	result.y = a.y * b.y;
	result.z = a.z * b.z;
	return result;
}

Vec4 vec4_cmul(Vec4 a, Vec4 b) {
	Vec4 result;
	result.x = a.x * b.x;
	result.y = a.y * b.y;
	result.z = a.z * b.z;
	result.w = a.w * b.w;
	return result;
}

Vec2 vec2_cdiv(Vec2 a, Vec2 b) {
	Vec2 result;
	result.x = a.x / b.x;
	result.y = a.y / b.y;
	return result;
}

Vec3 vec3_cdiv(Vec3 a, Vec3 b) {
	Vec3 result;
	result.x = a.x / b.x;
	result.y = a.y / b.y;
	result.z = a.z / b.z;
	return result;
}

Vec4 vec4_cdiv(Vec4 a, Vec4 b) {
	Vec4 result;
	result.x = a.x / b.x;
	result.y = a.y / b.y;
	result.z = a.z / b.z;
	result.w = a.w / b.w;
	return result;
}

f32 vec2_dot(Vec2 a, Vec2 b) {
	return a.x * b.x + a.y * b.y;
}

f32 vec3_dot(Vec3 a, Vec3 b) {
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

f32 vec4_dot(Vec4 a, Vec4 b) {
	return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

Rect rect_scale(Rect rect, Vec2 scale) {
	Rect result;
	result.pos = rect.pos;
	result.size = vec2_cmul(rect.size, scale);
	return result;
}

// padding = (Vec4) { left, top, right, bottom }
Rect pad_rect(Rect rect, Vec4 padding) {
	Rect result;
	result.x = rect.x + padding.x;
	result.y = rect.y + padding.y;
	result.w = rect.w - 2 * padding.z;
	result.h = rect.h - 2 * padding.w;
	return rect;
}
