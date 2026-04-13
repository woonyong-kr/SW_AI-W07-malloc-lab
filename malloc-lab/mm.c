#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

#define IMPLICIT 0
#define EXPLICIT 1
#define SEGREGATED 2

#define FIRST_FIT 0
#define BEST_FIT 1
#define NEXT_FIT 2

#define STRATEGY IMPLICIT
#define FIT_STRATEGY NEXT_FIT

#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1 << 12)

#define MAX(x, y) ((x) > (y) ? (x) : (y))

#define PACK(size, alloc) ((size) | (alloc))

#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

#define ALIGN(size)       (((size) + (DSIZE - 1)) & ~0x7)
#define CHUNK_ALIGN(size) (((size) + (CHUNKSIZE - 1)) & ~(CHUNKSIZE - 1))

static char *heap_listp;

static void init(void);
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);

team_t team = {
    "ateam",
    "Harry Bovik",
    "bovik@cs.cmu.edu",
    "",
    ""};

#if STRATEGY == IMPLICIT
#include "mm_implicit.c"
#elif STRATEGY == EXPLICIT
#include "mm_explicit.c"
#elif STRATEGY == SEGREGATED
#include "mm_segregated.c"
#endif

static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

    return coalesce(bp);
}

int mm_init(void)
{
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1)
        return -1;

    PUT(heap_listp, 0);
    PUT(heap_listp + WSIZE, PACK(DSIZE, 1));
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));
    heap_listp += 2 * WSIZE;

    init();

    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
        return -1;
    return 0;
}

void *mm_malloc(size_t size)
{
    size_t asize;
    size_t extendsize;
    void *bp;

    if (size == 0)
        return NULL;

    if (size <= DSIZE)
        asize = 2 * DSIZE;
    else
        asize = ALIGN(size + DSIZE);

    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    extendsize = CHUNK_ALIGN(asize);
    bp = extend_heap(extendsize / WSIZE);
    if (bp == NULL)
        return NULL;
    place(bp, asize);
    return bp;
}

void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));

    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));
    coalesce(ptr);
}

void *mm_realloc(void *ptr, size_t size)
{
    size_t oldsize;
    size_t asize;
    size_t next_alloc;
    size_t next_size;
    size_t combined;
    void *newptr;

    if (ptr == NULL)
        return mm_malloc(size);
    if (size == 0) {
        mm_free(ptr);
        return NULL;
    }

    oldsize = GET_SIZE(HDRP(ptr));

    if (size <= DSIZE)
        asize = 2 * DSIZE;
    else
        asize = ALIGN(size + DSIZE);

    if (oldsize == asize)
        return ptr;

    if (asize <= oldsize) {
        if ((oldsize - asize) >= (2 * DSIZE)) {
            PUT(HDRP(ptr), PACK(asize, 1));
            PUT(FTRP(ptr), PACK(asize, 1));
            newptr = NEXT_BLKP(ptr);
            PUT(HDRP(newptr), PACK(oldsize - asize, 0));
            PUT(FTRP(newptr), PACK(oldsize - asize, 0));
            coalesce(newptr);
        }
        return ptr;
    }

    next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(ptr)));
    next_size = GET_SIZE(HDRP(NEXT_BLKP(ptr)));

    if (!next_alloc && (oldsize + next_size) >= asize) {
        combined = oldsize + next_size;
        PUT(HDRP(ptr), PACK(combined, 1));
        PUT(FTRP(ptr), PACK(combined, 1));
        if ((combined - asize) >= (2 * DSIZE)) {
            PUT(HDRP(ptr), PACK(asize, 1));
            PUT(FTRP(ptr), PACK(asize, 1));
            newptr = NEXT_BLKP(ptr);
            PUT(HDRP(newptr), PACK(combined - asize, 0));
            PUT(FTRP(newptr), PACK(combined - asize, 0));
        }
        return ptr;
    }

    newptr = mm_malloc(size);
    if (newptr == NULL)
        return NULL;
    memcpy(newptr, ptr, oldsize - DSIZE);
    mm_free(ptr);
    return newptr;
}
