#ifndef MM_EXPLICIT_H
#define MM_EXPLICIT_H

#define PRED(bp) (*(void **)(bp))
#define SUCC(bp) (*(void **)((char *)(bp) + WSIZE))

static void *free_listp;

#endif
