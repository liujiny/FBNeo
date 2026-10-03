#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sched.h>
typedef int INT32;
#define FBNEO_RENDER_THREADS_TEST
#include "../../src/burn/devices/render_worker.h"
INT32 nBurnRenderCores = 1;
static std::atomic<unsigned> visits[513];
static int rows, expected;
static void draw(INT32 first, INT32 last, INT32 index)
{
    if (first < 0 || last > rows || first > last || index < 0 || index > 2) abort();
    for (int y = first; y < last; ++y) {
        if (visits[y].fetch_add(1) != (unsigned)expected) abort();
        if (!(y & 31)) sched_yield();
    }
}
int main()
{
    BurnRenderPool pool;
    pool.init(draw);
    for (int height = 0; height <= 513; ++height) {
        rows = height;
        for (int y = 0; y < 513; ++y) visits[y] = 0;
        for (int pass = 0; pass < 8; ++pass) {
            expected = pass;
            nBurnRenderCores = 1 + pass % 3;
            pool.render(height, pass != 5);
            for (int y = 0; y < height; ++y)
                if (visits[y] != (unsigned)(pass + 1)) abort();
            if (pass == 3) pool.exit();
        }
    }
    pool.exit();
    pool.exit();
    puts("PASS: 4112 render jobs, all heights 0..513, 1/2/3 threads, disable, reinit and shutdown");
}
