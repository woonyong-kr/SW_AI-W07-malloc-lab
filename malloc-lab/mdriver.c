/*
 * mdriver.c - CS:APP Malloc Lab 드라이버
 *
 * 여러 trace 파일 모음을 사용해 mm.c의 malloc/free/realloc
 * 구현을 테스트한다.
 *
 * Copyright (c) 2002, R. Bryant와 D. O'Hallaron, 모든 권리 보유.
 * 허가 없이 사용, 수정, 복사할 수 없다.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <float.h>
#include <time.h>

extern char *optarg; // optarg 선언 추가

#include "mm.h"
#include "memlib.h"
#include "fsecs.h"
#include "config.h"

/**********************
 * 상수와 매크로
 **********************/

/* 기타 */
#define MAXLINE 1024	   /* 최대 문자열 길이 */
#define HDRLINES 4		   /* trace 파일의 헤더 줄 수 */
#define LINENUM(i) (i + 5) /* trace 요청 번호를 줄 번호로 변환(1부터 시작) */

/* p가 ALIGNMENT 바이트 정렬이면 참을 반환한다 */
#define IS_ALIGNED(p) ((((unsigned int)(p)) % ALIGNMENT) == 0)

/******************************
 * 주요 복합 데이터 타입
 *****************************/

/* 각 블록 페이로드의 범위를 기록한다 */
typedef struct range_t
{
	char *lo;			  /* 낮은 페이로드 주소 */
	char *hi;			  /* 높은 페이로드 주소 */
	struct range_t *next; /* 다음 리스트 원소 */
} range_t;

/* 단일 trace 연산(할당기 요청)의 특성을 나타낸다 */
typedef struct
{
	enum
	{
		ALLOC,
		FREE,
		REALLOC
	} type;	   /* 요청 종류 */
	int index; /* 나중에 free()에서 사용할 인덱스 */
	int size;  /* alloc/realloc 요청의 바이트 크기 */
} traceop_t;

/* 하나의 trace 파일 정보를 담는다 */
typedef struct
{
	int sugg_heapsize;	 /* 권장 힙 크기(사용하지 않음) */
	int num_ids;		 /* alloc/realloc ID 개수 */
	int num_ops;		 /* 서로 다른 요청 개수 */
	int weight;			 /* 이 trace의 가중치(사용하지 않음) */
	traceop_t *ops;		 /* 요청 배열 */
	char **blocks;		 /* malloc/realloc이 반환한 포인터 배열 */
	size_t *block_sizes; /* 각 블록에 대응하는 페이로드 크기 배열 */
} trace_t;

/*
 * fcyc로 시간을 측정하는 xxx_speed 함수에 전달할 파라미터를 담는다.
 * fcyc가 입력으로 포인터 하나만 받기 때문에 이 구조체가 필요하다.
 */
typedef struct
{
	trace_t *trace;
	range_t *ranges;
} speed_t;

/* 특정 trace에서 malloc 함수의 주요 통계를 요약한다 */
typedef struct
{
	/* libc malloc과 학생 malloc 패키지(mm.c)에 공통으로 정의된다 */
	double ops;	 /* trace의 연산 수(malloc/free/realloc) */
	int valid;	 /* 할당기가 이 trace를 올바르게 처리했는가? */
	double secs; /* trace 실행에 걸린 시간(초) */

	/* 학생 malloc 패키지에만 정의된다 */
	double util; /* 이 trace의 공간 활용도(libc에서는 항상 0) */

	/* 참고: secs와 util은 valid가 참일 때만 의미가 있다 */
} stats_t;

/********************
 * 전역 변수
 *******************/
int verbose = 0;	   /* 상세 출력 여부를 나타내는 전역 플래그 */
static int errors = 0; /* 학생 malloc 실행 중 발견한 오류 수 */
char msg[MAXLINE];	   /* 오류 메시지를 조합할 때 사용하는 버퍼 */

/* 기본 trace 파일을 찾는 디렉터리 */
static char tracedir[MAXLINE] = TRACEDIR;

/* 기본 trace 파일 이름 목록 */
static char *default_tracefiles[] = {
	DEFAULT_TRACEFILES, NULL};

/*********************
 * 함수 프로토타입
 *********************/

/* range 리스트를 다루는 함수들 */
static int add_range(range_t **ranges, char *lo, int size,
					 int tracenum, int opnum);
static void remove_range(range_t **ranges, char *lo);
static void clear_ranges(range_t **ranges);

/* trace를 읽고, 메모리를 할당하고, 해제하는 함수들 */
static trace_t *read_trace(char *tracedir, char *filename);
static void free_trace(trace_t *trace);

