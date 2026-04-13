/*
 * memlib.c - 메모리 시스템을 모사하는 모듈이다.
 *            학생의 malloc 패키지 호출과 libc의 시스템 malloc 패키지 호출을
 *            번갈아 실행할 수 있게 해 주기 때문에 필요하다.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <errno.h>

#include "memlib.h"
#include "config.h"

/* 내부 변수 */
static char *mem_start_brk;  /* 힙의 첫 바이트를 가리킨다 */
static char *mem_brk;        /* 힙의 마지막 바이트를 가리킨다 */
static char *mem_max_addr;   /* 합법적인 최대 힙 주소 */

/* 
 * mem_init - 메모리 시스템 모델을 초기화한다
 */
void mem_init(void)
{
    /* 사용 가능한 가상 메모리를 모델링할 저장 공간을 할당한다 */
    if ((mem_start_brk = (char *)malloc(MAX_HEAP)) == NULL) {
	fprintf(stderr, "mem_init_vm: malloc 오류\n");
	exit(1);
    }

    mem_max_addr = mem_start_brk + MAX_HEAP;  /* 최대 유효 힙 주소 */
    mem_brk = mem_start_brk;                  /* 처음에는 힙이 비어 있다 */
}

/* 
 * mem_deinit - 메모리 시스템 모델이 사용한 저장 공간을 해제한다
 */
void mem_deinit(void)
{
    free(mem_start_brk);
}

/*
 * mem_reset_brk - 시뮬레이션된 brk 포인터를 초기화해 빈 힙으로 만든다
 */
void mem_reset_brk()
{
    mem_brk = mem_start_brk;
}

/* 
 * mem_sbrk - sbrk 함수를 단순화해 모델링한 것이다. 힙을 incr 바이트만큼
 *    확장하고 새 영역의 시작 주소를 반환한다. 이 모델에서는 힙을 줄일 수 없다.
 */
void *mem_sbrk(int incr) 
{
    char *old_brk = mem_brk;

    if ( (incr < 0) || ((mem_brk + incr) > mem_max_addr)) {
	errno = ENOMEM;
	fprintf(stderr, "오류: mem_sbrk에 실패했습니다. 메모리가 부족합니다...\n");
	return (void *)-1;
    }
    mem_brk += incr;
    return (void *)old_brk;
}

/*
 * mem_heap_lo - 힙의 첫 바이트 주소를 반환한다
 */
void *mem_heap_lo()
{
    return (void *)mem_start_brk;
}

/* 
 * mem_heap_hi - 힙의 마지막 바이트 주소를 반환한다
 */
void *mem_heap_hi()
{
    return (void *)(mem_brk - 1);
}

/*
 * mem_heapsize() - 힙 크기를 바이트 단위로 반환한다
 */
size_t mem_heapsize() 
{
    return (size_t)(mem_brk - mem_start_brk);
}

/*
 * mem_pagesize() - 시스템의 페이지 크기를 반환한다
 */
size_t mem_pagesize()
{
    return (size_t)getpagesize();
}
