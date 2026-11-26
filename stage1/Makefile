CC = cc
CFLAGS = -std=c11 -Wall -Wextra -Werror -pthread -g
STAGE0 = ../stage0/build/mokac
RUNTIME_DIR = runtime
STD_FILES = std/String.moka std/Console.moka std/ByteBuffer.moka std/Io.moka std/Exception.moka std/Runnable.moka std/Thread.moka std/Mutex.moka
BOOT_STD_FILES = std_boot/String.moka std_boot/Console.moka std_boot/ByteBuffer.moka std_boot/Io.moka std_boot/Exception.moka
SRC_FILES = src/Token.moka src/Lexer.moka src/Ast.moka src/Parser.moka src/Sema.moka src/Codegen.moka src/Main.moka

.PHONY: all stage0 mokac test-std test-lexer test-e2e test-gc test-features test-inheritance test-interfaces test-console-input test-import test-static-fields test-phase-a test-phase-b test-phase-c test-final test-phase-d test-phase-e test-phase-f test-bootstrap clean

all: mokac test-std test-lexer test-e2e test-gc test-features test-inheritance test-interfaces test-console-input test-import test-static-fields test-phase-a test-phase-b test-phase-c test-final test-phase-d test-phase-e test-phase-f test-bootstrap

stage0:
	$(MAKE) -C ../stage0 mokac

mokac: build/mokac

build/mokac: stage0 $(BOOT_STD_FILES) $(SRC_FILES)
	@mkdir -p build
	$(STAGE0) $(BOOT_STD_FILES) $(SRC_FILES) -o build/stage1.c
	$(CC) $(CFLAGS) -I$(RUNTIME_DIR) build/stage1.c -o build/mokac -lm

test-std: stage0 $(BOOT_STD_FILES) tests/test_std.moka
	@mkdir -p build
	$(STAGE0) $(BOOT_STD_FILES) tests/test_std.moka -o build/test_std.c
	$(CC) $(CFLAGS) -I$(RUNTIME_DIR) build/test_std.c -o build/test_std -lm
	./build/test_std

test-lexer: stage0 $(BOOT_STD_FILES) src/Token.moka src/Lexer.moka tests/test_lexer.moka
	@mkdir -p build
	$(STAGE0) $(BOOT_STD_FILES) src/Token.moka src/Lexer.moka tests/test_lexer.moka -o build/test_lexer.c
	$(CC) $(CFLAGS) -I$(RUNTIME_DIR) build/test_lexer.c -o build/test_lexer -lm
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

test-interfaces: build/mokac $(STD_FILES) tests/test_interfaces.moka
	./build/mokac tests/test_interfaces.moka -o build/test_interfaces
	./build/test_interfaces

test-console-input: build/mokac $(STD_FILES) tests/test_console_input.moka
	./build/mokac tests/test_console_input.moka -o build/test_console_input
	printf "Moka\n-42\n" | ./build/test_console_input

test-import: build/mokac $(STD_FILES) tests/test_import.moka tests/import_helper.moka
	./build/mokac tests/test_import.moka -o build/test_import
	./build/test_import

test-static-fields: build/mokac $(STD_FILES) tests/test_static_fields.moka
	./build/mokac tests/test_static_fields.moka -o build/test_static_fields
	./build/test_static_fields

test-phase-a: build/mokac $(STD_FILES) tests/test_phase_a.moka
	./build/mokac tests/test_phase_a.moka -o build/test_phase_a
	./build/test_phase_a

test-phase-b: build/mokac $(STD_FILES) tests/test_phase_b.moka
	./build/mokac tests/test_phase_b.moka -o build/test_phase_b
	./build/test_phase_b

test-phase-c: build/mokac $(STD_FILES) tests/test_phase_c.moka
	./build/mokac tests/test_phase_c.moka -o build/test_phase_c
	./build/test_phase_c

test-final: build/mokac $(STD_FILES) tests/test_final.moka
	./build/mokac tests/test_final.moka -o build/test_final
	./build/test_final

test-phase-d: build/mokac $(STD_FILES) tests/test_phase_d.moka
	./build/mokac tests/test_phase_d.moka -o build/test_phase_d
	./build/test_phase_d

test-phase-e: build/mokac $(STD_FILES) tests/test_phase_e.moka
	./build/mokac tests/test_phase_e.moka -o build/test_phase_e
	./build/test_phase_e

test-phase-f: build/mokac $(STD_FILES) tests/test_phase_f.moka
	./build/mokac tests/test_phase_f.moka -o build/test_phase_f
	./build/test_phase_f

test-bootstrap: build/mokac $(BOOT_STD_FILES) $(SRC_FILES)
	./build/mokac --emit-c $(BOOT_STD_FILES) $(SRC_FILES) -o build/stage1_gen1.c
	$(CC) $(CFLAGS) -I$(RUNTIME_DIR) build/stage1_gen1.c -o build/mokac_s2 -lm
	./build/mokac_s2 --emit-c $(BOOT_STD_FILES) $(SRC_FILES) -o build/stage1_gen2.c
	diff -u build/stage1_gen1.c build/stage1_gen2.c
	@echo "Bootstrap fixed-point verified: stage1_gen1.c == stage1_gen2.c"

clean:
	rm -rf build
