// Copy one wrapped EPIC12 VRAM row to the software video output buffer.
#ifndef FBNEO_EPIC12_SCREEN_COPY_H
#define FBNEO_EPIC12_SCREEN_COPY_H
#include <stdint.h>
#include <string.h>
#if defined(__SSE2__) && defined(__x86_64__)
#include <emmintrin.h>
#endif

static inline UINT16 epic12_screen_rgb565(UINT32 color)
{
	return ((color >> 8) & 0xf800) | ((color >> 5) & 0x07e0) | ((color >> 3) & 0x001f);
}

static inline bool epic12_screen_row_overlap(const void *dst, size_t bytes, const UINT32 *src)
{
	const uintptr_t d = (uintptr_t)dst, s = (uintptr_t)src;
	return d >= s ? d - s < 0x2000 * sizeof(*src) : s - d < bytes;
}

#if defined(__SSE2__) && defined(__x86_64__)
static inline __m128i epic12_screen_pack565(__m128i color)
{
	const __m128i red = _mm_and_si128(_mm_srli_epi32(color, 8), _mm_set1_epi32(0xf800));
	const __m128i green = _mm_and_si128(_mm_srli_epi32(color, 5), _mm_set1_epi32(0x07e0));
	const __m128i blue = _mm_and_si128(_mm_srli_epi32(color, 3), _mm_set1_epi32(0x001f));
	return _mm_or_si128(_mm_or_si128(red, green), blue);
}
#ifdef EPIC12_SCREEN_TEST
static unsigned epic12_screen_simd_blocks;
#endif
#endif

static void epic12_screen_row565(UINT8 *dst, const UINT32 *src, unsigned offset, int width)
{
	if (width <= 0) return;
	offset &= 0x1fff;
	// Preserve sequential reads/writes if an output buffer aliases VRAM.
	if (epic12_screen_row_overlap(dst, (size_t)width * 2, src)) {
		for (int x = 0; x < width; x++) {
			const UINT16 color = epic12_screen_rgb565(src[(offset + (unsigned)x) & 0x1fff]);
			memcpy(dst + (size_t)x * 2, &color, sizeof(color));
		}
		return;
	}
	while (width > 0) {
		int count = 0x2000 - offset;
		if (count > width) count = width;
		int x = 0;
#if defined(__SSE2__) && defined(__x86_64__)
		const __m128i bias32 = _mm_set1_epi32(0x8000);
		const __m128i bias16 = _mm_set1_epi16((short)-32768);
		for (; x + 8 <= count; x += 8) {
			const __m128i a = epic12_screen_pack565(_mm_loadu_si128((const __m128i *)(src + offset + x)));
			const __m128i b = epic12_screen_pack565(_mm_loadu_si128((const __m128i *)(src + offset + x + 4)));
			// SSE2 packs signed words. Bias into [-32768,32767], then undo
			// the bias after packing so RGB565 values above 0x7fff survive.
			const __m128i packed = _mm_packs_epi32(_mm_sub_epi32(a, bias32), _mm_sub_epi32(b, bias32));
			_mm_storeu_si128((__m128i *)(dst + (size_t)x * 2), _mm_xor_si128(packed, bias16));
#ifdef EPIC12_SCREEN_TEST
			++epic12_screen_simd_blocks;
#endif
		}
#endif
		for (; x < count; x++) {
			const UINT16 color = epic12_screen_rgb565(src[offset + x]);
			memcpy(dst + (size_t)x * 2, &color, sizeof(color));
		}
		dst += (size_t)count * 2;
		width -= count;
		offset = 0;
	}
}

static void epic12_screen_row32(UINT32 *dst, const UINT32 *src, unsigned offset, int width)
{
	if (width <= 0) return;
	offset &= 0x1fff;
	if (epic12_screen_row_overlap(dst, (size_t)width * 4, src)) {
		for (int x = 0; x < width; x++)
			dst[x] = src[(offset + (unsigned)x) & 0x1fff];
		return;
	}
	while (width > 0) {
		int count = 0x2000 - offset;
		if (count > width) count = width;
		memcpy(dst, src + offset, (size_t)count * sizeof(*dst));
		dst += count;
		width -= count;
		offset = 0;
	}
}
#endif
