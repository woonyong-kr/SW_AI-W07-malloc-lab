/* 
 * clock.c - x86, Alpha, Sparc 시스템의 사이클 카운터를 다루는 루틴
 * 
 * Copyright (c) 2002, R. Bryant와 D. O'Hallaron, 모든 권리 보유.
 * 허가 없이 사용, 수정, 복사할 수 없다.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/times.h>
#include "clock.h"


/******************************************************* 
 * 머신 의존 함수
 *
 * 참고: __i386__와 __alpha 상수는
 * GCC가 C 전처리기를 호출할 때 설정된다.
 * gcc -v로 직접 확인할 수 있다.
 *******************************************************/

#if defined(__i386__)  
/*******************************************************
 * Pentium용 start_counter()와 get_counter()
 *******************************************************/


/* $begin x86cyclecounter */
/* 사이클 카운터를 초기화한다 */
static unsigned cyc_hi = 0;
static unsigned cyc_lo = 0;


/* *hi와 *lo에 사이클 카운터의 상위/하위 비트를 저장한다.
   구현에는 rdtsc 명령을 사용하는 어셈블리 코드가 필요하다. */
void access_counter(unsigned *hi, unsigned *lo)
{
    asm("rdtsc; movl %%edx,%0; movl %%eax,%1"   /* 사이클 카운터 읽기 */
	: "=r" (*hi), "=r" (*lo)                /* 결과를 */
	: /* 입력 없음 */                       /* 두 출력값에 저장 */
	: "%edx", "%eax");
}

/* 현재 사이클 카운터 값을 기록한다. */
void start_counter()
{
    access_counter(&cyc_hi, &cyc_lo);
}

/* 마지막 start_counter 호출 이후의 사이클 수를 반환한다. */
double get_counter()
{
    unsigned ncyc_hi, ncyc_lo;
    unsigned hi, lo, borrow;
    double result;

    /* 사이클 카운터 값을 가져온다 */
    access_counter(&ncyc_hi, &ncyc_lo);

    /* 배정밀도 뺄셈을 수행한다 */
    lo = ncyc_lo - cyc_lo;
    borrow = lo > ncyc_lo;
    hi = ncyc_hi - cyc_hi - borrow;
    result = (double) hi * (1 << 30) * 4 + lo;
    if (result < 0) {
	fprintf(stderr, "Error: counter returns neg value: %.0f\n", result);
    }
    return result;
}
/* $end x86cyclecounter */

#elif defined(__alpha)

/****************************************************
 * Alpha용 start_counter()와 get_counter()
 ***************************************************/

/* 사이클 카운터를 초기화한다 */
static unsigned cyc_hi = 0;
static unsigned cyc_lo = 0;


/* Alpha 사이클 타이머로 사이클 수를 계산한 뒤,
   측정된 클록 속도로 초 단위 값을 계산한다.
*/

/*
 * counterRoutine은 Alpha 프로세서의 사이클 카운터에 접근하기 위한
 * Alpha 명령어 배열이다. 카운터에 접근하기 위해 rpcc 명령을 사용한다.
 * 이 64비트 레지스터는 두 부분으로 나뉜다. 하위 32비트는 현재 프로세스가
 * 사용한 사이클 수이고, 상위 32비트는 벽시계 기준 사이클 수이다.
 * 이 명령어들은 카운터를 읽은 뒤 하위 32비트를 unsigned int로 변환하는데,
 * 이것이 사용자 공간 카운터 값이다.
 * 참고: 이 카운터가 측정할 수 있는 시간 범위는 매우 짧다.
 * 450MHz 클록에서는 약 9초 정도만 측정할 수 있다. */
static unsigned int counterRoutine[] =
{
    0x601fc000u,
    0x401f0000u,
    0x6bfa8001u
};

/* 위 명령어 배열을 함수처럼 사용한다. */
static unsigned int (*counter)(void)= (void *)counterRoutine;


void start_counter()
{
    /* 사이클 카운터 값을 가져온다 */
    cyc_hi = 0;
    cyc_lo = counter();
}

