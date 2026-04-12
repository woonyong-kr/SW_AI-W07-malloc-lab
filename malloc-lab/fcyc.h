/*
 * fcyc.h - 테스트 함수 f가 사용한 CPU 사이클 시간을 추정하는
 *     fcyc.c 루틴의 프로토타입
 * 
 * Copyright (c) 2002, R. Bryant와 D. O'Hallaron, 모든 권리 보유.
 * 허가 없이 사용, 수정, 복사할 수 없다.
 *
 */

/* 테스트 함수는 범용 포인터를 입력으로 받는다 */
typedef void (*test_funct)(void *);

/* 테스트 함수 f가 사용한 사이클 수를 계산한다 */
double fcyc(test_funct f, void* argp);

/*********************************************************
 * 측정 루틴에서 사용하는 여러 파라미터를 설정한다
 *********************************************************/

/* 
 * set_fcyc_clear_cache - 설정되면 각 측정 전에 캐시를 비우는 코드를 실행한다.
 *     기본값 = 0
 */
void set_fcyc_clear_cache(int clear);

/* 
 * set_fcyc_cache_size - 캐시를 비울 때 사용할 캐시 크기를 설정한다.
 *     기본값 = 1<<19 (512KB)
 */
void set_fcyc_cache_size(int bytes);

/* 
 * set_fcyc_cache_block - 캐시 블록 크기를 설정한다.
 *     기본값 = 32
 */
void set_fcyc_cache_block(int bytes);

/* 
 * set_fcyc_compensate - 설정되면 타이머 인터럽트 오버헤드를 보정하려고 시도한다.
 *     기본값 = 0
 */
void set_fcyc_compensate(int compensate_arg);

/* 
 * set_fcyc_k - K-best 측정 방식에서 사용할 K 값을 설정한다.
 *     기본값 = 3
 */
void set_fcyc_k(int k);

/* 
 * set_fcyc_maxsamples - 허용 오차 안에서 K-best를 찾기 위해 시도할
 *     최대 샘플 수를 설정한다.
 *     이를 넘기면 찾은 최적 샘플을 그대로 반환한다.
 *     기본값 = 20
 */
void set_fcyc_maxsamples(int maxsamples_arg);

/* 
 * set_fcyc_epsilon - K-best에 필요한 허용 오차를 설정한다.
 *     기본값 = 0.01
 */
void set_fcyc_epsilon(double epsilon_arg);




