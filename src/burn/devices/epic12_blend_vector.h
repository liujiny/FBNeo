// Four CV1000 pixels with exact integer tint/factor rounding. Used only by
// dependency-checked CPU batches: all source reads are stable until the join.
#ifndef FBNEO_EPIC12_BLEND_VECTOR_H
#define FBNEO_EPIC12_BLEND_VECTOR_H

#if defined(__SSE2__) && defined(__x86_64__)
static inline __m128i epic12_div31_u16(__m128i product)
{
	// ceil(65536 / 31) is exact for 0..1953, the largest tint product
	// (31 * 63). Keep the tint clamp and subsequent blend division separate.
	return _mm_mulhi_epu16(product, _mm_set1_epi16(2115));
}

class Epic12BlendVector {
	__m128i tint, sa, da, sa_scaled, da_scaled;
	bool untinted, source_full, source_zero, destination_full, destination_zero;

	template<int Mode>
	static __m128i factor(__m128i s, __m128i d, __m128i alpha_value) {
		if (Mode == 1) return s;
		if (Mode == 2) return d;
		if (Mode == 5) return _mm_sub_epi16(_mm_set1_epi16(31), s);
		if (Mode == 6) return _mm_sub_epi16(_mm_set1_epi16(31), d);
		return alpha_value;
	}
	template<int Mode>
	static __m128i contribution(__m128i value, __m128i s, __m128i d,
		__m128i alpha_value, __m128i alpha_scaled, bool full, bool zero) {
		if (Mode == 0) {
			if (full) return value;
			if (zero) return _mm_setzero_si128();
            // Fixed alpha is five bits. After the full/zero cases it is
            // 1..30, so alpha*2115 fits unsigned16. Reassociate the exact
            // reciprocal product without changing either rounding stage.
            return _mm_mulhi_epu16(value, alpha_scaled);
        }
		return epic12_div31_u16(_mm_mullo_epi16(value, factor<Mode>(s, d, alpha_value)));
	}
	template<int SourceMode, int DestinationMode>
	__m128i half(__m128i s, __m128i d) const {
		const __m128i limit = _mm_set1_epi16(31);
		if (!untinted) s = _mm_min_epi16(epic12_div31_u16(_mm_mullo_epi16(s, tint)), limit);
		__m128i a = contribution<SourceMode>(s, s, d, sa, sa_scaled, source_full, source_zero);
		if (DestinationMode == 2) {
			// Match clr_add_with_clr_square: its green/blue
			// terms use the red source contribution in the original renderer.
			a = _mm_shufflelo_epi16(a, _MM_SHUFFLE(3, 2, 2, 2));
			a = _mm_shufflehi_epi16(a, _MM_SHUFFLE(3, 2, 2, 2));
		}
		const __m128i b = contribution<DestinationMode>(d, s, d, da, da_scaled, destination_full, destination_zero);
		return _mm_min_epi16(_mm_add_epi16(a, b), limit);
	}

public:
	Epic12BlendVector(const clr_t &colour, unsigned source_alpha, unsigned destination_alpha)
		: tint(_mm_set_epi16(0, colour.r, colour.g, colour.b, 0, colour.r, colour.g, colour.b)),
		  sa(_mm_set1_epi16(source_alpha)), da(_mm_set1_epi16(destination_alpha)),
          sa_scaled(_mm_set1_epi16(source_alpha * 2115U)),
          da_scaled(_mm_set1_epi16(destination_alpha * 2115U)),
		  untinted((colour.r == 31 || colour.r == 32) && (colour.g == 31 || colour.g == 32)
			&& (colour.b == 31 || colour.b == 32)),
		  source_full(source_alpha == 31), source_zero(source_alpha == 0),
		  destination_full(destination_alpha == 31), destination_zero(destination_alpha == 0) {}

	template<int SourceMode, int DestinationMode>
	__m128i blend(__m128i source, __m128i destination) const {
		const __m128i channels = _mm_set1_epi32(0x001f1f1f);
		const __m128i zero = _mm_setzero_si128();
		const __m128i s = _mm_and_si128(_mm_srli_epi32(source, 3), channels);
		const __m128i d = _mm_and_si128(_mm_srli_epi32(destination, 3), channels);
		const __m128i lo = half<SourceMode, DestinationMode>(_mm_unpacklo_epi8(s, zero), _mm_unpacklo_epi8(d, zero));
		const __m128i hi = half<SourceMode, DestinationMode>(_mm_unpackhi_epi8(s, zero), _mm_unpackhi_epi8(d, zero));
		const __m128i rgb = _mm_slli_epi32(_mm_packus_epi16(lo, hi), 3);
		return _mm_or_si128(rgb, _mm_and_si128(source, _mm_set1_epi32(0x20000000)));
	}
};
#endif
#endif
