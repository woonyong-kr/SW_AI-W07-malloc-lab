#include "mm_explicit.h"

static void init(void)
{
    free_listp = NULL;
}

static void *coalesce(void *bp)
{
    return bp;
}

static void *find_fit(size_t asize)
{
    (void)asize;
    return NULL;
}

static void place(void *bp, size_t asize)
{
    (void)bp;
    (void)asize;
}
