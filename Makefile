CC ?= cc
CFLAGS = -Wall -Wextra -O1 -g -Isrc -Ibuild -Wno-unused-function -Wno-sign-compare

OBJS = build/parser.tab.o build/lex.yy.o build/ast.o build/interp.o build/codegen_py.o build/main.o

minic: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

build/parser.tab.c build/parser.tab.h: src/parser.y | build
	bison -d -v -o build/parser.tab.c src/parser.y

build/lex.yy.c: src/lexer.l build/parser.tab.h | build
	flex -o $@ src/lexer.l

build/%.o: build/%.c
	$(CC) $(CFLAGS) -Wno-unused-parameter -c -o $@ $<

build/%.o: src/%.c build/parser.tab.h | build
	$(CC) $(CFLAGS) -c -o $@ $<

build:
	mkdir -p build

test: minic
	./tests/run_tests.sh

view: minic
	python3 tools/pack.py

conflicts:
	./demo/run_conflicts.sh

clean:
	rm -rf build minic web/data.js
