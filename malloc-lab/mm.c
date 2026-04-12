/*
 * mm-naive.c - 가장 빠르지만 메모리 효율이 가장 낮은 malloc 패키지.
 *
 * 이 단순한 방식에서는 brk 포인터를 단순히 증가시켜 블록을 할당한다.
 * 블록은 순수한 페이로드만 가지며 헤더나 푸터가 없다.
 * 블록은 병합되거나 재사용되지 않는다. realloc은
 * mm_malloc과 mm_free를 직접 사용해 구현한다.
 *
 * 학생에게: 이 헤더 주석을 여러분의 해법을 높은 수준에서 설명하는
 * 자체 헤더 주석으로 교체하라.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * 학생에게: 다른 작업을 하기 전에 아래 구조체에
 * 팀 정보를 먼저 입력하라.
 ********************************************************/
team_t team = {
    /* 팀 이름 */
    "ateam",
    /* 첫 번째 팀원의 전체 이름 */
    "Harry Bovik",
    /* 첫 번째 팀원의 이메일 주소 */
    "bovik@cs.cmu.edu",
    /* 두 번째 팀원의 전체 이름(없으면 빈칸) */
    "",
    /* 두 번째 팀원의 이메일 주소(없으면 빈칸) */
    ""};

/* 워드(4) 또는 더블 워드(8) 정렬 */
#define ALIGNMENT 8

/* ALIGNMENT의 가장 가까운 배수로 올림 */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/*
 * mm_init - malloc 패키지를 초기화한다.
 */
int mm_init(void)
{
    return 0;
}

/*
 * mm_malloc - brk 포인터를 증가시켜 블록을 할당한다.
 *     항상 정렬 크기의 배수인 블록을 할당한다.
 */
void *mm_malloc(size_t size)
{
    int newsize = ALIGN(size + SIZE_T_SIZE);
    void *p = mem_sbrk(newsize);
    if (p == (void *)-1)
        return NULL;
    else
    {
        *(size_t *)p = size;
        return (void *)((char *)p + SIZE_T_SIZE);
    }
}

/*
 * mm_free - 블록을 해제해도 아무 일도 하지 않는다.
 */
void mm_free(void *ptr)
{
}

/*
 * mm_realloc - mm_malloc과 mm_free를 조합해 단순하게 구현한다.
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;

    newptr = mm_malloc(size);
    if (newptr == NULL)
        return NULL;
    copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    if (size < copySize)
        copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}
