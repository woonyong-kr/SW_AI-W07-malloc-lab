#include "mm_segregated.h"

static void init(void)
{
    int i;

    for (i = 0; i < SEG_COUNT; i++)
        seg_lists[i] = NULL;
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
