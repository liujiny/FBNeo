// Preserve SH3 RAM wrapping while copying contiguous command payloads. The
// upload format is native 16-bit tRGB555 expanded to the existing 32-bit VRAM.
#ifndef FBNEO_EPIC12_UPLOAD_H
#define FBNEO_EPIC12_UPLOAD_H

#if defined(__SSE2__) && defined(__x86_64__)
#include <emmintrin.h>
static inline __m128i epic12_expand_words4(__m128i words)
{
	const __m128i marker = _mm_slli_epi32(_mm_and_si128(words, _mm_set1_epi32(0x8000)), 14);
	const __m128i red = _mm_slli_epi32(_mm_and_si128(words, _mm_set1_epi32(0x7c00)), 9);
	const __m128i green = _mm_slli_epi32(_mm_and_si128(words, _mm_set1_epi32(0x03e0)), 6);
	const __m128i blue = _mm_slli_epi32(_mm_and_si128(words, _mm_set1_epi32(0x001f)), 3);
	return _mm_or_si128(_mm_or_si128(marker, red), _mm_or_si128(green, blue));
}
#endif

static inline void epic12_expand_words(UINT32 *out, const UINT16 *in, UINT32 count)
{
	UINT32 i = 0;
#if defined(__SSE2__) && defined(__x86_64__)
	const __m128i zero = _mm_setzero_si128();
	for (; i + 8 <= count; i += 8) {
		const __m128i words = _mm_loadu_si128((const __m128i *)(in + i));
		_mm_storeu_si128((__m128i *)(out + i), epic12_expand_words4(_mm_unpacklo_epi16(words, zero)));
		_mm_storeu_si128((__m128i *)(out + i + 4), epic12_expand_words4(_mm_unpackhi_epi16(words, zero)));
	}
#endif
	for (; i < count; ++i) {
		const UINT32 pen = in[i];
		out[i] = ((pen & 0x8000) << 14) | ((pen & 0x7c00) << 9)
			| ((pen & 0x03e0) << 6) | ((pen & 0x001f) << 3);
	}
}

static inline void epic12_copy_payload(UINT16 *out, const UINT16 *in,
	UINT32 *addr, UINT32 ram_mask, UINT32 count)
{
	const UINT32 ram_words = (ram_mask >> 1) + 1;
	while (count) {
		const UINT32 offset = (*addr & ram_mask) >> 1;
		const UINT32 span = count < ram_words - offset ? count : ram_words - offset;
		memcpy(out + offset, in + offset, span * sizeof(*in));
		*addr += span * 2; count -= span;
	}
}

static inline void epic12_upload_row(UINT32 *out, const UINT16 *in,
	UINT32 *addr, UINT32 ram_mask, UINT32 count)
{
	const UINT32 ram_words = (ram_mask >> 1) + 1;
	while (count) {
		const UINT32 offset = (*addr & ram_mask) >> 1;
		const UINT32 span = count < ram_words - offset ? count : ram_words - offset;
		epic12_expand_words(out, in + offset, span);
		*addr += span * 2; out += span; count -= span;
	}
}
#endif
