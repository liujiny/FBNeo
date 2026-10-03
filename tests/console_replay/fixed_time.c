/* Host replay fixture only: stabilize PGM calendar and BurnRandom seed.
 * Preload into the test process; never link into a production core. */
#include <time.h>
time_t time(time_t *out)
{
    const time_t fixed = (time_t)1790985600;
    if (out) *out = fixed;
    return fixed;
}