/* libc malloc의 정확성과 속도를 평가하는 루틴 */
static int eval_libc_valid(trace_t *trace, int tracenum);
static void eval_libc_speed(void *ptr);

/* mm.c의 학생 malloc 패키지에 대해 정확성, 공간 활용도, 속도를 평가하는 루틴 */
static int eval_mm_valid(trace_t *trace, int tracenum, range_t **ranges);
static double eval_mm_util(trace_t *trace, int tracenum, range_t **ranges);
static void eval_mm_speed(void *ptr);

/* 각종 보조 루틴 */
static void printresults(int n, stats_t *stats);
static void usage(void);
static void unix_error(char *msg);
static void malloc_error(int tracenum, int opnum, char *msg);
static void app_error(char *msg);

/**************
 * 메인 루틴
 **************/
int main(int argc, char **argv)
{
	int i;
	int c;
	char **tracefiles = NULL;	/* NULL로 끝나는 trace 파일 이름 배열 */
	int num_tracefiles = 0;		/* 해당 배열에 들어 있는 trace 수 */
	trace_t *trace = NULL;		/* 메모리에 적재한 단일 trace 파일 */
	range_t *ranges = NULL;		/* 한 trace의 블록 범위를 추적한다 */
	stats_t *libc_stats = NULL; /* trace별 libc 통계 */
	stats_t *mm_stats = NULL;	/* trace별 mm(학생) 통계 */
	speed_t speed_params;		/* xx_speed 루틴에 전달할 입력 파라미터 */

	int team_check = 1; /* 설정되면 팀 구조를 검사한다(-a로 해제) */
	int run_libc = 0;	/* 설정되면 libc malloc도 실행한다(-l) */
	int autograder = 0; /* 설정되면 자동 채점용 요약 정보를 출력한다(-g) */

	/* 성능 지수를 계산할 때 사용하는 임시 변수 */
	double secs, ops, util, avg_mm_util, avg_mm_throughput, p1, p2, perfindex;
	int numcorrect;

	/*
	 * 명령줄 인자를 읽고 해석한다
	 */
	while ((c = getopt(argc, argv, "f:t:hvVgal")) != EOF)
	{
		printf("getopt 반환값: %d\n", c); // 디버깅용 출력 추가

		switch (c)
		{
		case 'g': /* 자동 채점용 요약 정보 생성 */
			autograder = 1;
			break;
		case 'f': /* 특정 trace 파일 하나만 사용(현재 디렉터리 기준 상대 경로) */
			num_tracefiles = 1;
			if ((tracefiles = realloc(tracefiles, 2 * sizeof(char *))) == NULL)
				unix_error("오류: main에서 realloc에 실패했습니다");
			strcpy(tracedir, "./");
			tracefiles[0] = strdup(optarg);
			tracefiles[1] = NULL;
			break;
		case 't':					 /* trace가 위치한 디렉터리 */
			if (num_tracefiles == 1) /* 이미 -f가 나오면 무시 */
				break;
			strcpy(tracedir, optarg);
			if (tracedir[strlen(tracedir) - 1] != '/')
				strcat(tracedir, "/"); /* 경로는 항상 "/"로 끝난다 */
			break;
		case 'a': /* 팀 구조를 검사하지 않음 */
			team_check = 0;
			break;
		case 'l': /* libc malloc 실행 */
			run_libc = 1;
			break;
		case 'v': /* trace별 성능 분석 출력 */
			verbose = 1;
			break;
		case 'V': /* -v보다 더 자세한 출력 */
			verbose = 2;
			break;
		case 'h': /* 이 메시지를 출력 */
			usage();
			exit(0);
		default:
			usage();
			exit(1);
		}
	}

	/*
	 * 팀 정보를 확인하고 출력한다
	 */
	if (team_check)
	{
		/* 학생은 팀 정보를 반드시 입력해야 한다 */
		if (!strcmp(team.teamname, ""))
		{
			printf("오류: mm.c에 팀 정보를 입력해 주세요.\n");
			exit(1);
		}
		else
			printf("팀 이름:%s\n", team.teamname);
		if ((*team.name1 == '\0') || (*team.id1 == '\0'))
		{
			printf("오류: 팀원 1의 정보를 모두 입력해야 합니다.\n");
			exit(1);
		}
		else
			printf("팀원 1:%s:%s\n", team.name1, team.id1);

		if (((*team.name2 != '\0') && (*team.id2 == '\0')) ||
			((*team.name2 == '\0') && (*team.id2 != '\0')))
		{
			printf("오류: 팀원 2 정보는 모두 입력하거나 모두 비워야 합니다.\n");
			exit(1);
		}
		else if (*team.name2 != '\0')
			printf("팀원 2:%s:%s\n", team.name2, team.id2);
	}

	/*
	 * -f 명령줄 인자가 없으면 default_traces[]에 정의된
	 * 전체 trace 파일 집합을 사용한다
	 */
	if (tracefiles == NULL)
	{
		tracefiles = default_tracefiles;
		num_tracefiles = sizeof(default_tracefiles) / sizeof(char *) - 1;
		printf("%s의 기본 trace 파일을 사용합니다\n", tracedir);
	}

	/* 시간 측정 패키지를 초기화한다 */
	init_fsecs();

	/*
	 * 필요하면 libc malloc 패키지를 실행하고 평가한다
	 */
	if (run_libc)
	{
		if (verbose > 1)
			printf("\nlibc malloc을 테스트합니다\n");

		/* trace 파일마다 하나씩 stats_t를 가지는 libc 통계 배열을 할당한다 */
		libc_stats = (stats_t *)calloc(num_tracefiles, sizeof(stats_t));
		if (libc_stats == NULL)
			unix_error("main에서 libc_stats calloc에 실패했습니다");

		/* K-best 방식을 사용해 libc malloc 패키지를 평가한다 */
		for (i = 0; i < num_tracefiles; i++)
		{
			trace = read_trace(tracedir, tracefiles[i]);
			libc_stats[i].ops = trace->num_ops;
			if (verbose > 1)
				printf("libc malloc의 정확성을 검사하고, ");
			libc_stats[i].valid = eval_libc_valid(trace, i);
			if (libc_stats[i].valid)
			{
				speed_params.trace = trace;
				if (verbose > 1)
					printf("성능도 측정합니다.\n");
				libc_stats[i].secs = fsecs(eval_libc_speed, &speed_params);
			}
			free_trace(trace);
		}

		/* libc 결과를 간단한 표로 출력한다 */
		if (verbose)
		{
			printf("\nlibc malloc 결과:\n");
			printresults(num_tracefiles, libc_stats);
		}
	}

	/*
	 * 학생의 mm 패키지는 항상 실행하고 평가한다
	 */
	if (verbose > 1)
		printf("\nmm malloc을 테스트합니다\n");

	/* trace 파일마다 하나씩 stats_t를 가지는 mm 통계 배열을 할당한다 */
	mm_stats = (stats_t *)calloc(num_tracefiles, sizeof(stats_t));
	if (mm_stats == NULL)
		unix_error("main에서 mm_stats calloc에 실패했습니다");

	/* memlib.c의 시뮬레이션 메모리 시스템을 초기화한다 */
	mem_init();

	/* K-best 방식을 사용해 학생의 mm malloc 패키지를 평가한다 */
	for (i = 0; i < num_tracefiles; i++)
	{
		trace = read_trace(tracedir, tracefiles[i]);
		mm_stats[i].ops = trace->num_ops;
		if (verbose > 1)
			printf("mm_malloc의 정확성을 검사하고, ");
		mm_stats[i].valid = eval_mm_valid(trace, i, &ranges);
		if (mm_stats[i].valid)
		{
			if (verbose > 1)
				printf("효율을 확인하고, ");
			mm_stats[i].util = eval_mm_util(trace, i, &ranges);
			speed_params.trace = trace;
			speed_params.ranges = ranges;
			if (verbose > 1)
				printf("성능도 측정합니다.\n");
			mm_stats[i].secs = fsecs(eval_mm_speed, &speed_params);
		}
		free_trace(trace);
	}

	/* mm 결과를 간단한 표로 출력한다 */
	if (verbose)
	{
		printf("\nmm malloc 결과:\n");
		printresults(num_tracefiles, mm_stats);
		printf("\n");
	}

	/*
	 * 학생의 mm 패키지에 대한 전체 통계를 누적한다
	 */
	secs = 0;
	ops = 0;
	util = 0;
	numcorrect = 0;
	for (i = 0; i < num_tracefiles; i++)
	{
		secs += mm_stats[i].secs;
		ops += mm_stats[i].ops;
		util += mm_stats[i].util;
		if (mm_stats[i].valid)
			numcorrect++;
	}
	avg_mm_util = util / num_tracefiles;

	/*
	 * 성능 지수를 계산해 출력한다
	 */
	if (errors == 0)
	{
		avg_mm_throughput = ops / secs;

		p1 = UTIL_WEIGHT * avg_mm_util;
		if (avg_mm_throughput > AVG_LIBC_THRUPUT)
		{
			p2 = (double)(1.0 - UTIL_WEIGHT);
		}
		else
		{
			p2 = ((double)(1.0 - UTIL_WEIGHT)) *
				 (avg_mm_throughput / AVG_LIBC_THRUPUT);
		}

		perfindex = (p1 + p2) * 100.0;
		printf("성능 지수 = %.0f(활용도) + %.0f(처리량) = %.0f/100\n",
			   p1 * 100,
			   p2 * 100,
			   perfindex);
	}
	else
	{ /* 오류가 있었다 */
		perfindex = 0.0;
		printf("오류 %d개로 종료했습니다\n", errors);
	}

	if (autograder)
	{
		printf("correct:%d\n", numcorrect);
		printf("perfidx:%.0f\n", perfindex);
	}

	exit(0);
}

