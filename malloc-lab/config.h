#ifndef __CONFIG_H_
#define __CONFIG_H_

/*
 * config.h - malloc lab 설정 파일
 *
 * Copyright (c) 2002, R. Bryant와 D. O'Hallaron, 모든 권리 보유.
 * 허가 없이 사용, 수정, 복사할 수 없다.
 */

/*
 * 드라이버가 기본 trace 파일을 찾을 때 사용하는 기본 경로이다.
 * 실행 시 -t 플래그로 이 값을 덮어쓸 수 있다.
 */
#define TRACEDIR "./traces/"

/*
 * 드라이버가 테스트에 사용할 TRACEDIR의 기본 trace 파일 목록이다.
 * 드라이버의 테스트 목록에 trace를 추가하거나 삭제하려면 이 값을 수정하라.
 * 예를 들어 학생들에게 realloc 구현을 요구하지 않으려면 마지막 두 개의
 * trace를 삭제하면 된다.
 */
#define DEFAULT_TRACEFILES \
  "amptjp-bal.rep",\
  "cccp-bal.rep",\
  "cp-decl-bal.rep",\
  "expr-bal.rep",\
  "coalescing-bal.rep",\
  "random-bal.rep",\
  "random2-bal.rep",\
  "binary-bal.rep",\
  "binary2-bal.rep",\
  "realloc-bal.rep",\
  "realloc2-bal.rep"

/*
 * 이 상수는 보통 학생들이 사용하는 것과 같은 종류의 기준 시스템에서,
 * 우리의 trace로 측정한 libc malloc 패키지의 예상 성능을 나타낸다.
 * 목적은 성능 지수에서 처리량이 기여하는 상한을 두는 것이다.
 * 학생들이 AVG_LIBC_THRUPUT를 넘어서면 점수상 추가 이득은 없다.
 * 이렇게 하면 매우 빠르지만 지나치게 단순한 malloc 패키지를 만드는 것을 막는다.
 */
#define AVG_LIBC_THRUPUT      600E3  /* 초당 600 Kops */

 /* 
  * 이 상수는 공간 활용도(UTIL_WEIGHT)와 처리량(1 - UTIL_WEIGHT)이
  * 성능 지수에 기여하는 비중을 결정한다.
  */
#define UTIL_WEIGHT .60

/* 
 * 바이트 단위 정렬 요구사항(4 또는 8)
 */
#define ALIGNMENT 8  

/* 
 * 바이트 단위 최대 힙 크기
 */
#define MAX_HEAP (20*(1<<20))  /* 20MB */

/*****************************************************************************
 * 시간 측정 방식을 선택하려면 아래 USE_xxx 상수 중 정확히 하나만 "1"로 설정하라.
 *****************************************************************************/
#define USE_FCYC   0   /* K-best 방식을 사용하는 사이클 카운터(x86 및 Alpha 전용) */
#define USE_ITIMER 0   /* 인터벌 타이머(모든 Unix 시스템) */
#define USE_GETTOD 1   /* gettimeofday(모든 Unix 시스템) */

#endif /* __CONFIG_H */
