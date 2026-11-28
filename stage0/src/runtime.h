/**
 * runtime.h — Stage 0 C11 bootstrap runtime.
 *
 * Provides the Arena Allocator (RtArena), minimal object/array headers (Obj,
 * RtArray), string/byte helpers, and native I/O functions used by Stage 0
 * compiled programs (including compiling Stage 1).
 */
#ifndef MOKA_RUNTIME_H
#define MOKA_RUNTIME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* =========================================================================
 * Arena Allocator (8-byte aligned, zero-initialized)
 * ========================================================================= */

#define MOKA_ARENA_DEFAULT_CAP (1024 * 1024) /* 1 MB per block */

typedef struct RtArenaBlock {
    struct RtArenaBlock* next;
    size_t used;
    size_t cap;
    uint8_t data[];
} RtArenaBlock;

typedef struct RtArena {
    RtArenaBlock* head;
    RtArenaBlock* current;
} RtArena;

static inline size_t rt_align8(size_t n) {
    return (n + 7u) & ~(size_t)7u;
}

static inline RtArenaBlock* rt_arena_new_block(size_t min_cap) {
    size_t cap = min_cap > MOKA_ARENA_DEFAULT_CAP ? min_cap : MOKA_ARENA_DEFAULT_CAP;
    RtArenaBlock* block = (RtArenaBlock*)calloc(1, sizeof(RtArenaBlock) + cap);
    if (!block) {
        fprintf(stderr, "moka runtime error: out of memory allocating arena block (%zu bytes)\n", cap);
        exit(1);
    }
    block->next = NULL;
    block->used = 0;
    block->cap = cap;
    return block;
}

static inline void rt_arena_init(RtArena* arena) {
    arena->head = NULL;
    arena->current = NULL;
}

static inline void* rt_arena_alloc(RtArena* arena, size_t size) {
    size_t aligned = rt_align8(size);
    if (aligned == 0) {
        aligned = 8;
    }

    if (!arena->current || arena->current->used + aligned > arena->current->cap) {
        RtArenaBlock* block = rt_arena_new_block(aligned);
        if (!arena->head) {
            arena->head = block;
        } else {
            arena->current->next = block;
        }
        arena->current = block;
    }

    void* ptr = arena->current->data + arena->current->used;
    arena->current->used += aligned;
    return ptr; /* Already zero-initialized via calloc */
}

static inline void rt_arena_free(RtArena* arena) {
    RtArenaBlock* block = arena->head;
    while (block) {
        RtArenaBlock* next = block->next;
        free(block);
        block = next;
    }
    arena->head = NULL;
    arena->current = NULL;
}

/* =========================================================================
 * GC-Ready Object Header & Arrays
 * ========================================================================= */

typedef enum {
    OBJ_INSTANCE = 0,
    OBJ_VAL_ARRAY = 1, /* Primitive elements (int, byte, boolean) */
    OBJ_REF_ARRAY = 2  /* Reference elements (Obj*) */
} ObjType;

typedef struct Obj {
    ObjType type;
    bool marked;
    struct Obj* next;
} Obj;

typedef struct RtArray {
    Obj header;
    int32_t length;
    int32_t elem_size;
    uint8_t data[];
} RtArray;

/* Global runtime state */
static RtArena rt_global_arena = {NULL, NULL};
static Obj* rt_all_objects = NULL;
static int rt_cli_argc = 0;
static char** rt_cli_argv = NULL;

static inline void rt_init(int argc, char** argv) {
    rt_arena_init(&rt_global_arena);
    rt_all_objects = NULL;
    rt_cli_argc = argc;
    rt_cli_argv = argv;
}

static inline void rt_shutdown(void) {
    rt_arena_free(&rt_global_arena);
    rt_all_objects = NULL;
}

static inline void* rt_alloc_obj(size_t size, ObjType type) {
    Obj* obj = (Obj*)rt_arena_alloc(&rt_global_arena, size);
    obj->type = type;
    obj->marked = false;
    obj->next = rt_all_objects;
    rt_all_objects = obj;
    return obj;
}

static inline RtArray* rt_array_new(int32_t length, int32_t elem_size, bool is_ref) {
    if (length < 0) {
        fprintf(stderr, "moka runtime error: negative array length %d\n", length);
        exit(1);
    }
    size_t data_size = (size_t)length * (size_t)elem_size;
    RtArray* arr = (RtArray*)rt_alloc_obj(
        sizeof(RtArray) + data_size,
        is_ref ? OBJ_REF_ARRAY : OBJ_VAL_ARRAY
    );
    arr->length = length;
    arr->elem_size = elem_size;
    return arr;
}

static inline RtArray* rt_byte_array_from_literal(const char* str, int32_t len) {
    RtArray* arr = rt_array_new(len, (int32_t)sizeof(uint8_t), false);
    if (len > 0 && str) {
        memcpy(arr->data, str, (size_t)len);
    }
    return arr;
}

