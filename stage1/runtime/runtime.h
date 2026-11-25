#ifndef MOKA_RUNTIME_H
#define MOKA_RUNTIME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* =========================================================================
 * Mark-and-Sweep Garbage Collector with Shadow Stack
 * ========================================================================= */

typedef enum {
    OBJ_INSTANCE = 0,
    OBJ_VAL_ARRAY = 1, /* Primitive elements (int, byte, boolean) */
    OBJ_REF_ARRAY = 2  /* Reference elements (Obj*) */
} ObjType;

typedef struct RtTypeInfo {
    const char* name;
    int32_t ptr_count;
    const size_t* ptr_offsets;
    const void* vtable;
} RtTypeInfo;

typedef struct Obj {
    ObjType type;
    bool marked;
    const RtTypeInfo* type_info;
    struct Obj* next;
} Obj;

typedef struct RtArray {
    Obj header;
    int32_t length;
    int32_t elem_size;
    uint8_t data[];
} RtArray;

typedef struct RtShadowFrame {
    struct RtShadowFrame* prev;
    int32_t count;
    void** slots;
} RtShadowFrame;

/* Global GC & runtime state */
static Obj* rt_all_objects = NULL;
static RtShadowFrame* rt_shadow_stack = NULL;
static size_t rt_num_objects = 0;
static size_t rt_gc_threshold = 1024;
static size_t rt_total_collections = 0;
static size_t rt_total_freed = 0;
static int rt_cli_argc = 0;
static char** rt_cli_argv = NULL;

static inline void rt_init(int argc, char** argv) {
    rt_all_objects = NULL;
    rt_shadow_stack = NULL;
    rt_num_objects = 0;
    rt_gc_threshold = 1024;
    rt_total_collections = 0;
    rt_total_freed = 0;
    rt_cli_argc = argc;
    rt_cli_argv = argv;
}

static inline void rt_gc_mark_obj(Obj* obj) {
    if (!obj || obj->marked) {
        return;
    }
    obj->marked = true;

    if (obj->type == OBJ_INSTANCE && obj->type_info) {
        for (int32_t i = 0; i < obj->type_info->ptr_count; i++) {
            size_t off = obj->type_info->ptr_offsets[i];
            Obj* child = NULL;
            memcpy(&child, (const uint8_t*)obj + off, sizeof(Obj*));
            rt_gc_mark_obj(child);
        }
    } else if (obj->type == OBJ_REF_ARRAY) {
        RtArray* arr = (RtArray*)obj;
        for (int32_t i = 0; i < arr->length; i++) {
            Obj* child = NULL;
            memcpy(&child, arr->data + (size_t)i * sizeof(Obj*), sizeof(Obj*));
            rt_gc_mark_obj(child);
        }
    }
}

static inline void rt_gc_mark_roots(void) {
    for (RtShadowFrame* f = rt_shadow_stack; f != NULL; f = f->prev) {
        for (int32_t i = 0; i < f->count; i++) {
            if (f->slots[i] != NULL) {
                Obj* obj = NULL;
                memcpy(&obj, f->slots[i], sizeof(Obj*));
                rt_gc_mark_obj(obj);
            }
        }
    }
}

static inline void rt_gc_sweep(void) {
    Obj** cur = &rt_all_objects;
    while (*cur != NULL) {
        Obj* obj = *cur;
        if (!obj->marked) {
            *cur = obj->next;
            free(obj);
            rt_num_objects--;
            rt_total_freed++;
        } else {
            obj->marked = false;
            cur = &obj->next;
        }
    }
}

static inline void rt_gc_collect(void) {
    rt_gc_mark_roots();
    rt_gc_sweep();
    rt_total_collections++;
    rt_gc_threshold = rt_num_objects * 2;
    if (rt_gc_threshold < 256) {
        rt_gc_threshold = 256;
    }
}

static inline void rt_gc_maybe_collect(void) {
    if (rt_num_objects >= rt_gc_threshold) {
        rt_gc_collect();
    }
}

static inline int32_t rt_gc_live_objects(void) {
    return (int32_t)rt_num_objects;
}

static inline int32_t rt_gc_collections(void) {
    return (int32_t)rt_total_collections;
}

static inline int32_t rt_gc_freed_objects(void) {
    return (int32_t)rt_total_freed;
}

static inline void rt_shutdown(void) {
    Obj* obj = rt_all_objects;
    while (obj) {
        Obj* next = obj->next;
        free(obj);
        obj = next;
    }
    rt_all_objects = NULL;
    rt_shadow_stack = NULL;
    rt_num_objects = 0;
}

static inline void* rt_alloc_instance(size_t size, const RtTypeInfo* type_info) {
    Obj* obj = (Obj*)calloc(1, size);
    if (!obj) {
        fprintf(stderr, "moka runtime error: out of memory allocating %zu bytes\n", size);
        exit(1);
    }
    obj->type = OBJ_INSTANCE;
    obj->marked = false;
    obj->type_info = type_info;
    obj->next = rt_all_objects;
    rt_all_objects = obj;
    rt_num_objects++;
    return obj;
}

static inline void* rt_alloc_obj(size_t size, ObjType type) {
    Obj* obj = (Obj*)calloc(1, size);
    if (!obj) {
        fprintf(stderr, "moka runtime error: out of memory allocating %zu bytes\n", size);
        exit(1);
    }
    obj->type = type;
    obj->marked = false;
    obj->type_info = NULL;
    obj->next = rt_all_objects;
    rt_all_objects = obj;
    rt_num_objects++;
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