/*****************************************************************
 * 아래 루틴들은 range 리스트를 다룬다. range 리스트는
 * 할당된 각 블록 페이로드의 범위를 추적한다. 우리는
 * 이 리스트를 사용해 서로 겹치는 할당 블록을 탐지한다.
 ****************************************************************/

/*
 * add_range - trace tracenum의 요청 opnum에 따라 학생의 mm_malloc을 호출해
 *     주소 lo에 size 바이트 블록을 막 할당했다고 가정한다.
 *     이 블록의 정합성을 확인한 뒤, 해당 블록용 range 구조체를 생성해
 *     range 리스트에 추가한다.
 */
static int add_range(range_t **ranges, char *lo, int size,
					 int tracenum, int opnum)
{
	char *hi = lo + size - 1;
	range_t *p;
	char msg[MAXLINE];

	assert(size > 0);

	/* 페이로드 주소는 ALIGNMENT 바이트 정렬이어야 한다 */
	if (!IS_ALIGNED(lo))
	{
		sprintf(msg, "페이로드 주소 (%p)가 %d바이트 정렬이 아닙니다",
				lo, ALIGNMENT);
		malloc_error(tracenum, opnum, msg);
		return 0;
	}

	/* 페이로드는 힙의 범위 안에 있어야 한다 */
	if ((lo < (char *)mem_heap_lo()) || (lo > (char *)mem_heap_hi()) ||
		(hi < (char *)mem_heap_lo()) || (hi > (char *)mem_heap_hi()))
	{
		sprintf(msg, "페이로드 (%p:%p)가 힙 범위 (%p:%p) 밖에 있습니다",
				lo, hi, mem_heap_lo(), mem_heap_hi());
		malloc_error(tracenum, opnum, msg);
		return 0;
	}

	/* 페이로드는 다른 어떤 페이로드와도 겹치면 안 된다 */
	for (p = *ranges; p != NULL; p = p->next)
	{
		if ((lo >= p->lo && lo <= p->hi) ||
			(hi >= p->lo && hi <= p->hi))
		{
			sprintf(msg, "페이로드 (%p:%p)가 다른 페이로드 (%p:%p)와 겹칩니다\n",
					lo, hi, p->lo, p->hi);
			malloc_error(tracenum, opnum, msg);
			return 0;
		}
	}

	/*
	 * 모든 것이 정상으로 보이므로 range 구조체를 생성해
	 * 이 블록의 범위를 range 리스트에 기록한다.
	 */
	if ((p = (range_t *)malloc(sizeof(range_t))) == NULL)
		unix_error("add_range에서 malloc 오류가 발생했습니다");
	p->next = *ranges;
	p->lo = lo;
	p->hi = hi;
	*ranges = p;
	return 1;
}

