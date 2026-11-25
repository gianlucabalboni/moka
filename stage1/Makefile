CC = cc
CFLAGS = -std=c11 -Wall -Wextra -Werror -g
STAGE0 = ../stage0/build/mokac
RUNTIME_DIR = runtime
STD_FILES = std/String.moka std/Console.moka std/ByteBuffer.moka std/Io.moka
SRC_FILES = src/Token.moka src/Lexer.moka src/Ast.moka src/Parser.moka src/Sema.moka src/Codegen.moka src/Main.moka

.PHONY: all stage0 mokac test-std test-lexer test-e2e test-gc test-features test-inheritance test-bootstrap clean

all: mokac test-std test-lexer test-e2e test-gc test-features test-inheritance test-bootstrap

stage0:
	$(MAKE) -C ../stage0 mokac

mokac: build/mokac

build/mokac: stage0 $(STD_FILES) $(SRC_FILES)
	@mkdir -p build
	$(STAGE0) $(STD_FILES) $(SRC_FILES) -o build/stage1.c
	$(CC) $(CFLAGS) -I$(RUNTIME_DIR) build/stage1.c -o build/mokac

test-std: stage0 $(STD_FILES) tests/test_std.moka
	@mkdir -p build
	$(STAGE0) $(STD_FILES) tests/test_std.moka -o build/test_std.c
	$(CC) $(CFLAGS) -I$(RUNTIME_DIR) build/test_std.c -o build/test_std
	./build/test_std

test-lexer: stage0 $(STD_FILES) src/Token.moka src/Lexer.moka tests/test_lexer.moka
	@mkdir -p build
	$(STAGE0) $(STD_FILES) src/Token.moka src/Lexer.moka tests/test_lexer.moka -o build/test_lexer.c
	$(CC) $(CFLAGS) -I$(RUNTIME_DIR) build/test_lexer.c -o build/test_lexer
	./build/test_lexer

test-e2e: build/mokac $(STD_FILES) tests/hello_console.moka
	./build/mokac tests/hello_console.moka -o build/hello_console
	./build/hello_console

test-gc: build/mokac $(STD_FILES) tests/test_gc.moka
	./build/mokac tests/test_gc.moka -o build/test_gc
	./build/test_gc

test-features: build/mokac $(STD_FILES) tests/test_stage1_features.moka
	./build/mokac tests/test_stage1_features.moka -o build/test_stage1_features
	./build/test_stage1_features

test-inheritance: build/mokac $(STD_FILES) tests/test_inheritance.moka
	./build/mokac tests/test_inheritance.moka -o build/test_inheritance
	./build/test_inheritance

test-bootstrap: build/mokac $(STD_FILES) $(SRC_FILES)
	./build/mokac --emit-c $(STD_FILES) $(SRC_FILES) -o build/stage1_gen1.c
	$(CC) $(CFLAGS) -I$(RUNTIME_DIR) build/stage1_gen1.c -o build/mokac_s2
	./build/mokac_s2 --emit-c $(STD_FILES) $(SRC_FILES) -o build/stage1_gen2.c
	diff -u build/stage1_gen1.c build/stage1_gen2.c
	@echo "Bootstrap fixed-point verified: stage1_gen1.c == stage1_gen2.c"

clean:
	rm -rf build
