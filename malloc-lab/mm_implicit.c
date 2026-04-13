#include "mm_implicit.h"

#if FIT_STRATEGY == NEXT_FIT
static void *last_fitp;
#endif

static void init(void)
{
#if FIT_STRATEGY == NEXT_FIT
    last_fitp = NULL;
#endif
}

static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) {
        return bp;
    } else if (prev_alloc && !next_alloc) {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    } else if (!prev_alloc && next_alloc) {
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    } else {
        size += GET_SIZE(HDRP(PREV_BLKP(bp)))
              + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

#if FIT_STRATEGY == NEXT_FIT
    if (last_fitp != NULL &&
        (char *)bp < (char *)last_fitp &&
        (char *)last_fitp < (char *)NEXT_BLKP(bp)) {
        last_fitp = bp;
    }
#endif

    return bp;
}

#if FIT_STRATEGY == FIRST_FIT

static void *find_fit(size_t asize)
{
    void *bp = heap_listp;

    while (GET_SIZE(HDRP(bp)) > 0) {
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp))))
            return bp;
        bp = NEXT_BLKP(bp);
    }
    return NULL;
}

#elif FIT_STRATEGY == BEST_FIT

static void *find_fit(size_t asize)
{
    void *bp = heap_listp;
    void *best = NULL;
    size_t best_size = ~(size_t)0;

    while (GET_SIZE(HDRP(bp)) > 0) {
        size_t bsize = GET_SIZE(HDRP(bp));
        if (!GET_ALLOC(HDRP(bp)) && (asize <= bsize)) {
            if (bsize < best_size) {
                best = bp;
                best_size = bsize;
            }
            if (bsize == asize)
                return best;
        }
        bp = NEXT_BLKP(bp);
    }
    return best;
}

#elif FIT_STRATEGY == NEXT_FIT

static void *find_fit(size_t asize)
{
    void *bp;

    if (last_fitp == NULL)
        last_fitp = heap_listp;

    bp = last_fitp;
    while (GET_SIZE(HDRP(bp)) > 0) {
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) {
            last_fitp = bp;
            return bp;
        }
        bp = NEXT_BLKP(bp);
    }

    bp = heap_listp;
    while (bp != last_fitp && GET_SIZE(HDRP(bp)) > 0) {
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) {
            last_fitp = bp;
            return bp;
        }
        bp = NEXT_BLKP(bp);
    }

    return NULL;
}

#endif

static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));

    if ((csize - asize) >= (2 * DSIZE)) {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));
    } else {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}