/*
 * remove_range - 페이로드 시작 주소가 lo인 블록의 range 기록을 해제한다
 */
static void remove_range(range_t **ranges, char *lo)
{
	range_t *p;
	range_t **prevpp = ranges;
	int size;

	for (p = *ranges; p != NULL; p = p->next)
	{
		if (p->lo == lo)
		{
			*prevpp = p->next;
			size = p->hi - p->lo + 1;
			free(p);
			break;
		}
		prevpp = &(p->next);
	}
}

/*
 * clear_ranges - 하나의 trace에 대한 모든 range 기록을 해제한다
 */
static void clear_ranges(range_t **ranges)
{
	range_t *p;
	range_t *pnext;

	for (p = *ranges; p != NULL; p = pnext)
	{
		pnext = p->next;
		free(p);
	}
	*ranges = NULL;
}

/**********************************************
 * 아래 루틴들은 trace 파일을 다룬다
 *********************************************/

/*
 * read_trace - trace 파일을 읽어 메모리에 저장한다
 */
static trace_t *read_trace(char *tracedir, char *filename)
{
	FILE *tracefile;
	trace_t *trace;
	char type[MAXLINE];
	char path[MAXLINE];
	unsigned index, size;
	unsigned max_index = 0;
	unsigned op_index;

	if (verbose > 1)
		printf("trace 파일을 읽는 중: %s\n", filename);

	/* trace 레코드를 할당한다 */
	if ((trace = (trace_t *)malloc(sizeof(trace_t))) == NULL)
		unix_error("read_trace에서 첫 번째 malloc에 실패했습니다");

	/* trace 파일 헤더를 읽는다 */
	strcpy(path, tracedir);
	strcat(path, filename);
	if ((tracefile = fopen(path, "r")) == NULL)
	{
		sprintf(msg, "read_trace에서 %s 파일을 열 수 없습니다", path);
		unix_error(msg);
	}
	fscanf(tracefile, "%d", &(trace->sugg_heapsize)); /* 사용하지 않음 */
	fscanf(tracefile, "%d", &(trace->num_ids));
	fscanf(tracefile, "%d", &(trace->num_ops));
	fscanf(tracefile, "%d", &(trace->weight)); /* 사용하지 않음 */

	/* trace의 각 요청 줄은 이 배열에 저장한다 */
	if ((trace->ops =
			 (traceop_t *)malloc(trace->num_ops * sizeof(traceop_t))) == NULL)
		unix_error("read_trace에서 두 번째 malloc에 실패했습니다");

	/* 할당된 블록 포인터 배열은 여기에 저장한다 */
	if ((trace->blocks =
			 (char **)malloc(trace->num_ids * sizeof(char *))) == NULL)
		unix_error("read_trace에서 세 번째 malloc에 실패했습니다");

	/* 각 블록에 대응하는 바이트 크기도 함께 저장한다 */
	if ((trace->block_sizes =
			 (size_t *)malloc(trace->num_ids * sizeof(size_t))) == NULL)
		unix_error("read_trace에서 네 번째 malloc에 실패했습니다");

	/* trace 파일의 모든 요청 줄을 읽는다 */
	index = 0;
	op_index = 0;
	while (fscanf(tracefile, "%s", type) != EOF)
	{
		switch (type[0])
		{
		case 'a':
			fscanf(tracefile, "%u %u", &index, &size);
			trace->ops[op_index].type = ALLOC;
			trace->ops[op_index].index = index;
			trace->ops[op_index].size = size;
			max_index = (index > max_index) ? index : max_index;
			break;
		case 'r':
			fscanf(tracefile, "%u %u", &index, &size);
			trace->ops[op_index].type = REALLOC;
			trace->ops[op_index].index = index;
			trace->ops[op_index].size = size;
			max_index = (index > max_index) ? index : max_index;
			break;
		case 'f':
			fscanf(tracefile, "%ud", &index);
			trace->ops[op_index].type = FREE;
			trace->ops[op_index].index = index;
			break;
		default:
			printf("trace 파일 %s에 잘못된 타입 문자 (%c)가 있습니다\n",
				   path, type[0]);
			exit(1);
		}
		op_index++;
	}
	fclose(tracefile);
	assert(max_index == trace->num_ids - 1);
	assert(trace->num_ops == op_index);

	return trace;
}

