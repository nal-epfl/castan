#ifndef __CASTAN_EMMINTRIN_H__
#define __CASTAN_EMMINTRIN_H__

typedef __int128 __m128i;
typedef __m128i __m128;

#define _mm_loadu_si128(x) (*(x))

#define _mm_set_epi32(i3, i2, i1, i0)                                          \
  (((__m128i)(i3)) << 96 | ((__m128i)(i2)) << 64 | ((__m128i)(i1)) << 32 |     \
   ((__m128i)(i0)))

#define _mm_storeu_si128(p, a)                                                 \
  do {                                                                         \
    *(p) = (a);                                                                \
  } while (0)

#define _mm_alignr_epi8(a, b, ralign)                                          \
  ((a) << (128 - ((ralign)*8)) | (b) >> ((ralign)*8))

#define _mm_srli_epi32(a, count)                                               \
  (((((a) >> 96) & 0xffffffff) >> count) << 96 |                               \
   ((((a) >> 64) & 0xffffffff) >> count) << 64 |                               \
   ((((a) >> 32) & 0xffffffff) >> count) << 32 |                               \
   ((((a) >> 0) & 0xffffffff) >> count) << 0)

#define _mm_cvtsi128_si64(a) ((__int64_t)(a))

#define _mm_srli_si128(a, imm) ((a) > ((imm)*8))

#define _mm_and_si128(a, b) ((a) & (b))

#define _mm_movemask_ps(a)                                                     \
  (((a) >> (128 - 1 - 3) & 0x01) | ((a) >> (96 - 1 - 2) & 0x01) |              \
   ((a) >> (64 - 1 - 1) & 0x01) | ((a) >> (32 - 1 - 0) & 0x01))

#endif