double get_counter()
{
    unsigned ncyc_hi, ncyc_lo;
    unsigned hi, lo, borrow;
    double result;
    ncyc_lo = counter();
    ncyc_hi = 0;
    lo = ncyc_lo - cyc_lo;
    borrow = lo > ncyc_lo;
    hi = ncyc_hi - cyc_hi - borrow;
    result = (double) hi * (1 << 30) * 4 + lo;
    if (result < 0) {
	fprintf(stderr, "Error: Cycle counter returning negative value: %.0f\n", result);
    }
    return result;
}

#else

/****************************************************************
 * 아직 사이클 카운터 루틴을 구현하지 않은 다른 플랫폼들이다.
 * 최신 sparc(v8plus) 모델은 사용자 프로그램에서 접근 가능한
 * 사이클 카운터를 제공하지만, 이를 지원하지 않는 sparc 장비도 아직 많아
 * 여기서는 Sparc 버전을 제공하지 않는다.
 ***************************************************************/

void start_counter()
{
    printf("ERROR: You are trying to use a start_counter routine in clock.c\n");
    printf("that has not been implemented yet on this platform.\n");
    printf("Please choose another timing package in config.h.\n");
    exit(1);
}

double get_counter() 
{
    printf("ERROR: You are trying to use a get_counter routine in clock.c\n");
    printf("that has not been implemented yet on this platform.\n");
    printf("Please choose another timing package in config.h.\n");
    exit(1);
}
#endif




/*******************************
 * 머신 독립 함수
 ******************************/
double ovhd()
{
    /* 캐시 효과를 없애기 위해 두 번 수행한다 */
    int i;
    double result;

    for (i = 0; i < 2; i++) {
	start_counter();
	result = get_counter();
    }
    return result;
}

/* $begin mhz */
/* sleeptime초 동안 잠드는 사이에 흐른 사이클 수를 측정해 */
/* 클록 속도를 추정한다 */
double mhz_full(int verbose, int sleeptime)
{
    double rate;

    start_counter();
    sleep(sleeptime);
    rate = get_counter() / (1e6*sleeptime);
    if (verbose) 
	printf("Processor clock rate ~= %.1f MHz\n", rate);
    return rate;
}
/* $end mhz */

/* 기본 sleeptime을 사용하는 버전 */
double mhz(int verbose)
{
    return mhz_full(verbose, 2);
}

/** 타이머 인터럽트 오버헤드를 보정하는 특수 카운터 */

static double cyc_per_tick = 0.0;

#define NEVENT 100
#define THRESHOLD 1000
#define RECORDTHRESH 3000

/* 타이머 인터럽트가 얼마나 시간을 사용하는지 추정한다 */
static void callibrate(int verbose)
{
    double oldt;
    struct tms t;
    clock_t oldc;
    int e = 0;

    times(&t);
    oldc = t.tms_utime;
    start_counter();
    oldt = get_counter();
    while (e <NEVENT) {
	double newt = get_counter();

	if (newt-oldt >= THRESHOLD) {
	    clock_t newc;
	    times(&t);
	    newc = t.tms_utime;
	    if (newc > oldc) {
		double cpt = (newt-oldt)/(newc-oldc);
		if ((cyc_per_tick == 0.0 || cyc_per_tick > cpt) && cpt > RECORDTHRESH)
		    cyc_per_tick = cpt;
		/*
		  if (verbose)
		  printf("%.0f 사이클과 %d 틱 동안의 이벤트를 관찰함. 비율 = %f\n",
		  newt-oldt, (int) (newc-oldc), cpt);
		*/
		e++;
		oldc = newc;
	    }
	    oldt = newt;
	}
    }
    if (verbose)
	printf("Setting cyc_per_tick to %f\n", cyc_per_tick);
}

static clock_t start_tick = 0;

void start_comp_counter() 
{
    struct tms t;

    if (cyc_per_tick == 0.0)
	callibrate(0);
    times(&t);
    start_tick = t.tms_utime;
    start_counter();
}

double get_comp_counter() 
{
    double time = get_counter();
    double ctime;
    struct tms t;
    clock_t ticks;

    times(&t);
    ticks = t.tms_utime - start_tick;
    ctime = time - ticks*cyc_per_tick;
    /*
      printf("측정한 사이클 = %.0f, 틱 = %d, 보정 후 사이클 = %.0f\n",
      time, (int) ticks, ctime);
    */
    return ctime;
}