/*
 * free_trace - trace 레코드와, read_trace()에서 할당했던
 *              세 개의 배열을 모두 해제한다.
 */
void free_trace(trace_t *trace)
{
	free(trace->ops); /* 세 배열을 해제한다 */
	free(trace->blocks);
	free(trace->block_sizes);
	free(trace); /* 마지막으로 trace 레코드 자체를 해제한다 */
}

/**********************************************************************
 * 아래 함수들은 libc 및 mm malloc 패키지의 정확성,
 * 공간 활용도, 처리량을 평가한다.
 **********************************************************************/

/*
 * eval_mm_valid - mm malloc 패키지의 정확성을 검사한다
 */
static int eval_mm_valid(trace_t *trace, int tracenum, range_t **ranges)
{
	int i, j;
	int index;
	int size;
	int oldsize;
	char *newp;
	char *oldp;
	char *p;

	/* 힙을 재설정하고 range 리스트의 기록을 모두 해제한다 */
	mem_reset_brk();
	clear_ranges(ranges);

	/* mm 패키지의 초기화 함수를 호출한다 */
	if (mm_init() < 0)
	{
		malloc_error(tracenum, 0, "mm_init에 실패했습니다.");
		return 0;
	}

	/* trace의 각 연산을 순서대로 해석한다 */
	for (i = 0; i < trace->num_ops; i++)
	{
		index = trace->ops[i].index;
		size = trace->ops[i].size;

		switch (trace->ops[i].type)
		{

		case ALLOC: /* mm_malloc */

			/* 학생의 malloc을 호출한다 */
			if ((p = mm_malloc(size)) == NULL)
			{
				malloc_error(tracenum, i, "mm_malloc에 실패했습니다.");
				return 0;
			}

			/*
			 * 새 블록의 범위를 검사해 올바르면 range 리스트에 추가한다.
			 * 블록은 올바르게 정렬되어 있어야 하고,
			 * 현재 할당된 다른 블록과 겹치면 안 된다.
			 */
			if (add_range(ranges, p, size, tracenum, i) == 0)
				return 0;

			/* 추가: cgw
			 * 범위를 index의 하위 1바이트 값으로 채운다.
			 * 나중에 블록을 realloc할 때, 이전 데이터가 새 블록으로
			 * 제대로 복사되었는지 확인하는 데 사용한다.
			 */
			memset(p, index & 0xFF, size);

			/* 영역을 기록한다 */
			trace->blocks[index] = p;
			trace->block_sizes[index] = size;
			break;

		case REALLOC: /* mm_realloc */

			/* 학생의 realloc을 호출한다 */
			oldp = trace->blocks[index];
			if ((newp = mm_realloc(oldp, size)) == NULL)
			{
				malloc_error(tracenum, i, "mm_realloc에 실패했습니다.");
				return 0;
			}

			/* 이전 영역을 range 리스트에서 제거한다 */
			remove_range(ranges, oldp);

			/* 새 블록이 올바른지 검사하고 range 리스트에 추가한다 */
			if (add_range(ranges, newp, size, tracenum, i) == 0)
				return 0;

			/* 추가: cgw
			 * 새 블록에 이전 블록의 데이터가 들어 있는지 확인한 뒤,
			 * 새 인덱스의 하위 1바이트 값으로 새 블록을 채운다.
			 */
			oldsize = trace->block_sizes[index];
			if (size < oldsize)
				oldsize = size;
			for (j = 0; j < oldsize; j++)
			{
				if (newp[j] != (index & 0xFF))
				{
					malloc_error(tracenum, i, "mm_realloc이 이전 블록의 "
											  "데이터를 보존하지 못했습니다");
					return 0;
				}
			}
			memset(newp, index & 0xFF, size);

			/* 영역을 기록한다 */
			trace->blocks[index] = newp;
			trace->block_sizes[index] = size;
			break;

		case FREE: /* mm_free */

			/* 리스트에서 영역을 제거하고 학생의 free 함수를 호출한다 */
			p = trace->blocks[index];
			remove_range(ranges, p);
			mm_free(p);
			break;

		default:
			app_error("eval_mm_valid에 존재하지 않는 요청 타입이 들어왔습니다");
		}
	}

	/* 현재까지 확인한 바로는 유효한 malloc 패키지이다 */
	return 1;
}

