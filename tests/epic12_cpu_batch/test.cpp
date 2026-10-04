// Differential oracle: the original generated EPIC12 rasterizers, rather
// than a second implementation of the optimized renderer's equations.
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
static bool fail_create;
static unsigned create_calls;
static int test_create(pthread_t *t, const pthread_attr_t *a, void *(*f)(void *), void *p) {
	++create_calls;
	return fail_create ? EAGAIN : pthread_create(t, a, f, p);
}
#define pthread_create test_create
#define FBNEO_RENDER_THREADS_TEST
#include "../../src/burn/devices/epic12.cpp"
#undef pthread_create

INT32 nBurnRenderCores = 2;
static unsigned seed = 0xb17281a5;
static unsigned rnd() { seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5; return seed; }
static unsigned clamp(unsigned value) { return value > 31 ? 31 : value; }
static const size_t VRAM_WORDS = 8192 * 4096;
static UINT32 *reference, *candidate;
static UINT64 reference_delay;

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); exit(1); } } while (0)

struct Draw {
	int sx, sy, x, y, w, h, fx, fy, tr, blend, sm, dm, sa, da;
	clr_t tint;
	rectangle clip;
	Draw() : sx(512), sy(0), x(0), y(0), w(64), h(64), fx(0), fy(0), tr(0), blend(0),
		sm(0), dm(0), sa(31), da(31), clip(0, 8191, 0, 4095) { tint.r = tint.g = tint.b = 32; tint.t = 0; }
};

static void original(const Draw &d) {
	m_bitmaps = reference;
	epic12_device_blit_delay = reference_delay;
	const bool tinted = d.tint.r != 32 || d.tint.g != 32 || d.tint.b != 32;
	const bool blend = d.blend && !(d.sm == 0 && d.sa == 31 && d.dm == 4 && d.da == 31);
	const unsigned index = (d.fx ? 4 : 0) | (tinted ? 2 : 0) | (d.tr ? 1 : 0);
	static epic12_device_blitfunction plain[8] = {
		draw_sprite_f0_ti0_tr0_simple, draw_sprite_f0_ti0_tr1_simple,
		draw_sprite_f0_ti1_tr0_plain, draw_sprite_f0_ti1_tr1_plain,
		draw_sprite_f1_ti0_tr0_simple, draw_sprite_f1_ti0_tr1_simple,
		draw_sprite_f1_ti1_tr0_plain, draw_sprite_f1_ti1_tr1_plain
	};
	static epic12_device_blitfunction *blended[8] = {
		epic12_device_f0_ti0_tr0_blit_funcs, epic12_device_f0_ti0_tr1_blit_funcs,
		epic12_device_f0_ti1_tr0_blit_funcs, epic12_device_f0_ti1_tr1_blit_funcs,
		epic12_device_f1_ti0_tr0_blit_funcs, epic12_device_f1_ti0_tr1_blit_funcs,
		epic12_device_f1_ti1_tr0_blit_funcs, epic12_device_f1_ti1_tr1_blit_funcs
	};
	epic12_device_blitfunction fn = blend ? blended[index][d.sm | (d.dm << 3)] : plain[index];
	fn(&d.clip, reference, d.sx, d.sy, d.x, d.y, d.w, d.h, d.fy, d.sa, d.da, &d.tint);
	reference_delay = epic12_device_blit_delay;
}

static void encode(const Draw &d, UINT16 *out) {
	out[0] = 0x1000 | (d.fx ? 0x800 : 0) | (d.fy ? 0x400 : 0)
		| (d.blend ? 0x200 : 0) | (d.tr ? 0x100 : 0) | (d.sm << 4) | d.dm;
	out[1] = (d.sa << 11) | (d.da << 3); out[2] = d.sx; out[3] = d.sy;
	out[4] = d.x; out[5] = d.y; out[6] = d.w - 1; out[7] = d.h - 1;
	out[8] = d.tint.r << 2; out[9] = (d.tint.g << 10) | (d.tint.b << 2);
}