static inline int32_t rt_bounds_check(RtArray* arr, int32_t index) {
    if (!arr) {
        fprintf(stderr, "moka runtime error: null array dereference\n");
        exit(1);
    }
    if (index < 0 || index >= arr->length) {
        fprintf(stderr, "moka runtime error: array index %d out of bounds (length %d)\n",
                index, arr->length);
        exit(1);
    }
    return index;
}

static inline void* rt_array_elem_ptr(RtArray* arr, int32_t index) {
    rt_bounds_check(arr, index);
    return arr->data + (size_t)index * (size_t)arr->elem_size;
}

static inline void* rt_null_check(void* ptr, const char* context) {
    if (!ptr) {
        fprintf(stderr, "moka runtime error: null pointer dereference (%s)\n",
                context ? context : "unknown");
        exit(1);
    }
    return ptr;
}

/* =========================================================================
 * Minimal Native I/O & System Helpers
 * ========================================================================= */

static inline void rt_print_int(int32_t val) {
    printf("%d\n", val);
}

static inline void rt_print_bool(bool val) {
    printf("%s\n", val ? "true" : "false");
}

static inline void rt_print_bytes(RtArray* bytes) {
    if (!bytes) {
        return;
    }
    if (bytes->length > 0) {
        fwrite(bytes->data, 1, (size_t)bytes->length, stdout);
    }
}

static inline void rt_println_bytes(RtArray* bytes) {
    rt_print_bytes(bytes);
    putchar('\n');
}

static inline void rt_eprint_bytes(RtArray* bytes) {
    if (!bytes) {
        return;
    }
    if (bytes->length > 0) {
        fwrite(bytes->data, 1, (size_t)bytes->length, stderr);
    }
}

static inline int32_t rt_argc(void) {
    return (int32_t)rt_cli_argc;
}

static inline RtArray* rt_argv_bytes(int32_t index) {
    if (index < 0 || index >= rt_cli_argc || !rt_cli_argv) {
        return rt_array_new(0, (int32_t)sizeof(uint8_t), false);
    }
    const char* s = rt_cli_argv[index];
    int32_t len = (int32_t)strlen(s);
    return rt_byte_array_from_literal(s, len);
}

static inline char* rt_bytes_to_cstr(RtArray* bytes) {
    if (!bytes) {
        fprintf(stderr, "moka runtime error: null path bytes\n");
        exit(1);
    }
    char* cstr = (char*)malloc((size_t)bytes->length + 1);
    if (!cstr) {
        fprintf(stderr, "moka runtime error: out of memory\n");
        exit(1);
    }
    if (bytes->length > 0) {
        memcpy(cstr, bytes->data, (size_t)bytes->length);
    }
    cstr[bytes->length] = '\0';
    return cstr;
}

static inline RtArray* rt_read_file_bytes(RtArray* path_bytes) {
    char* path = rt_bytes_to_cstr(path_bytes);
    FILE* f = fopen(path, "rb");
    if (!f) {
        free(path);
        return NULL;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        free(path);
        return NULL;
    }
    long size = ftell(f);
    if (size < 0) {
        fclose(f);
        free(path);
        return NULL;
    }
    rewind(f);

    RtArray* arr = rt_array_new((int32_t)size, (int32_t)sizeof(uint8_t), false);
    if (size > 0) {
        size_t nread = fread(arr->data, 1, (size_t)size, f);
        if ((long)nread != size) {
            fclose(f);
            free(path);
            return NULL;
        }
    }
    fclose(f);
    free(path);
    return arr;
}

static inline bool rt_write_file_bytes(RtArray* path_bytes, RtArray* data_bytes) {
    if (!data_bytes) {
        return false;
    }
    char* path = rt_bytes_to_cstr(path_bytes);
    FILE* f = fopen(path, "wb");
    if (!f) {
        free(path);
        return false;
    }
    bool ok = true;
    if (data_bytes->length > 0) {
        size_t written = fwrite(data_bytes->data, 1, (size_t)data_bytes->length, f);
        ok = ((int32_t)written == data_bytes->length);
    }
    fclose(f);
    free(path);
    return ok;
}

static inline void rt_exit(int32_t code) {
    exit(code);
}

static inline int32_t rt_system(RtArray* cmd_bytes) {
    char* cmd = rt_bytes_to_cstr(cmd_bytes);
    int rc = system(cmd);
    free(cmd);
    return (int32_t)rc;
}

static inline bool rt_remove_file(RtArray* path_bytes) {
    char* path = rt_bytes_to_cstr(path_bytes);
    int rc = remove(path);
    free(path);
    return rc == 0;
}

#endif /* MOKA_RUNTIME_H */
