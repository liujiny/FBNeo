// Exact integer path for the fixed source/destination alpha blend used by
// CV1000 effects. Keep the two source rounding/clamping stages in a compact
// table, then saturate all three packed colour channels together.
#ifndef FBNEO_EPIC12_FAST_BLIT_H
#define FBNEO_EPIC12_FAST_BLIT_H

static UINT8 epic12_source_scale[64][32][32];

static void epic12_init_blend_tables()
{
	for (int tint = 0; tint < 64; tint++)
		for (int alpha = 0; alpha < 32; alpha++)
			for (int c = 0; c < 32; c++)
				epic12_source_scale[tint][alpha][c] =
					epic12_device_colrtable[epic12_device_colrtable[c][tint]][alpha];
}

static inline UINT32 epic12_packed_add(UINT32 s, UINT32 d)
{
	// Channels occupy bits 3..7 of separate bytes. Their carry bits fit in
	// the gaps, so one word add and a mask implement three saturated adds.
	UINT32 sum = s + d;
	UINT32 carry = sum & 0x01010100;
	return (sum | (carry - (carry >> 5))) & 0x00f8f8f8;
}

enum {
	EPIC12_DEST_SCALED,
	EPIC12_DEST_FULL,
	EPIC12_DEST_ZERO
};

template<bool FlipX, bool Transparent, int DestinationMode, bool IdentitySource>
static void epic12_blend_fixed(BLIT_PARAMS)
{
	if (FlipX) src_x += dimx - 1;
	int yf = flipy ? -1 : 1;
	if (flipy) src_y += dimy - 1;

	// Match the existing source-wrap rejection before destination clipping.
	if (FlipX ? ((src_x & 0x1fff) < ((src_x - (dimx - 1)) & 0x1fff))
	          : ((src_x & 0x1fff) > ((src_x + (dimx - 1)) & 0x1fff))) return;
	int x0 = dst_x_start < clip->min_x ? clip->min_x - dst_x_start : 0;
	int y0 = dst_y_start < clip->min_y ? clip->min_y - dst_y_start : 0;
	int x1 = dimx, y1 = dimy;
	if (dst_x_start + x1 - 1 > clip->max_x) x1 = clip->max_x - dst_x_start + 1;
	if (dst_y_start + y1 - 1 > clip->max_y) y1 = clip->max_y - dst_y_start + 1;
	if (x1 <= x0 || y1 <= y0) return;
	epic12_device_blit_delay += (y1 - y0) * (x1 - x0);
	const UINT8 *red = epic12_source_scale[tint_clr->r][s_alpha];
	const UINT8 *green = epic12_source_scale[tint_clr->g][s_alpha];
	const UINT8 *blue = epic12_source_scale[tint_clr->b][s_alpha];
	const UINT8 *dest = epic12_device_colrtable[d_alpha];
	for (int y = y0; y < y1; y++) {
		UINT32 *out = m_bitmaps + (dst_y_start + y) * 0x2000 + dst_x_start + x0;
		const UINT32 *in = gfx + (((src_y + yf * y) & 0x0fff) * 0x2000) + src_x + (FlipX ? -x0 : x0);
		for (int x = x0; x < x1; x++, out++, in += FlipX ? -1 : 1) {
			UINT32 pen = *in;
			if (Transparent && !(pen & 0x20000000)) continue;
			UINT32 s;
			if (IdentitySource) s = pen & 0x00f8f8f8;
			else s = (red[(pen >> 19) & 31] << 19) |
			         (green[(pen >> 11) & 31] << 11) | (blue[(pen >> 3) & 31] << 3);
			if (DestinationMode == EPIC12_DEST_ZERO) {
				// alpha 0 contributes no destination colour. Keep the
				// source read/write order for overlapping VRAM and retain
				// the source transparency bit, even when s is black.
				*out = s | (pen & 0x20000000);
			} else {
				UINT32 d = *out;
				if (DestinationMode == EPIC12_DEST_FULL) d &= 0x00f8f8f8;
				else d = (dest[(d >> 19) & 31] << 19) |
				         (dest[(d >> 11) & 31] << 11) | (dest[(d >> 3) & 31] << 3);
				*out = epic12_packed_add(s, d) | (pen & 0x20000000);
			}
		}
	}
}

static void epic12_draw_fixed(int flipx, int transparent, BLIT_PARAMS)
{
	// For every 5-bit component c, min(31, c*tint/31) == c at
	// tint 31 and 32. With full source alpha the original two rounding
	// stages are therefore an identity, including saturated white.
	// Select once per sprite; the pixel loop needs no colour table reads
	// for this common untinted additive blend. Other values keep the LUT.
	const bool identity_source = s_alpha == 31 &&
		(tint_clr->r == 31 || tint_clr->r == 32) &&
		(tint_clr->g == 31 || tint_clr->g == 32) &&
		(tint_clr->b == 31 || tint_clr->b == 32);
#define EPIC12_FIXED_CALL(f,t,d) do { \
	if (identity_source) epic12_blend_fixed<f,t,d,true>(clip,gfx,src_x,src_y,dst_x_start,dst_y_start,dimx,dimy,flipy,s_alpha,d_alpha,tint_clr); \
	else epic12_blend_fixed<f,t,d,false>(clip,gfx,src_x,src_y,dst_x_start,dst_y_start,dimx,dimy,flipy,s_alpha,d_alpha,tint_clr); \
} while (0)
	if (d_alpha == 0) {
		if (flipx) { if (transparent) EPIC12_FIXED_CALL(true,true,EPIC12_DEST_ZERO); else EPIC12_FIXED_CALL(true,false,EPIC12_DEST_ZERO); }
		else { if (transparent) EPIC12_FIXED_CALL(false,true,EPIC12_DEST_ZERO); else EPIC12_FIXED_CALL(false,false,EPIC12_DEST_ZERO); }
	} else if (d_alpha == 31) {
		if (flipx) { if (transparent) EPIC12_FIXED_CALL(true,true,EPIC12_DEST_FULL); else EPIC12_FIXED_CALL(true,false,EPIC12_DEST_FULL); }
		else { if (transparent) EPIC12_FIXED_CALL(false,true,EPIC12_DEST_FULL); else EPIC12_FIXED_CALL(false,false,EPIC12_DEST_FULL); }
	} else {
		if (flipx) { if (transparent) EPIC12_FIXED_CALL(true,true,EPIC12_DEST_SCALED); else EPIC12_FIXED_CALL(true,false,EPIC12_DEST_SCALED); }
		else { if (transparent) EPIC12_FIXED_CALL(false,true,EPIC12_DEST_SCALED); else EPIC12_FIXED_CALL(false,false,EPIC12_DEST_SCALED); }
	}
#undef EPIC12_FIXED_CALL
}
#endif