static UINT16 shadow[4096];
static void draw(const Draw &d) {
	const UINT64 saved = epic12_device_blit_delay;
	original(d);
	epic12_device_blit_delay = saved; m_bitmaps = candidate;
	encode(d, shadow); m_ram16_copy = shadow; m_main_rammask = sizeof(shadow) - 1;
	m_clip = d.clip;
	UINT32 addr = 0; gfx_draw(&addr); CHECK(addr == 20);
}

static void compare() {
	epic12_cpu_batch.flush();
	CHECK(epic12_device_blit_delay == reference_delay);
	if (memcmp(reference, candidate, VRAM_WORDS * sizeof(*reference))) {
		for (size_t i = 0; i < VRAM_WORDS; ++i) if (reference[i] != candidate[i]) {
			fprintf(stderr, "VRAM mismatch %zu,%zu: %08x != %08x\n", i % 8192, i / 8192, reference[i], candidate[i]);
			exit(1);
		}
	}
}

static void configure(int lanes) {
	compare();
	epic12_cpu_batch.begin(lanes);
}

static void writes_and_dependencies() {
	Draw d;
	// RAW (later source reads pending output), WAR (later destination writes
	// an earlier source), WAW (same destination keeps original draw order).
	draw(d);
	d.sx = 0; d.sy = 0; d.x = 192; d.y = 128; draw(d);
	d = Draw(); draw(d);
	d.sx = 768; d.x = 512; draw(d);
	d = Draw(); d.sx = 640; draw(d); d.sx = 704; draw(d);
	// Self overlap includes shifted rows, horizontal directions and flips.
	for (int i = 0; i < 8; ++i) {
		d = Draw(); d.sx = 32; d.sy = 32; d.x = 32 + (i & 1); d.y = 32 + ((i >> 1) & 1);
		d.fx = i & 1; d.fy = (i >> 2) & 1; d.tr = 1; draw(d);
	}
	// Source wrapping, negative destinations and empty clipping.
	d = Draw(); d.sx = 8180; d.w = 32; d.clip = rectangle(16, 23, 0, 63); draw(d);
	d.sx = 512; d.sy = 4090; d.fx = 1; d.fy = 1; d.clip = rectangle(0, 63, 0, 63); draw(d);
	d = Draw(); d.x = -17; d.y = -9; d.clip = rectangle(0, 31, 0, 31); draw(d);
	d.x = 100; draw(d);
	compare();
	// Alpha changes invalidate the entire affected cache page, including a
	// previously cached empty query. RGB without the marker is transparent.
	d = Draw(); d.tr = 1; d.sx = 1024; d.sy = 1024; d.w = d.h = 32; draw(d); compare();
	rectangle changed(1024, 1055, 1024, 1055);
	epic12_cpu_batch.before_write(changed);
	for (int y = 1024; y < 1056; ++y) for (int x = 1024; x < 1056; ++x)
		reference[y * 8192 + x] = candidate[y * 8192 + x] = 0x00f8a870;
	draw(d); compare();
	epic12_cpu_batch.before_write(changed);
	reference[1037 * 8192 + 1041] = candidate[1037 * 8192 + 1041] = 0x20f80000;
	for (int i = 0; i < 4; ++i) { d.fx = i & 1; d.fy = (i >> 1) & 1; draw(d); }
	compare();
}

static void random_lists() {
	for (int list = 0; list < 48; ++list) {
		configure(1 + list % 3);
		for (int i = 0; i < 321; ++i) {
			Draw d;
			d.sx = 512 + rnd() % 384; d.sy = rnd() % 384;
			d.x = rnd() % 256; d.y = rnd() % 256;
			d.w = 1 + rnd() % 128; d.h = 1 + rnd() % 64;
			d.fx = rnd() & 1; d.fy = rnd() & 1; d.tr = rnd() & 1;
			d.blend = rnd() & 1; d.sa = rnd() % 32; d.da = rnd() % 32;
			if (i % 3) d.sa = 31;
			if (i % 4) d.da = (i & 1) ? 31 : 0;
			if (!(i % 5)) { d.tint.r = rnd() % 64; d.tint.g = rnd() % 64; d.tint.b = rnd() % 64; }
			if (!(i % 17)) { d.sm = rnd() % 8; d.dm = rnd() % 8; }
			if (!(i % 19)) d.clip = rectangle(13, 111, 7, 93);
			if (!(i % 23)) { d.sx = d.x + 1; d.sy = d.y; }
			draw(d);
		}
		compare();
	}
}

