/* 
 * 함수 타이머
 */
typedef void (*ftimer_test_funct)(void *); 

/* Unix 인터벌 타이머를 사용해 f(argp)의 실행 시간을 추정한다.
   n번 실행한 평균값을 반환한다 */
double ftimer_itimer(ftimer_test_funct f, void *argp, int n);


/* gettimeofday를 사용해 f(argp)의 실행 시간을 추정한다.
   n번 실행한 평균값을 반환한다 */
double ftimer_gettod(ftimer_test_funct f, void *argp, int n);

