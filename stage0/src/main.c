#include "codegen.h"
#include "parser.h"
#include "runtime.h"
#include "sema.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char* read_entire_file(const char* path, int32_t* len_out) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "mokac error: cannot open input file '%s'\n", path);
        return NULL;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        fprintf(stderr, "mokac error: failed to seek '%s'\n", path);
        return NULL;
    }
    long size = ftell(f);
    if (size < 0) {
        fclose(f);
        fprintf(stderr, "mokac error: failed to determine size of '%s'\n", path);
        return NULL;
    }
    rewind(f);

    char* buf = (char*)malloc((size_t)size + 1);
    if (!buf) {
        fclose(f);
        fprintf(stderr, "mokac error: out of memory reading '%s'\n", path);
        return NULL;
    }
    if (size > 0) {
        size_t n = fread(buf, 1, (size_t)size, f);
        if ((long)n != size) {
            free(buf);
            fclose(f);
            fprintf(stderr, "mokac error: failed to read '%s'\n", path);
            return NULL;
        }
    }
    buf[size] = '\0';
    fclose(f);
    *len_out = (int32_t)size;
    return buf;
}

static void print_usage(void) {
    fprintf(stderr,
            "Usage: mokac <file1.moka> [file2.moka ...] [-o <output.c>] [--runtime <header>]\n");
}

int main(int argc, char** argv) {
    const char* out_path = "out.c";
    const char* runtime_include = "runtime.h";
    const char** inputs = (const char**)malloc((size_t)argc * sizeof(char*));
    int32_t input_count = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage();
            free(inputs);
            return 0;
        } else if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "mokac error: missing filename after '-o'\n");
                free(inputs);
                return 1;
            }
            out_path = argv[++i];
        } else if (strcmp(argv[i], "--runtime") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "mokac error: missing header path after '--runtime'\n");
                free(inputs);
                return 1;
            }
            runtime_include = argv[++i];
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "mokac error: unknown option '%s'\n", argv[i]);
            free(inputs);
            return 1;
        } else {
            inputs[input_count++] = argv[i];
        }
    }

    if (input_count == 0) {
        print_usage();
        free(inputs);
        return 1;
    }

    RtArena arena;
    rt_arena_init(&arena);

    /* Keep source buffers alive until after sema/codegen since AST may reference them */
    char** src_buffers = (char**)calloc((size_t)input_count, sizeof(char*));
    ClassDecl** all_classes = NULL;
    int32_t total_classes = 0;
    int32_t cap_classes = 0;

    bool ok = true;
    for (int32_t i = 0; i < input_count && ok; i++) {
        int32_t len = 0;
        src_buffers[i] = read_entire_file(inputs[i], &len);
        if (!src_buffers[i]) {
            ok = false;
            break;
        }

        Parser p;
        parser_init(&p, &arena, inputs[i], src_buffers[i], len);
        ClassDecl** file_classes = NULL;
        int32_t file_count = 0;
        if (!parser_parse_classes(&p, &file_classes, &file_count)) {
            ok = false;
            break;
        }

        if (total_classes + file_count > cap_classes) {
            int32_t new_cap = cap_classes == 0 ? 16 : cap_classes * 2;
            while (new_cap < total_classes + file_count) {
                new_cap *= 2;
            }
            all_classes = (ClassDecl**)realloc(all_classes, (size_t)new_cap * sizeof(ClassDecl*));
            cap_classes = new_cap;
        }
        for (int32_t c = 0; c < file_count; c++) {
            all_classes[total_classes++] = file_classes[c];
        }
    }

    if (ok) {
        Program prog;
        prog.classes = all_classes;
        prog.class_count = total_classes;

        if (!sema_analyze(&prog, &arena)) {
            ok = false;
        } else {
            FILE* out = (strcmp(out_path, "-") == 0) ? stdout : fopen(out_path, "w");
            if (!out) {
                fprintf(stderr, "mokac error: cannot open output file '%s'\n", out_path);
                ok = false;
            } else {
                ok = codegen_emit(&prog, out, runtime_include);
                if (out != stdout) {
                    fclose(out);
                }
            }
        }
    }

    for (int32_t i = 0; i < input_count; i++) {
        free(src_buffers[i]);
    }
    free(src_buffers);
    free(all_classes);
    free(inputs);
    rt_arena_free(&arena);

    return ok ? 0 : 1;
}