static void capacity_and_row_balance() {
	for (int lanes = 1; lanes <= 3; ++lanes) {
		configure(lanes);
		// No dependency/fallback barriers: force the capacity flush at 256.
		for (int i = 0; i < 321; ++i) {
			Draw d; d.sx += (i & 3) * 64; d.x = (i & 3) * 64;
			d.h = 128; d.y = (i & 1) * 128; d.fx = i & 1;
			draw(d);
		}
		compare();
		// Sparse rows and concentrated overdraw force the split deadline, not
		// just the common uniform-work case. Destination never aliases source.
		for (int i = 0; i < 241; ++i) {
			Draw d; d.h = 1; d.w = 256;
			d.y = i < 2 ? i : 4095; d.fx = i & 1; draw(d);
		}
		compare();
	}
}

static UINT32 expanded(UINT16 v) {
	return ((UINT32)(v >> 15) << 29) | (((v >> 10) & 31) << 19)
		| (((v >> 5) & 31) << 11) | ((v & 31) << 3);
}

static void uploads() {
	UINT16 all[65536];
	UINT32 converted[65538];
	for (unsigned i = 0; i < 65536; ++i) all[i] = i;
	converted[0] = converted[65537] = 0xdeadbeef;
	epic12_expand_words(converted + 1, all, 65536);
	for (unsigned i = 0; i < 65536; ++i) CHECK(converted[i + 1] == expanded(all[i]));
	CHECK(converted[0] == 0xdeadbeef && converted[65537] == 0xdeadbeef);

	UINT16 input[64], expected_copy[64], got_copy[64];
	UINT32 got_pixels[204], expected_pixels[204];
	for (unsigned i = 0; i < 64; ++i) input[i] = rnd();
	// Odd byte addresses, repeated RAM wrapping and UINT32 address overflow
	// must match the original word readers, including untouched output guards.
	for (unsigned i = 0; i < 200; ++i) {
		const unsigned count = i;
		const UINT32 start = i < 128 ? i : 0xffffff80U + i - 128;
		UINT32 a = start, b = start;
		memset(expected_copy, 0x55, sizeof(expected_copy));
		memset(got_copy, 0x55, sizeof(got_copy));
		m_main_rammask = sizeof(input) - 1; m_ram16 = input; m_ram16_copy = expected_copy;
		for (unsigned j = 0; j < count; ++j) COPY_NEXT_WORD(&a);
		epic12_copy_payload(got_copy, input, &b, m_main_rammask, count);
		CHECK(a == b && !memcmp(expected_copy, got_copy, sizeof(got_copy)));
		memset(expected_pixels, 0x66, sizeof(expected_pixels));
		memset(got_pixels, 0x66, sizeof(got_pixels));
		a = b = start; m_ram16_copy = input;
		for (unsigned j = 0; j < count; ++j) expected_pixels[j + 1] = expanded(READ_NEXT_WORD(&a));
		epic12_upload_row(got_pixels + 1, input, &b, m_main_rammask, count);
		CHECK(a == b && !memcmp(expected_pixels, got_pixels, sizeof(got_pixels)));
	}
	// Real shadow-copy command parser, with its header crossing the RAM end.
	UINT16 source[128], snapshot[128], expected[128];
	for (unsigned i = 0; i < 128; ++i) source[i] = rnd();
	const UINT32 start = 249;
	source[((start + 12) & 255) >> 1] = 18;
	source[((start + 14) & 255) >> 1] = 2;
	memset(snapshot, 0x33, sizeof(snapshot)); memcpy(expected, snapshot, sizeof(snapshot));
	m_ram16 = source; m_ram16_copy = expected; m_main_rammask = sizeof(source) - 1;
	UINT32 a = start, b = start;
	for (unsigned i = 0; i < 8 + 19 * 3; ++i) COPY_NEXT_WORD(&a);
	m_ram16_copy = snapshot; m_blit_delay_ns = 123; m_blit_idle_op_bytes = 17;
	gfx_upload_shadow_copy(&b);
	CHECK(a == b && !memcmp(expected, snapshot, sizeof(snapshot)));
	CHECK(m_blit_delay_ns == 123 + ((16 + 19 * 3 * 2) / 4) * EP1C_SRAM_CLK_NANOSEC);
	CHECK(m_blit_idle_op_bytes == 0);
	m_ram16 = NULL; m_ram16_copy = shadow; m_main_rammask = sizeof(shadow) - 1;
}

