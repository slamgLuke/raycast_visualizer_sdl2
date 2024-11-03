#pragma once
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

#define ASSERT(_e, ...)                                                               \
    if (!(_e)) {                                                                      \
        fprintf(stderr, "Assert failed on line %d in file %s\n", __LINE__, __FILE__); \
        fprintf(stderr, __VA_ARGS__);                                                 \
        exit(1);                                                                      \
    }

typedef float f32;
typedef double f64;
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef size_t usize;
typedef ssize_t isize;

typedef struct {
    f32 x, y;
} vec2;

typedef struct {
    i32 x, y;
} ivec2;
