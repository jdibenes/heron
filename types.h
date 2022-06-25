//*************************************************************************************************
// Project Heron - GBA Emulator
// jcds (jdibenes@outlook.com)
// 2013
//*************************************************************************************************

#pragma once

#include <stdint.h>

typedef int8_t  s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef union                         {s8  c; u8  b;} t8;
typedef union {struct {t8  b0, b1;} b; s16 s; u16 w;} t16;
typedef union {struct {t16 w0, w1;} w; s32 i; u32 d;} t32;
typedef union {struct {t32 d0, d1;} d; s64 l; u64 q;} t64;

#define BIT(b)          (1U << (b))
#define BITTEST(v, b)   (((v) & BIT(b)) != 0U)
#define SIGNEX(s, b)    ((s & (BIT(b) - 1)) | ~((s & BIT(b)) - 1))
#define SUBVAL(v, b, m) (((v) >> (b)) & (m))
#define NEGATE(v)       (~(v) + 1U)
#define ALIGN(m, w)     ((m) & ~((w) - 1U))