static void small_simd_edges() {
	for (int w = 1; w <= 19; ++w) for (int mode = 0; mode < 16; ++mode) {
		Draw d;
		d.w = w; d.h = 3; d.sx += w & 3; d.x += w & 3;
		d.fx = mode & 1; d.fy = (mode >> 1) & 1; d.tr = (mode >> 2) & 1;
		d.blend = (mode >> 3) & 1; d.da = (w & 1) ? 31 : 0;
		draw(d);
	}
	compare();
}

static void command_lists() {
	// Exercise parser flushes, clip changes and uploads between cached draws.
	const UINT16 endings[3] = { 0x0000, 0xf000, 0x3000 };
	for (int t = 0; t < 3; ++t) {
		configure(t + 1);
		UINT16 list[256]; unsigned n = 0;
		list[n++] = 0xc000; list[n++] = 0;
		Draw d; d.sx = 1024; d.sy = 1024; d.tr = 1; d.w = 19; d.h = 7;
		const UINT64 saved = epic12_device_blit_delay;
		encode(d, list + n); n += 10; original(d);
		memset(list + n, 0, 8 * sizeof(list[0]));
		list[n] = 0x2000; list[n + 4] = d.sx; list[n + 5] = d.sy;
		list[n + 6] = d.w - 1; list[n + 7] = d.h - 1; n += 8;
		for (int y = 0; y < d.h; ++y) for (int x = 0; x < d.w; ++x) {
			list[n] = rnd(); reference[(d.sy + y) * 8192 + d.sx + x] = expanded(list[n++]);
		}
		d.x = 64; d.y = 64; encode(d, list + n); n += 10; original(d);
		list[n++] = 0xc000; list[n++] = 1;
		m_gfx_clip_x_shadowcopy = 40; m_gfx_clip_y_shadowcopy = 56;
		d = Draw(); d.w = 128; d.h = 64;
		d.clip = rectangle(8, 391, 24, 327);
		encode(d, list + n); n += 10; original(d);
		list[n++] = endings[t];
		CHECK(n < sizeof(shadow) / sizeof(shadow[0]));
		memcpy(shadow, list, n * sizeof(list[0]));
		m_ram16_copy = shadow; m_main_rammask = sizeof(shadow) - 1;
		m_gfx_addr_shadowcopy = 0; m_bitmaps = candidate;
		epic12_device_blit_delay = saved;
		gfx_exec();
		// Check before compare() could hide a missing end-of-list flush.
		CHECK(!memcmp(reference, candidate, VRAM_WORDS * sizeof(*reference)));
		compare();
	}
}