/*
 * eval_mm_util - 학생 패키지의 공간 활용도를 평가한다
 *   핵심 아이디어는 최적의 할당기, 즉 빈틈도 내부 단편화도 없는 할당기에 대해
 *   힙의 최고 수위(high water mark) "hwm"을 기억하는 것이다.
 *   활용도는 hwm/heapsize 비율이며, heapsize는 학생 malloc 패키지가
 *   trace를 실행한 뒤의 힙 크기(바이트)이다. mem_sbrk() 구현은 학생이
 *   brk 포인터를 줄일 수 없게 하므로, brk는 항상 힙의 최고 수위가 된다.
 *
 */
static double eval_mm_util(trace_t *trace, int tracenum, range_t **ranges)
{
	int i;
	int index;
	int size, newsize, oldsize;
	int max_total_size = 0;
	int total_size = 0;
	char *p;
	char *newp, *oldp;

	/* 힙과 mm malloc 패키지를 초기화한다 */
	mem_reset_brk();
	if (mm_init() < 0)
		app_error("eval_mm_util에서 mm_init에 실패했습니다");

	for (i = 0; i < trace->num_ops; i++)
	{
		switch (trace->ops[i].type)
		{

		case ALLOC: /* mm_alloc */
			index = trace->ops[i].index;
			size = trace->ops[i].size;

			if ((p = mm_malloc(size)) == NULL)
				app_error("eval_mm_util에서 mm_malloc에 실패했습니다");

			/* 영역과 크기를 기록한다 */
			trace->blocks[index] = p;
			trace->block_sizes[index] = size;

			/* 현재 할당된 모든 블록의 총 크기를 추적한다 */
			total_size += size;

			/* 통계를 갱신한다 */
			max_total_size = (total_size > max_total_size) ? total_size : max_total_size;
			break;

		case REALLOC: /* mm_realloc */
			index = trace->ops[i].index;
			newsize = trace->ops[i].size;
			oldsize = trace->block_sizes[index];

			oldp = trace->blocks[index];
			if ((newp = mm_realloc(oldp, newsize)) == NULL)
				app_error("eval_mm_util에서 mm_realloc에 실패했습니다");

			/* 영역과 크기를 기록한다 */
			trace->blocks[index] = newp;
			trace->block_sizes[index] = newsize;

			/* 현재 할당된 모든 블록의 총 크기를 추적한다 */
			total_size += (newsize - oldsize);

			/* 통계를 갱신한다 */
			max_total_size = (total_size > max_total_size) ? total_size : max_total_size;
			break;

		case FREE: /* mm_free */
			index = trace->ops[i].index;
			size = trace->block_sizes[index];
			p = trace->blocks[index];

			mm_free(p);

			/* 현재 할당된 모든 블록의 총 크기를 추적한다 */
			total_size -= size;

			break;

		default:
			app_error("eval_mm_util에 존재하지 않는 요청 타입이 들어왔습니다");
		}
	}

	return ((double)max_total_size / (double)mem_heapsize());
}

