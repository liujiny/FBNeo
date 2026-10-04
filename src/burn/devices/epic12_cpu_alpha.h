// Cached transparency bounds for the CPU CV1000 blitter. Inspired by Salvia's
// epic12_gpu_alpha.h; no GPU atlas, Xenos layout or VMX code is required here.
// Only the ordered list owner accesses this cache, never the raster workers.
#ifndef FBNEO_EPIC12_CPU_ALPHA_H
#define FBNEO_EPIC12_CPU_ALPHA_H

class Epic12CpuAlpha {
	enum { ALPHA_PAGE_SIDE = 32, SETS = 64, WAYS = 2, QUERIES = 4 };
	struct Page {
		UINT32 tag, rows[ALPHA_PAGE_SIDE], groups[ALPHA_PAGE_SIDE / 8];
		UINT32 keys[QUERIES], answers[QUERIES];
		unsigned next_query;
	};
	Page pages[SETS * WAYS];
	unsigned char next[SETS];

	static unsigned set_for(UINT32 tag) { return (tag * 0x9e3779b1U) >> 26; }
	static UINT32 mask(int first, int last) {
		return (~0U << first) & (~0U >> (31 - last));
	}
	static int first_bit(UINT32 bits) {
#if defined(__GNUC__) || defined(__clang__)
		return __builtin_ctz(bits); // callers exclude zero
#else
		int n = 0; while (!(bits & 1)) { bits >>= 1; ++n; } return n;
#endif
	}
	static int last_bit(UINT32 bits) {
#if defined(__GNUC__) || defined(__clang__)
		return 31 - __builtin_clz(bits);
#else
		int n = 0; while (bits >>= 1) ++n; return n;
#endif
	}
	Page &get(const UINT32 *vram, UINT32 tag) {
		const unsigned set = set_for(tag);
		Page *p = pages + set * WAYS;
		for (unsigned i = 0; i < WAYS; ++i)
			if (p[i].tag == tag) return p[i];
		Page &page = p[next[set]++ & (WAYS - 1)];
		page.tag = tag;
		page.next_query = 0;
		for (unsigned i = 0; i < QUERIES; ++i) page.keys[i] = ~0U;
		const UINT32 *source = vram + (tag >> 8) * ALPHA_PAGE_SIDE * 8192
			+ (tag & 255) * ALPHA_PAGE_SIDE;
		for (int y = 0; y < ALPHA_PAGE_SIDE; ++y) {
			UINT32 bits = 0;
#if defined(__SSE2__) && defined(__x86_64__)
			for (int x = 0; x < ALPHA_PAGE_SIDE; x += 4) {
				const __m128i pen = _mm_loadu_si128((const __m128i *)(source + x));
				bits |= (UINT32)_mm_movemask_ps(_mm_castsi128_ps(_mm_slli_epi32(pen, 2))) << x;
			}
#else
			for (int x = 0; x < ALPHA_PAGE_SIDE; ++x) bits |= ((source[x] >> 29) & 1U) << x;
#endif
			page.rows[y] = bits;
			if (!(y & 7)) page.groups[y >> 3] = bits;
			else page.groups[y >> 3] |= bits;
			source += 8192;
		}
		return page;
	}
	static bool bounds(Page &page, int x0, int y0, int x1, int y1, rectangle &out) {
		const UINT32 key = x0 | (y0 << 5) | (x1 << 10) | (y1 << 15);
		UINT32 answer = 0;
		unsigned i;
		for (i = 0; i < QUERIES; ++i) if (page.keys[i] == key) {
			answer = page.answers[i];
			break;
		}
		if (i == QUERIES) {
			const UINT32 selected = mask(x0, x1);
			UINT32 columns = 0;
			int top = ALPHA_PAGE_SIDE, bottom = 0;
			for (int y = y0; y <= y1; ++y) {
				if (!(page.groups[y >> 3] & selected)) { y |= 7; continue; }
				const UINT32 bits = page.rows[y] & selected;
				if (!bits) continue;
				if (top == ALPHA_PAGE_SIDE) top = y;
				bottom = y;
				columns |= bits;
			}
			if (columns) answer = 0x100000U | first_bit(columns) | (top << 5)
				| (last_bit(columns) << 10) | (bottom << 15);
			const unsigned slot = page.next_query++ & (QUERIES - 1);
			page.keys[slot] = key;
			page.answers[slot] = answer;
		}
		if (!answer) return false;
		out.set(answer & 31, (answer >> 10) & 31, (answer >> 5) & 31, (answer >> 15) & 31);
		return true;
	}

public:
	Epic12CpuAlpha() { clear(); }
	void clear() {
		for (unsigned i = 0; i < SETS * WAYS; ++i) pages[i].tag = ~0U;
		memset(next, 0, sizeof(next));
	}
	void invalidate(const rectangle &r) {
		if (r.min_x > r.max_x || r.min_y > r.max_y) return;
		if (r.min_x < 0 || r.max_x >= 8192 || r.min_y < 0 || r.max_y >= 4096) {
			clear(); return;
		}
		// Invalidate resident entries rather than walking potentially thousands
		// of nonresident pages after a large upload or software fallback.
		const int x0 = r.min_x >> 5, x1 = r.max_x >> 5;
		const int y0 = r.min_y >> 5, y1 = r.max_y >> 5;
		// For a small write, probe only the affected sets. Large rectangles
		// still scan residents, bounding work independently of upload area.
		if ((x1 - x0 + 1) * (y1 - y0 + 1) < SETS) {
			for (int y = y0; y <= y1; ++y) for (int x = x0; x <= x1; ++x) {
				const UINT32 tag = y * 256 + x;
				Page *p = pages + set_for(tag) * WAYS;
				for (unsigned i = 0; i < WAYS; ++i)
					if (p[i].tag == tag) p[i].tag = ~0U;
			}
			return;
		}
		for (unsigned i = 0; i < SETS * WAYS; ++i) {
			const UINT32 tag = pages[i].tag;
			if (tag == ~0U) continue;
			const int x = tag & 255, y = tag >> 8;
			if (x >= x0 && x <= x1 && y >= y0 && y <= y1) pages[i].tag = ~0U;
		}
	}
	// Input is a nonwrapping source rectangle in physical VRAM. Empty results
	// leave out untouched, including on repeated cached queries.
	bool trim(const UINT32 *vram, const rectangle &r, rectangle &out) {
		bool found = false;
		for (int py = r.min_y >> 5; py <= r.max_y >> 5; ++py)
			for (int px = r.min_x >> 5; px <= r.max_x >> 5; ++px) {
				const int ox = px * ALPHA_PAGE_SIDE, oy = py * ALPHA_PAGE_SIDE;
				const int x0 = r.min_x > ox ? r.min_x - ox : 0;
				const int y0 = r.min_y > oy ? r.min_y - oy : 0;
				const int x1 = r.max_x < ox + 31 ? r.max_x - ox : 31;
				const int y1 = r.max_y < oy + 31 ? r.max_y - oy : 31;
				rectangle part;
				if (!bounds(get(vram, py * 256 + px), x0, y0, x1, y1, part)) continue;
				part.min_x += ox; part.max_x += ox; part.min_y += oy; part.max_y += oy;
				if (!found) { out = part; found = true; }
				else {
					if (part.min_x < out.min_x) out.min_x = part.min_x;
					if (part.max_x > out.max_x) out.max_x = part.max_x;
					if (part.min_y < out.min_y) out.min_y = part.min_y;
					if (part.max_y > out.max_y) out.max_y = part.max_y;
				}
			}
		return found;
	}
};
#endif