static void worker_lifetimes() {
	Draw large; large.w = 320; large.h = 240;
	for (int lanes = 1; lanes <= 3; ++lanes) {
		epic12_cpu_batch.exit(); epic12_cpu_batch.init(candidate);
		const unsigned before = create_calls;
		configure(lanes); draw(large); compare();
		CHECK(create_calls == before + lanes - 1);
		// Reuse workers across batches, then lower the option and release them.
		draw(large); compare(); CHECK(create_calls == before + lanes - 1);
		configure(1); draw(large); compare();
	}
	epic12_cpu_batch.exit(); epic12_cpu_batch.init(candidate);
	fail_create = true;
	const unsigned before = create_calls;
	configure(3); draw(large); compare();
	CHECK(create_calls == before + 2);
	draw(large); compare(); CHECK(create_calls == before + 2);
	fail_create = false;
	epic12_cpu_batch.exit(); epic12_cpu_batch.init(candidate);
	configure(3); draw(large); compare(); CHECK(create_calls == before + 4);

	// Large commands owned by the existing asynchronous list worker also use
	// the helpers. Tests above deliberately keep the owner on the main thread.
	reference_delay = 0; // The real ordered callback resets the list counter.
	original(large); m_bitmaps = candidate; epic12_device_blit_delay = 0;
	encode(large, shadow); shadow[10] = 0xf000;
	m_ram16_copy = shadow; m_main_rammask = sizeof(shadow) - 1;
	m_gfx_addr_shadowcopy = 0;
	m_gfx_clip_x_shadowcopy = m_gfx_clip_y_shadowcopy = 32;
	thready.init(run_blitter_cb); CHECK(thready.available);
	nBurnRenderCores = 3; thready.set_threading(1);
	thready.notify(); thready.notify_wait(); compare();
	thready.exit();
	// Cache metadata must not survive restoring/replacing emulated VRAM.
	Draw d; d.sx = d.sy = 1024; d.w = d.h = 32; d.tr = 1;
	draw(d); compare();
	for (int y = 1024; y < 1056; ++y) for (int x = 1024; x < 1056; ++x)
		reference[y * 8192 + x] = candidate[y * 8192 + x] = 0;
	epic12_cpu_batch.invalidate_all(); draw(d); compare();
	// Use the actual reset entry point; it must also discard cached bounds.
	reference[1025 * 8192 + 1025] = candidate[1025 * 8192 + 1025] = 0x20f8f8f8;
	epic12_reset(); reference_delay = 0;
	configure(1); draw(d); compare();
	CHECK(thready.startup_frame == 180);
}

int main() {
	setbuf(stdout, NULL);
	for (int a = 0; a < 32; ++a) for (int b = 0; b < 64; ++b) {
		epic12_device_colrtable[a][b] = clamp(a * b / 31);
		epic12_device_colrtable_rev[a ^ 31][b] = clamp(a * b / 31);
	}
	for (int a = 0; a < 32; ++a) for (int b = 0; b < 32; ++b)
		epic12_device_colrtable_add[a][b] = clamp(a + b);
	epic12_init_blend_tables();
	reference = (UINT32 *)calloc(VRAM_WORDS, sizeof(*reference));
	candidate = (UINT32 *)calloc(VRAM_WORDS, sizeof(*candidate));
	CHECK(reference && candidate);
	for (int y = 0; y < 512; ++y) for (int x = 0; x < 1152; ++x)
		reference[y * 8192 + x] = rnd() & 0x20f8f8f8;
	// Raw copies must preserve even unused pixel bits; tinted/blended paths
	// must discard them exactly like the legacy colour conversion.
	for (int y = 0; y < 4; ++y) for (int x = 512; x < 544; ++x)
		reference[y * 8192 + x] = rnd();
	for (int y = 4080; y < 4096; ++y) for (int x = 500; x < 1100; ++x)
		reference[y * 8192 + x] = rnd() & 0x20f8f8f8;
	memcpy(candidate, reference, VRAM_WORDS * sizeof(*reference));
	m_bitmaps = candidate; epic12_cpu_batch.init(candidate);
	uploads(); puts("PASS upload expansion, wrapping, payload copy and command timing");
	small_simd_edges(); writes_and_dependencies(); random_lists();
	capacity_and_row_balance(); command_lists();
	puts("PASS legacy-rasterizer differential: pixels, delay, dependencies and parser barriers");
	worker_lifetimes(); epic12_cpu_batch.exit();
	puts("PASS helper creation/failure, ordered owner, reinit, option changes and cache reset");
	free(reference); free(candidate); m_bitmaps = NULL;
	return 0;
}
