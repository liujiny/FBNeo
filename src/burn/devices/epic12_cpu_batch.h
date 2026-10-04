// PS4 CV1000 raster batches. The list owner retains all device state and
// timing. Helpers write disjoint destination rows and never mutate commands,
// source pixels, alpha metadata or the legacy delay counter.
#ifndef FBNEO_EPIC12_CPU_BATCH_H
#define FBNEO_EPIC12_CPU_BATCH_H

#include "epic12_cpu_alpha.h"
#include "epic12_blend_vector.h"

static void epic12_cpu_job(INT32 first, INT32 last, INT32 index);

class Epic12CpuBatch {
	enum { CAPACITY = 256, MIN_PIXELS_PER_LANE = 16384 };
	struct Command {
		int sx, sy, x, y, w, h;
		bool flipx, flipy, transparent, raw, identity, source_dest;
		UINT8 sa, da;
		clr_t tint;
	};
	Command commands[CAPACITY];
	unsigned count;
	UINT64 pixels;
	UINT32 *vram;
	rectangle sources, destinations;
	Epic12CpuAlpha alpha;
	BurnRenderWorker workers[2];
	bool attempted[2], available[2];
	int requested, row_work[4097];

	static bool intersects(const rectangle &a, const rectangle &b) {
		return a.min_x <= b.max_x && b.min_x <= a.max_x
			&& a.min_y <= b.max_y && b.min_y <= a.max_y;
	}
	static void extend(rectangle &a, const rectangle &b) {
		if (b.min_x < a.min_x) a.min_x = b.min_x;
		if (b.max_x > a.max_x) a.max_x = b.max_x;
		if (b.min_y < a.min_y) a.min_y = b.min_y;
		if (b.max_y > a.max_y) a.max_y = b.max_y;
	}
	static rectangle source_rect(const Command &c) {
		return rectangle(c.sx, c.sx + c.w - 1, c.sy, c.sy + c.h - 1);
	}
	static rectangle destination_rect(const Command &c) {
		return rectangle(c.x, c.x + c.w - 1, c.y, c.y + c.h - 1);
	}
	bool depends_on_batch(const rectangle &src, const rectangle &dst) const {
		// Aggregate boxes reject the common case cheaply. Their empty gaps
		// are not dependencies: only actual pending reads/writes need a join.
		const bool raw = intersects(src, destinations);
		const bool war = intersects(dst, sources);
		if (!raw && !war) return false;
		for (unsigned i = 0; i < count; ++i) {
			if (raw && intersects(src, destination_rect(commands[i]))) return true;
			if (war && intersects(dst, source_rect(commands[i]))) return true;
		}
		return false;
	}
	template<int DestinationMode>
	void draw_command(const Command &c, int first, int last) {
		const int y0 = first > c.y ? first - c.y : 0;
		const int y1 = last < c.y + c.h ? last - c.y : c.h;
		if (y0 >= y1) return;
		const UINT8 *red = epic12_source_scale[c.tint.r][c.sa];
		const UINT8 *green = epic12_source_scale[c.tint.g][c.sa];
		const UINT8 *blue = epic12_source_scale[c.tint.b][c.sa];
		const UINT8 *dest = epic12_device_colrtable[c.da];
	#if defined(__SSE2__) && defined(__x86_64__)
		const Epic12BlendVector vector(c.tint, c.sa, c.da);
	#endif
		for (int y = y0; y < y1; ++y) {
			UINT32 *out = vram + (c.y + y) * 8192 + c.x;
			const UINT32 *in = vram + (c.sy + (c.flipy ? c.h - 1 - y : y)) * 8192 + c.sx;
			if (c.raw && !c.transparent && !c.flipx) {
				memcpy(out, in, c.w * sizeof(*out));
				continue;
			}
			int x = 0;
#if defined(__SSE2__) && defined(__x86_64__)
			// Whole-batch dependency checks guarantee disjoint source/destination
			// storage, including flipped rows and reads made by other helpers.
			if (DestinationMode == 0 && (c.raw || (c.identity && (c.da == 0 || c.da == 31)))) {
				const __m128i colour = _mm_set1_epi32(0x00f8f8f8);
				const __m128i marker = _mm_set1_epi32(0x20000000);
				for (; x + 4 <= c.w; x += 4) {
					__m128i pen = _mm_loadu_si128((const __m128i *)(in + (c.flipx ? c.w - x - 4 : x)));
					if (c.flipx) pen = _mm_shuffle_epi32(pen, _MM_SHUFFLE(0, 1, 2, 3));
					const __m128i valid = _mm_srai_epi32(_mm_slli_epi32(pen, 2), 31);
					const int visible = _mm_movemask_epi8(valid);
					if (c.transparent && !visible) continue;
					__m128i old = _mm_setzero_si128();
					if (c.da || (c.transparent && visible != 0xffff))
						old = _mm_loadu_si128((const __m128i *)(out + x));
					__m128i result = pen;
					if (!c.raw) {
						result = _mm_and_si128(pen, colour);
						if (c.da) result = _mm_and_si128(_mm_adds_epu8(result, _mm_and_si128(old, colour)), colour);
						result = _mm_or_si128(result, _mm_and_si128(pen, marker));
					}
					if (c.transparent && visible != 0xffff)
						result = _mm_or_si128(_mm_and_si128(valid, result), _mm_andnot_si128(valid, old));
					_mm_storeu_si128((__m128i *)(out + x), result);
				}
			} else {
				for (; x + 4 <= c.w; x += 4) {
					__m128i pen = _mm_loadu_si128((const __m128i *)(in + (c.flipx ? c.w - x - 4 : x)));
					if (c.flipx) pen = _mm_shuffle_epi32(pen, _MM_SHUFFLE(0, 1, 2, 3));
					const __m128i valid = _mm_srai_epi32(_mm_slli_epi32(pen, 2), 31);
					const int visible = _mm_movemask_epi8(valid);
					if (c.transparent && !visible) continue;
					__m128i old = _mm_setzero_si128();
					if (c.da || (c.transparent && visible != 0xffff))
						old = _mm_loadu_si128((const __m128i *)(out + x));
					__m128i result = vector.blend<0, DestinationMode>(pen, old);
					if (c.transparent && visible != 0xffff)
						result = _mm_or_si128(_mm_and_si128(valid, result), _mm_andnot_si128(valid, old));
					_mm_storeu_si128((__m128i *)(out + x), result);
				}
			}
#endif
			for (; x < c.w; ++x) {
				const UINT32 pen = in[c.flipx ? c.w - 1 - x : x];
				if (c.transparent && !(pen & 0x20000000)) continue;
				if (c.raw) { out[x] = pen; continue; }
				if (DestinationMode == 1) {
					const UINT32 old = out[x];
					const unsigned r = (pen >> 19) & 31, g = (pen >> 11) & 31, b = (pen >> 3) & 31;
					// Destination factor uses the tinted source before source-alpha
					// multiplication, with both rounding stages kept separate.
					const unsigned tr = epic12_source_scale[c.tint.r][31][r];
					const unsigned tg = epic12_source_scale[c.tint.g][31][g];
					const unsigned tb = epic12_source_scale[c.tint.b][31][b];
					out[x] = (epic12_device_colrtable_add[red[r]][epic12_device_colrtable[tr][(old >> 19) & 31]] << 19)
						| (epic12_device_colrtable_add[green[g]][epic12_device_colrtable[tg][(old >> 11) & 31]] << 11)
						| (epic12_device_colrtable_add[blue[b]][epic12_device_colrtable[tb][(old >> 3) & 31]] << 3)
						| (pen & 0x20000000);
					continue;
				}
				UINT32 s = c.identity ? pen & 0x00f8f8f8
					: (red[(pen >> 19) & 31] << 19) | (green[(pen >> 11) & 31] << 11) | (blue[(pen >> 3) & 31] << 3);
				if (c.da) {
					const UINT32 pen_d = out[x];
					const UINT32 d = c.da == 31 ? pen_d & 0x00f8f8f8
						: (dest[(pen_d >> 19) & 31] << 19) | (dest[(pen_d >> 11) & 31] << 11) | (dest[(pen_d >> 3) & 31] << 3);
					s = epic12_packed_add(s, d);
				}
				out[x] = s | (pen & 0x20000000);
			}
		}
	}

public:
	Epic12CpuBatch() : count(0), pixels(0), vram(NULL), requested(1) {
		for (int i = 0; i < 2; ++i) attempted[i] = available[i] = false;
	}
	void init(UINT32 *bitmap) {
		exit();
		vram = bitmap;
		alpha.clear();
	}
	void exit() {
		flush();
		for (int i = 0; i < 2; ++i) {
			workers[i].exit();
			attempted[i] = available[i] = false;
		}
		requested = 1;
		vram = NULL;
		alpha.clear();
	}
	// Called by the main thread after the previous ordered list was joined.
	// Never consult mutable frontend options from a running raster job.
	void begin(int lanes) {
		requested = lanes >= 1 && lanes <= 3 ? lanes : 1;
		for (int i = requested - 1; i < 2; ++i) {
			workers[i].exit();
			attempted[i] = available[i] = false;
		}
	}
	void invalidate_all() { flush(); alpha.clear(); }
	void before_write(const rectangle &r) { flush(); alpha.invalidate(r); }
	void draw_rows(int first, int last) {
		for (unsigned i = 0; i < count; ++i) {
			if (commands[i].source_dest) draw_command<1>(commands[i], first, last);
			else draw_command<0>(commands[i], first, last);
		}
	}
	void flush() {
		if (!count) return;
		int lanes = requested;
		const int first = destinations.min_y, last = destinations.max_y + 1;
		while (lanes > 1 && (pixels < (UINT64)lanes * MIN_PIXELS_PER_LANE || last - first < lanes)) --lanes;
		int cuts[4]; cuts[0] = first; cuts[lanes] = last;
		if (lanes > 1) {
			memset(row_work + first, 0, (last - first + 1) * sizeof(row_work[0]));
			for (unsigned i = 0; i < count; ++i) {
				const Command &c = commands[i];
				row_work[c.y] += c.w; row_work[c.y + c.h] -= c.w;
			}
			UINT64 done = 0;
			int work = 0, split = 1;
			for (int y = first; y < last && split < lanes; ++y) {
				work += row_work[y]; done += work;
				if ((done >= pixels * split / lanes || y + 1 == last - (lanes - split))
					&& y + 1 > cuts[split - 1] && y + 1 <= last - (lanes - split)) cuts[split++] = y + 1;
			}
			for (int i = 1; i < lanes; ++i) {
				if (!attempted[i - 1]) {
					attempted[i - 1] = true;
					available[i - 1] = workers[i - 1].init(epic12_cpu_job, i);
				}
				if (!available[i - 1] || !workers[i - 1].start(cuts[i], cuts[i + 1]))
					draw_rows(cuts[i], cuts[i + 1]);
			}
		}
		draw_rows(cuts[0], cuts[1]);
		for (int i = 1; i < lanes; ++i) if (available[i - 1]) workers[i - 1].finish();
		alpha.invalidate(destinations);
		count = 0; pixels = 0;
	}

