# Malloc Lab 작업 가이드

이 문서는 이 저장소에서 실제로 과제를 진행할 때 필요한 정보만 모아 다시 정리한 안내서입니다.
핵심은 `mm.c`에 여러분만의 메모리 할당기를 구현하고, `mdriver`로 정확성과 성능을 검증하는 것입니다.

## 1. 이 과제의 목표

직접 `malloc`, `free`, `realloc`에 해당하는 할당기를 구현합니다.

- 구현 대상 함수
  - `mm_init`
  - `mm_malloc`
  - `mm_free`
  - `mm_realloc`
- 구현 파일
  - `mm.c`
- 제출 대상
  - 기본 과제 기준으로는 `mm.c`

현재 `mm.c`는 동작만 겨우 하는 naive 구현입니다.
이 상태로는 메모리 재사용이 없고, 병합(coalescing)도 없어서 성능 점수가 좋지 않습니다.

## 2. 어디서부터 보면 되나

추천 순서는 아래입니다.

1. `mm.c`
   - 지금 어떤 시작 코드가 들어 있는지 먼저 봅니다.
   - 팀 정보도 여기 채워야 합니다.
2. `mm.h`
   - 여러분이 구현해야 하는 함수 시그니처를 확인합니다.
3. `mdriver.c`
   - 드라이버가 어떤 기준으로 정확성을 검사하는지 봅니다.
   - 특히 payload 정렬, 힙 범위, 겹침 여부를 확인합니다.
4. `config.h`
   - 어떤 trace로 테스트하는지, 성능 가중치가 어떻게 되는지 봅니다.
5. `traces/README`
   - trace 파일 형식과 각 trace의 의도를 봅니다.

## 3. 실제로 수정해야 하는 파일

실질적인 구현 파일은 `mm.c` 하나라고 생각하면 됩니다.

- 수정 권장
  - `mm.c`
- 읽기 전용처럼 생각해도 되는 파일
  - `mdriver.c`
  - `config.h`
  - `memlib.{c,h}`
  - `fsecs.{c,h}`
  - `fcyc.{c,h}`
  - `clock.{c,h}`
  - `ftimer.{c,h}`

가장 먼저 `mm.c` 상단의 팀 정보를 본인 정보로 바꾸세요.

## 4. 구현할 때 알아야 할 핵심 규칙

### 4-1. 힙은 `mem_sbrk()`로만 늘립니다

이 과제의 힙은 실제 시스템 힙이 아니라 `memlib`가 흉내 낸 가상 힙입니다.
새 공간이 필요하면 `mem_sbrk()`를 호출해 늘려야 합니다.

### 4-2. free list 포인터는 보통 "빈 블록 내부"에 저장합니다

CS:APP에서 말하는 explicit free list, segregated free list 방식에서는
free block의 payload 영역 일부를 `prev`, `next` 포인터 저장용으로 쓰는 것이 일반적입니다.

즉, 이런 식입니다.

```c
[ header | prev ptr | next ptr | ... free payload ... | footer ]
```

### 4-3. free block 관리용 노드를 따로 `malloc()` 해서 쓰면 안 된다고 보는 게 맞습니다

질문하신 부분의 결론부터 말하면:

- free block 하나하나를 관리하기 위한 노드/포인터 저장소를
  `malloc()`으로 따로 할당해서 쓰는 건 하지 않는 것이 맞습니다.
- 과제 취지상 사실상 반칙에 가깝고, 구현 방식으로도 권장되지 않습니다.

이유는 다음과 같습니다.

- 여러분이 구현하는 대상이 이미 `malloc`입니다.
- 블록 메타데이터를 시스템 `malloc`에 따로 저장하면
  과제의 측정 대상 힙 바깥에 관리 정보를 숨기는 셈이 됩니다.
- 드라이버의 공간 활용도 계산은 `mem_sbrk()`로 늘어난 힙 기준으로 이뤄지므로,
  바깥 메모리를 쓰면 unfair한 결과가 나올 수 있습니다.

### 4-4. 다만 "전역/정적 포인터"는 써도 됩니다

예를 들어 아래 같은 것은 일반적으로 괜찮습니다.

- free list head를 가리키는 전역 포인터
- segregated list의 head 배열
- rover 포인터(next-fit용)
- tree root 포인터

즉,

- `static void *free_listp;`
- `static void *seg_heads[LIST_COUNT];`

같은 것은 괜찮고,

- free block마다 별도 노드 구조체를 `malloc()` 해서 붙이는 방식은 피해야 합니다.

## 5. 추천 구현 순서

처음부터 복잡한 구조로 가지 말고 단계적으로 가는 편이 좋습니다.

1. block layout을 정합니다
   - header/footer 형식
   - 할당 비트
   - 크기 단위
   - 정렬 단위
2. implicit free list로 먼저 끝까지 동작하게 만듭니다
   - `mm_init`
   - `extend_heap`
   - `find_fit`
   - `place`
   - `coalesce`
3. `short1-bal.rep`, `short2-bal.rep`를 통과시킵니다
4. 그 다음 explicit free list 또는 segregated free list로 바꿉니다
5. 마지막으로 `realloc` 성능을 개선합니다

