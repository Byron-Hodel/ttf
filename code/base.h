#ifndef BASE_H

#include <stdint.h>
#include <stdalign.h>

#if defined(__clang__)
#define debug_trap() __builtin_trap()
#elif defined(_MSC_VER)
#define debug_trap() __debugtrap()
#endif

// useful for grepping codebase
#define global static
#define function static
#define local_persist static

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
typedef intptr_t isize;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef uintptr_t usize;

typedef float f32;
typedef double f64;

typedef i8 b8;
typedef i16 b16;
typedef i32 b32;
typedef i64 b64;

#define TRUE (1)
#define FALSE (0)

#define ASSERT(x) do { if (!(x)) debug_trap(); } while(0)

#define IS_POW2_OR_ZERO(x) (((x) & ((x)-1)) == 0)
#define IS_POW2(x) ((x) != 0 && IS_POW2_OR_ZERO(x))
#define ALIGN_FORWARD_POW2(x, alignment) (((x) + (alignment) - 1) &~ ((alignment) - 1))

#define BASE_H
#endif