/*
 * eval_mm_speed - fcyc()가 mm malloc 패키지의 실행 시간을
 *    측정할 때 사용하는 함수이다.
 */
static void eval_mm_speed(void *ptr)
{
	int i, index, size, newsize;
	char *p, *newp, *oldp, *block;
	trace_t *trace = ((speed_t *)ptr)->trace;

	/* 힙을 재설정하고 mm 패키지를 초기화한다 */
	mem_reset_brk();
	if (mm_init() < 0)
		app_error("eval_mm_speed에서 mm_init에 실패했습니다");

	/* 각 trace 요청을 해석한다 */
	for (i = 0; i < trace->num_ops; i++)
		switch (trace->ops[i].type)
		{

		case ALLOC: /* mm_malloc */
			index = trace->ops[i].index;
			size = trace->ops[i].size;
			if ((p = mm_malloc(size)) == NULL)
				app_error("eval_mm_speed에서 mm_malloc 오류가 발생했습니다");
			trace->blocks[index] = p;
			break;

		case REALLOC: /* mm_realloc */
			index = trace->ops[i].index;
			newsize = trace->ops[i].size;
			oldp = trace->blocks[index];
			if ((newp = mm_realloc(oldp, newsize)) == NULL)
				app_error("eval_mm_speed에서 mm_realloc 오류가 발생했습니다");
			trace->blocks[index] = newp;
			break;

		case FREE: /* mm_free */
			index = trace->ops[i].index;
			block = trace->blocks[index];
			mm_free(block);
			break;

		default:
			app_error("eval_mm_valid에 존재하지 않는 요청 타입이 들어왔습니다");
		}
}

/*
 * eval_libc_valid - 이 함수는 libc malloc이 해당 trace 집합을
 *    끝까지 실행할 수 있는지 확인하기 위해 실행한다.
 *    보수적으로 접근하여 libc malloc 호출이 하나라도 실패하면 종료한다.
 *
 */
static int eval_libc_valid(trace_t *trace, int tracenum)
{
	int i, newsize;
	char *p, *newp, *oldp;

	for (i = 0; i < trace->num_ops; i++)
	{
		switch (trace->ops[i].type)
		{

		case ALLOC: /* malloc */
			if ((p = malloc(trace->ops[i].size)) == NULL)
			{
				malloc_error(tracenum, i, "libc malloc에 실패했습니다");
				unix_error("시스템 메시지");
			}
			trace->blocks[trace->ops[i].index] = p;
			break;

		case REALLOC: /* realloc */
			newsize = trace->ops[i].size;
			oldp = trace->blocks[trace->ops[i].index];
			if ((newp = realloc(oldp, newsize)) == NULL)
			{
				malloc_error(tracenum, i, "libc realloc에 실패했습니다");
				unix_error("시스템 메시지");
			}
			trace->blocks[trace->ops[i].index] = newp;
			break;

		case FREE: /* free */
			free(trace->blocks[trace->ops[i].index]);
			break;

		default:
			app_error("eval_libc_valid에 잘못된 연산 타입이 들어왔습니다");
		}
	}

	return 1;
}

