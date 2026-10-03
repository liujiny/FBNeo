/* OpenOrbis mmap realloc fallback copies the old size after mremap fails,
 * including when shrinking. PS4 has no mremap implementation. Keep growth on
 * the SDK allocator, but allocate/copy only the requested bytes for shrink. */
#include <stdlib.h>
#include <malloc.h>
#include <string.h>

void *__real_realloc(void *ptr, size_t size);

void *__wrap_realloc(void *ptr, size_t size)
{
    void *replacement;
    if (!ptr)
        return __real_realloc(ptr, size);
    if (!size) {
        free(ptr);
        return NULL;
    }
    if (size >= malloc_usable_size(ptr))
        return __real_realloc(ptr, size);
    replacement = malloc(size);
    if (!replacement)
        return NULL; /* The original block remains owned by the caller. */
    memcpy(replacement, ptr, size);
    free(ptr);
    return replacement;
}
