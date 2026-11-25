CC = cc
CFLAGS = -std=c11 -Wall -Wextra -Werror -g

.PHONY: all mokac test test-runtime test-lexer test-parser test-sema test-codegen test-e2e clean

all: mokac test

mokac: build/mokac

build/mokac: src/main.c src/codegen.c src/codegen.h src/sema.c src/sema.h src/parser.c src/parser.h src/lexer.c src/lexer.h src/ast.h src/runtime.h
	@mkdir -p build
	$(CC) $(CFLAGS) src/main.c src/codegen.c src/sema.c src/parser.c src/lexer.c -o build/mokac

test: test-runtime test-lexer test-parser test-sema test-codegen test-e2e

test-runtime: tests/test_runtime.c src/runtime.h
	@mkdir -p build
	$(CC) $(CFLAGS) tests/test_runtime.c -o build/test_runtime
	./build/test_runtime

test-lexer: tests/test_lexer.c src/lexer.c src/lexer.h
	@mkdir -p build
	$(CC) $(CFLAGS) tests/test_lexer.c src/lexer.c -o build/test_lexer
	./build/test_lexer

test-parser: tests/test_parser.c src/parser.c src/parser.h src/lexer.c src/lexer.h src/ast.h src/runtime.h
	@mkdir -p build
	$(CC) $(CFLAGS) tests/test_parser.c src/parser.c src/lexer.c -o build/test_parser
	./build/test_parser

test-sema: tests/test_sema.c src/sema.c src/sema.h src/parser.c src/parser.h src/lexer.c src/lexer.h src/ast.h src/runtime.h
	@mkdir -p build
	$(CC) $(CFLAGS) tests/test_sema.c src/sema.c src/parser.c src/lexer.c -o build/test_sema
	./build/test_sema

test-codegen: tests/test_codegen.c src/codegen.c src/codegen.h src/sema.c src/sema.h src/parser.c src/parser.h src/lexer.c src/lexer.h src/ast.h src/runtime.h
	@mkdir -p build
	$(CC) $(CFLAGS) tests/test_codegen.c src/codegen.c src/sema.c src/parser.c src/lexer.c -o build/test_codegen
	./build/test_codegen

test-e2e: build/mokac std/String.moka std/Console.moka examples/hello.moka
	./build/mokac std/String.moka std/Console.moka examples/hello.moka -o build/hello.c
	$(CC) $(CFLAGS) -Isrc build/hello.c -o build/hello
	./build/hello arg1 arg2

build:
	mkdir -p build

clean:
	rm -rf build