	bool submit(int flipx, int transparent, int blend, int smode, int dmode, BLIT_PARAMS) {
		if (!vram || gfx != vram) return false;
		UINT8 source_alpha = s_alpha, destination_alpha = d_alpha;
		// Constant factors share the fixed-alpha kernel. Normalize only local
		// arguments; rejected modes retain the original rasterizer semantics.
		if (blend) {
			if (smode == 3 || smode == 7) source_alpha = 31;
			else if (smode == 4) source_alpha = 31 - s_alpha;
			else if (smode != 0) return false;
			if (dmode == 3 || dmode == 7) destination_alpha = 31;
			else if (dmode == 4) destination_alpha = 31 - d_alpha;
			else if (dmode == 1) destination_alpha = 31; // force destination load
			else if (dmode != 0) return false;
		}
		// Preserve the original horizontal-wrap rejection before clipping.
		if (src_x + dimx > 8192) return true;
		const int x0 = dst_x_start < clip->min_x ? clip->min_x - dst_x_start : 0;
		const int y0 = dst_y_start < clip->min_y ? clip->min_y - dst_y_start : 0;
		const int x1 = dst_x_start + dimx - 1 > clip->max_x ? clip->max_x - dst_x_start + 1 : dimx;
		const int y1 = dst_y_start + dimy - 1 > clip->max_y ? clip->max_y - dst_y_start + 1 : dimy;
		if (x1 <= x0 || y1 <= y0) return true;
		Command c;
		c.sx = src_x + (flipx ? dimx - x1 : x0);
		c.sy = (src_y + (flipy ? dimy - y1 : y0)) & 4095;
		c.x = dst_x_start + x0; c.y = dst_y_start + y0; c.w = x1 - x0; c.h = y1 - y0;
		if (c.x < 0 || c.y < 0 || c.x + c.w > 8192 || c.y + c.h > 4096 || c.sy + c.h > 4096) return false;
		c.flipx = !!flipx; c.flipy = !!flipy; c.transparent = !!transparent;
		c.source_dest = blend && dmode == 1;
		c.raw = !blend && tint_clr->r == 32 && tint_clr->g == 32 && tint_clr->b == 32;
		c.sa = blend ? source_alpha : 31; c.da = blend ? destination_alpha : 0; c.tint = *tint_clr;
		c.identity = c.sa == 31 && (c.tint.r == 31 || c.tint.r == 32)
			&& (c.tint.g == 31 || c.tint.g == 32) && (c.tint.b == 31 || c.tint.b == 32);
		rectangle src = source_rect(c), dst = destination_rect(c);
		if (intersects(src, dst)) return false; // preserve per-pixel overlap order
		if (count == CAPACITY || (count && depends_on_batch(src, dst))) flush();
		// Both RAW and WAR barriers are required: unlike a GPU atlas, these
		// workers read live VRAM. WAW pairs retain their order in each row.
		epic12_device_blit_delay += (UINT64)c.w * c.h;
		if (c.transparent) {
			rectangle visible;
			if (!alpha.trim(vram, src, visible)) return true;
			c.x += c.flipx ? src.max_x - visible.max_x : visible.min_x - src.min_x;
			c.y += c.flipy ? src.max_y - visible.max_y : visible.min_y - src.min_y;
			c.sx = visible.min_x; c.sy = visible.min_y;
			c.w = visible.max_x - visible.min_x + 1; c.h = visible.max_y - visible.min_y + 1;
			src = visible; dst = destination_rect(c);
		}
		if (!count) { sources = src; destinations = dst; }
		else { extend(sources, src); extend(destinations, dst); }
		commands[count++] = c;
		pixels += (UINT64)c.w * c.h;
		return true;
	}
};

static Epic12CpuBatch epic12_cpu_batch;

static void epic12_cpu_job(INT32 first, INT32 last, INT32)
{
	epic12_cpu_batch.draw_rows(first, last);
}
#endif
