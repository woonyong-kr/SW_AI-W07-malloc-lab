/* 사이클 카운터 사용 루틴 */

/* 카운터를 시작한다 */
void start_counter();

/* 카운터 시작 이후의 사이클 수를 구한다 */
double get_counter();

/* 카운터 오버헤드를 측정한다 */
double ovhd();

/* 기본 sleeptime을 사용해 프로세서의 클록 속도를 구한다 */
double mhz(int verbose);

/* 정확도를 더 세밀하게 제어하며 프로세서의 클록 속도를 구한다 */
double mhz_full(int verbose, int sleeptime);

/** 타이머 인터럽트 오버헤드를 보정하는 특수 카운터 */

void start_comp_counter();

double get_comp_counter();