## 6. 어떻게 테스트하나

이 디렉터리에서 실행합니다.

```bash
cd /Users/woonyong/workspace/Krafton-Jungle/SW_AI-W07-malloc-lab/malloc-lab
make
```

### 가장 작은 테스트

```bash
./mdriver -V -f short1-bal.rep
./mdriver -V -f short2-bal.rep
```

- `-V`: 자세한 추적 출력
- 특정 trace 하나만 집중해서 볼 때 가장 좋습니다

### 전체 기본 trace 실행

```bash
./mdriver -v
```

- `-v`: trace별 요약 출력
- 과제 전체 성능을 볼 때 사용합니다

### 드라이버 옵션 보기

```bash
./mdriver -h
```

자주 쓰는 옵션:

- `-f <file>`: 특정 trace만 실행
- `-v`: trace별 결과 출력
- `-V`: 더 자세한 디버그 출력
- `-l`: libc malloc과 함께 비교 실행
- `-a`: 팀 정보 체크 생략

## 7. 채점 기준

점수는 크게 두 축입니다.

- 정확성
- 성능
  - 공간 활용도(utilization)
  - 처리량(throughput)

### 정확성

드라이버는 대략 아래를 검사합니다.

- 반환 포인터가 정렬되어 있는가
- payload가 힙 범위 안에 있는가
- 다른 할당 블록과 겹치지 않는가
- `realloc`이 기존 데이터를 보존하는가

정확성에 문제가 있으면 성능 점수 이전에 실패합니다.

### 성능 가중치

`config.h` 기준:

- `UTIL_WEIGHT = 0.60`
- `AVG_LIBC_THRUPUT = 600E3`

즉 최종 평가는 대략:

- 활용도 비중 60%
- 처리량 비중 40%

드라이버는 이 값을 이용해 아래 형태로 점수를 계산합니다.

```text
성능 지수 = 활용도 점수 + 처리량 점수
최종 점수 = 100점 만점
```

처리량은 libc 기준 처리량을 넘는다고 해서 무한히 이득을 주지 않고,
상한이 걸려 있습니다.

## 8. 기본 trace는 무엇을 보는가

`config.h`의 기본 trace 목록은 아래 계열입니다.

- `short1`, `short2`
  - 아주 작은 디버깅용
- `amptjp`, `cccp`, `cp-decl`, `expr`
  - 실제 프로그램 기반 trace
- `binary`, `binary2`
  - 서로 다른 크기 패턴 대응
- `coalescing`
  - 병합이 제대로 되는지 확인
- `random`, `random2`
  - 전반적인 안정성 확인
- `realloc`, `realloc2`
  - `realloc` 품질 확인

특히 `realloc` trace가 기본 목록에 포함되어 있으므로,
단순히 새 블록을 잡고 항상 복사하는 naive `realloc`은 성능상 불리합니다.

## 9. trace 파일 형식

trace는 대략 이런 형식입니다.

```text
<sugg_heapsize>
<num_ids>
<num_ops>
<weight>
a <id> <bytes>
r <id> <bytes>
f <id>
```

의미:

- `a`: allocate
- `r`: realloc
- `f`: free

예:

```text
a 0 512
a 1 128
r 0 640
f 1
f 0
```

## 10. 디버깅 팁

- 처음엔 무조건 작은 trace 하나만 보세요.
- `short1-bal.rep`을 통과하기 전에는 전체 trace를 돌리지 않는 편이 낫습니다.
- block header/footer 출력용 헬퍼 함수를 잠깐 만들어 두면 빠릅니다.
- `coalescing-bal.rep`에서 병합이 안 되면 explicit list로 가도 계속 꼬입니다.
- `realloc-bal.rep`이 느리면 in-place 확장 가능성을 먼저 보세요.

## 11. 자주 헷갈리는 질문

### Q. free list 포인터는 따로 구조체를 `malloc()` 해서 관리해도 되나요?

권장하지 않습니다. 사실상 하지 않는 것이 맞습니다.

- per-block 메타데이터는 free block 내부에 저장하세요.
- list head, tree root, segregated head 배열 같은 전역/정적 포인터는 괜찮습니다.
- 하지만 free block 관리 노드를 시스템 `malloc()`으로 따로 빼는 방식은
  과제 취지와 공간 측정 기준에 맞지 않습니다.

### Q. header/footer는 꼭 둘 다 있어야 하나요?

반드시 그런 것은 아니지만,
초기 구현에서는 footer까지 두는 편이 coalescing 구현이 훨씬 쉽습니다.

### Q. 처음부터 explicit free list로 가야 하나요?

아닙니다.
implicit free list로 정확성을 먼저 맞춘 뒤 explicit/segregated로 가는 편이
훨씬 안정적입니다.

## 12. 한 줄 요약

이 과제는 `mm.c` 하나에 여러분만의 allocator를 구현하고,
`mdriver`로 정확성과 성능을 검증하는 문제입니다.

시작은:

1. 팀 정보 입력
2. implicit free list로 correctness 확보
3. explicit/segregated free list로 성능 개선
4. `realloc` 최적화

이 순서가 가장 안전합니다.
