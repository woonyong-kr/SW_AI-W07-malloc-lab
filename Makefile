CC ?= gcc
CFLAGS ?= -O0 -g -Wall -Wextra -std=c11

MMAP_DEMO_SRC := tools/mmap_one_byte_demo.c
MMAP_DEMO_BIN := tools/mmap_one_byte_demo
MMAP_DEMO_FILE := tools/mmap_demo.txt

.PHONY: help mmap-demo-build mmap-demo-run mmap-demo-run-private mmap-demo-clean

help:
	@echo "  make mmap-demo-build        # 아주 작은 mmap 실험 빌드"
	@echo "  make mmap-demo-run          # MAP_SHARED 실험 실행"
	@echo "  make mmap-demo-run-private  # MAP_PRIVATE 실험 실행"
	@echo "  make mmap-demo-clean        # 실험 바이너리 삭제"

$(MMAP_DEMO_BIN): $(MMAP_DEMO_SRC)
	$(CC) $(CFLAGS) -o $@ $<

mmap-demo-build: $(MMAP_DEMO_BIN)

mmap-demo-run: mmap-demo-build
	@echo "다른 터미널에서 같이 보기:"
	@echo "  watch -n 0.5 'xxd -g1 -l 128 $(MMAP_DEMO_FILE)'"
	./$(MMAP_DEMO_BIN) shared $(MMAP_DEMO_FILE)

mmap-demo-run-private: mmap-demo-build
	@echo "다른 터미널에서 같이 보기:"
	@echo "  watch -n 0.5 'xxd -g1 -l 128 $(MMAP_DEMO_FILE)'"
	./$(MMAP_DEMO_BIN) private $(MMAP_DEMO_FILE)

mmap-demo-clean:
	rm -f $(MMAP_DEMO_BIN)