/*
 * eval_libc_speed - 이 함수는 fcyc()가 trace 집합에서 libc malloc 패키지의
 *    실행 시간을 측정할 때 사용한다.
 */
static void eval_libc_speed(void *ptr)
{
	int i;
	int index, size, newsize;
	char *p, *newp, *oldp, *block;
	trace_t *trace = ((speed_t *)ptr)->trace;

	for (i = 0; i < trace->num_ops; i++)
	{
		switch (trace->ops[i].type)
		{
		case ALLOC: /* malloc */
			index = trace->ops[i].index;
			size = trace->ops[i].size;
			if ((p = malloc(size)) == NULL)
				unix_error("eval_libc_speed에서 malloc에 실패했습니다");
			trace->blocks[index] = p;
			break;

		case REALLOC: /* realloc */
			index = trace->ops[i].index;
			newsize = trace->ops[i].size;
			oldp = trace->blocks[index];
			if ((newp = realloc(oldp, newsize)) == NULL)
				unix_error("eval_libc_speed에서 realloc에 실패했습니다");

			trace->blocks[index] = newp;
			break;

		case FREE: /* free */
			index = trace->ops[i].index;
			block = trace->blocks[index];
			free(block);
			break;
		}
	}
}

/*************************************
 * 기타 보조 루틴
 ************************************/

/*
 * printresults - malloc 패키지의 성능 요약을 출력한다
 */
static void printresults(int n, stats_t *stats)
{
	int i;
	double secs = 0;
	double ops = 0;
	double util = 0;

	/* 각 trace의 개별 결과를 출력한다 */
	printf("%5s%7s %5s%8s%10s%6s\n",
		   "추적", " 정상", "활용", "연산", "시간", "Kops");
	for (i = 0; i < n; i++)
	{
		if (stats[i].valid)
		{
			printf("%2d%10s%5.0f%%%8.0f%10.6f%6.0f\n",
				   i,
				   "예",
				   stats[i].util * 100.0,
				   stats[i].ops,
				   stats[i].secs,
				   (stats[i].ops / 1e3) / stats[i].secs);
			secs += stats[i].secs;
			ops += stats[i].ops;
			util += stats[i].util;
		}
		else
		{
			printf("%2d%10s%6s%8s%10s%6s\n",
				   i,
				   "아니오",
				   "-",
				   "-",
				   "-",
				   "-");
		}
	}

	/* trace 집합의 전체 결과를 출력한다 */
	if (errors == 0)
	{
		printf("%12s%5.0f%%%8.0f%10.6f%6.0f\n",
			   "합계        ",
			   (util / n) * 100.0,
			   ops,
			   secs,
			   (ops / 1e3) / secs);
	}
	else
	{
		printf("%12s%6s%8s%10s%6s\n",
			   "합계        ",
			   "-",
			   "-",
			   "-",
			   "-");
	}
}

/*
 * app_error - 임의의 애플리케이션 오류를 보고한다
 */
void app_error(char *msg)
{
	printf("%s\n", msg);
	exit(1);
}

/*
 * unix_error - Unix 스타일 오류를 보고한다
 */
void unix_error(char *msg)
{
	printf("%s: %s\n", msg, strerror(errno));
	exit(1);
}

/*
 * malloc_error - mm_malloc 패키지가 반환한 오류를 보고한다
 */
void malloc_error(int tracenum, int opnum, char *msg)
{
	errors++;
	printf("오류 [trace %d, 줄 %d]: %s\n", tracenum, LINENUM(opnum), msg);
}

/*
 * usage - 명령줄 인자를 설명한다
 */
static void usage(void)
{
	fprintf(stderr, "사용법: mdriver [-hvVal] [-f <파일>] [-t <디렉터리>]\n");
	fprintf(stderr, "옵션\n");
	fprintf(stderr, "\t-a         팀 구조를 검사하지 않습니다.\n");
	fprintf(stderr, "\t-f <파일>  <파일>을 trace 파일로 사용합니다.\n");
	fprintf(stderr, "\t-g         자동 채점용 요약 정보를 생성합니다.\n");
	fprintf(stderr, "\t-h         이 메시지를 출력합니다.\n");
	fprintf(stderr, "\t-l         libc malloc도 함께 실행합니다.\n");
	fprintf(stderr, "\t-t <디렉터리> 기본 trace를 찾을 디렉터리입니다.\n");
	fprintf(stderr, "\t-v         trace별 성능 분석을 출력합니다.\n");
	fprintf(stderr, "\t-V         추가 디버그 정보를 출력합니다.\n");
}
