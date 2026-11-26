#ifndef MOKA_RUNTIME_H
#define MOKA_RUNTIME_H

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <setjmp.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* =========================================================================
 * Mark-and-Sweep Garbage Collector with Shadow Stack
 * ========================================================================= */

typedef enum
{
    OBJ_INSTANCE = 0,
    OBJ_VAL_ARRAY = 1,  /* Primitive elements (int, byte, boolean) */
    OBJ_REF_ARRAY = 2,  /* Reference elements (Obj*) */
    OBJ_IFACE_ARRAY = 3 /* Interface fat-pointer elements { void* obj; void* vtable; } */
} ObjType;

typedef struct RtTypeInfo
{
    const char *name;
    int32_t ptr_count;
    const size_t *ptr_offsets;
    const void *vtable;
    const struct RtTypeInfo *super;
    int32_t iface_count;
    const struct RtTypeInfo *const *interfaces;
    const void *const *iface_vtables;
} RtTypeInfo;

typedef struct Obj
{
    ObjType type;
    bool marked;
    const RtTypeInfo *type_info;
    struct Obj *next;
} Obj;

typedef struct RtArray
{
    Obj header;
    int32_t length;
    int32_t elem_size;
    uint8_t data[];
} RtArray;

typedef struct RtShadowFrame
{
    struct RtShadowFrame *prev;
    int32_t count;
    void **slots;
} RtShadowFrame;

typedef struct RtExceptionFrame
{
    jmp_buf env;
    struct RtExceptionFrame *prev;
    RtShadowFrame *saved_shadow_stack;
    void *exception;
} RtExceptionFrame;

typedef struct RtThreadNode
{
    RtShadowFrame *volatile *shadow_stack_ptr;
    RtExceptionFrame *volatile *exc_top_ptr;
    volatile int32_t in_native_wait;
    struct RtThreadNode *next;
} RtThreadNode;

/* Global & thread-local GC & runtime state */
static Obj *rt_all_objects = NULL;
static _Thread_local RtShadowFrame *rt_shadow_stack = NULL;
static _Thread_local RtExceptionFrame *rt_exc_top = NULL;
static _Thread_local RtThreadNode *rt_current_thread_node = NULL;
static RtThreadNode *rt_thread_list = NULL;
static pthread_mutex_t rt_gc_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t rt_stw_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t rt_stw_cond = PTHREAD_COND_INITIALIZER;
static atomic_int rt_stw_requested = 0;
static int rt_stw_paused_count = 0;

static atomic_size_t rt_num_objects = 0;
static size_t rt_gc_threshold = 1024;
static size_t rt_total_collections = 0;
static size_t rt_total_freed = 0;
static int rt_cli_argc = 0;
static char **rt_cli_argv = NULL;
static void **rt_static_roots = NULL;
static size_t rt_static_root_count = 0;
static size_t rt_static_root_cap = 0;

static inline void rt_thread_register(RtThreadNode *node)
{
    node->shadow_stack_ptr = (RtShadowFrame *volatile *)&rt_shadow_stack;
    node->exc_top_ptr = (RtExceptionFrame *volatile *)&rt_exc_top;
    node->in_native_wait = 0;
    pthread_mutex_lock(&rt_gc_mutex);
    node->next = rt_thread_list;
    rt_thread_list = node;
    pthread_mutex_unlock(&rt_gc_mutex);
    rt_current_thread_node = node;
}

static inline void rt_thread_unregister(RtThreadNode *node)
{
    pthread_mutex_lock(&rt_stw_mutex);
    pthread_mutex_lock(&rt_gc_mutex);
    RtThreadNode **cur = &rt_thread_list;
    while (*cur != NULL)
    {
        if (*cur == node)
        {
            *cur = node->next;
            break;
        }
        cur = &(*cur)->next;
    }
    pthread_mutex_unlock(&rt_gc_mutex);
    rt_current_thread_node = NULL;
    pthread_cond_broadcast(&rt_stw_cond);
    pthread_mutex_unlock(&rt_stw_mutex);
}

static inline void rt_gc_enter_blocking(void)
{
    if (!rt_current_thread_node)
        return;
    pthread_mutex_lock(&rt_stw_mutex);
    rt_current_thread_node->in_native_wait = 1;
    pthread_cond_broadcast(&rt_stw_cond);
    pthread_mutex_unlock(&rt_stw_mutex);
}

static inline void rt_gc_leave_blocking(void)
{
    if (!rt_current_thread_node)
        return;
    pthread_mutex_lock(&rt_stw_mutex);
    while (atomic_load(&rt_stw_requested))
    {
        pthread_cond_wait(&rt_stw_cond, &rt_stw_mutex);
    }
    rt_current_thread_node->in_native_wait = 0;
    pthread_mutex_unlock(&rt_stw_mutex);
}

static inline void rt_init(int argc, char **argv)
{
    static RtThreadNode main_node;
    rt_all_objects = NULL;
    rt_shadow_stack = NULL;
    rt_exc_top = NULL;
    atomic_store(&rt_num_objects, 0);
    rt_gc_threshold = 1024;
    rt_total_collections = 0;
    rt_total_freed = 0;
    rt_cli_argc = argc;
    rt_cli_argv = argv;
    rt_static_roots = NULL;
    rt_static_root_count = 0;
    rt_static_root_cap = 0;
    rt_thread_list = NULL;
    rt_thread_register(&main_node);
}

static inline void rt_gc_add_static_root(void *slot)
{
    if (rt_static_root_count >= rt_static_root_cap)
    {
        size_t new_cap = rt_static_root_cap == 0 ? 16 : rt_static_root_cap * 2;
        void **nr = (void **)realloc(rt_static_roots, new_cap * sizeof(void *));
        if (!nr)
        {
            fprintf(stderr, "moka runtime error: out of memory\n");
            exit(1);
        }
        rt_static_roots = nr;
        rt_static_root_cap = new_cap;
    }
    rt_static_roots[rt_static_root_count++] = slot;
}

static inline void rt_gc_mark_obj(Obj *obj)
{
    if (!obj || obj->marked)
    {
        return;
    }
    obj->marked = true;

    if (obj->type == OBJ_INSTANCE && obj->type_info)
    {
        for (int32_t i = 0; i < obj->type_info->ptr_count; i++)
        {
            size_t off = obj->type_info->ptr_offsets[i];
            Obj *child = NULL;
            memcpy(&child, (const uint8_t *)obj + off, sizeof(Obj *));
            rt_gc_mark_obj(child);
        }
    }
    else if (obj->type == OBJ_REF_ARRAY)
    {
        RtArray *arr = (RtArray *)obj;
        for (int32_t i = 0; i < arr->length; i++)
        {
            Obj *child = NULL;
            memcpy(&child, arr->data + (size_t)i * sizeof(Obj *), sizeof(Obj *));
            rt_gc_mark_obj(child);
        }
    }
    else if (obj->type == OBJ_IFACE_ARRAY)
    {
        RtArray *arr = (RtArray *)obj;
        for (int32_t i = 0; i < arr->length; i++)
        {
            Obj *child = NULL;
            memcpy(&child, arr->data + (size_t)i * (size_t)arr->elem_size, sizeof(Obj *));
            rt_gc_mark_obj(child);
        }
    }
}

static inline void rt_gc_mark_roots(void)
{
    for (RtThreadNode *tn = rt_thread_list; tn != NULL; tn = tn->next)
    {
        for (RtShadowFrame *f = *tn->shadow_stack_ptr; f != NULL; f = f->prev)
        {
            for (int32_t i = 0; i < f->count; i++)
            {
                if (f->slots[i] != NULL)
                {
                    Obj *obj = NULL;
                    memcpy(&obj, f->slots[i], sizeof(Obj *));
                    rt_gc_mark_obj(obj);
                }
            }
        }
        for (RtExceptionFrame *ef = *tn->exc_top_ptr; ef != NULL; ef = ef->prev)
        {
            if (ef->exception != NULL)
            {
                rt_gc_mark_obj((Obj *)ef->exception);
            }
        }
    }
    for (size_t i = 0; i < rt_static_root_count; i++)
    {
        if (rt_static_roots[i] != NULL)
        {
            Obj *obj = NULL;
            memcpy(&obj, rt_static_roots[i], sizeof(Obj *));
            rt_gc_mark_obj(obj);
        }
    }
}

static inline void rt_gc_sweep(void)
{
    Obj **cur = &rt_all_objects;
    while (*cur != NULL)
    {
        Obj *obj = *cur;
        if (!obj->marked)
        {
            *cur = obj->next;
            free(obj);
            atomic_fetch_sub(&rt_num_objects, 1);
            rt_total_freed++;
        }
        else
        {
            obj->marked = false;
            cur = &obj->next;
        }
    }
}

static inline void rt_gc_safepoint_park_locked(void)
{
    rt_stw_paused_count++;
    pthread_cond_broadcast(&rt_stw_cond);
    while (atomic_load(&rt_stw_requested))
    {
        pthread_cond_wait(&rt_stw_cond, &rt_stw_mutex);
    }
    rt_stw_paused_count--;
}

static inline void rt_gc_collect(void)
{
    pthread_mutex_lock(&rt_stw_mutex);
    if (atomic_load(&rt_stw_requested))
    {
        rt_gc_safepoint_park_locked();
        pthread_mutex_unlock(&rt_stw_mutex);
        return;
    }
    atomic_store(&rt_stw_requested, 1);
    while (1)
    {
        int other_running = 0;
        pthread_mutex_lock(&rt_gc_mutex);
        for (RtThreadNode *tn = rt_thread_list; tn != NULL; tn = tn->next)
        {
            if (tn != rt_current_thread_node && !tn->in_native_wait)
            {
                other_running++;
            }
        }
        pthread_mutex_unlock(&rt_gc_mutex);
        if (rt_stw_paused_count >= other_running)
        {
            break;
        }
        pthread_cond_wait(&rt_stw_cond, &rt_stw_mutex);
    }

    pthread_mutex_lock(&rt_gc_mutex);
    rt_gc_mark_roots();
    rt_gc_sweep();
    rt_total_collections++;
    size_t live = atomic_load(&rt_num_objects);
    rt_gc_threshold = live * 2;
    if (rt_gc_threshold < 256)
    {
        rt_gc_threshold = 256;
    }
    pthread_mutex_unlock(&rt_gc_mutex);

    atomic_store(&rt_stw_requested, 0);
    pthread_cond_broadcast(&rt_stw_cond);
    pthread_mutex_unlock(&rt_stw_mutex);
}

static inline void rt_gc_maybe_collect(void)
{
    if (atomic_load(&rt_stw_requested))
    {
        pthread_mutex_lock(&rt_stw_mutex);
        if (atomic_load(&rt_stw_requested))
        {
            rt_gc_safepoint_park_locked();
        }
        pthread_mutex_unlock(&rt_stw_mutex);
    }
    if (atomic_load(&rt_num_objects) >= rt_gc_threshold)
    {
        rt_gc_collect();
    }
}

static inline int32_t rt_gc_live_objects(void)
{
    return (int32_t)atomic_load(&rt_num_objects);
}

static inline int32_t rt_gc_collections(void)
{
    return (int32_t)rt_total_collections;
}

static inline int32_t rt_gc_freed_objects(void)
{
    return (int32_t)rt_total_freed;
}

static inline void rt_shutdown(void)
{
    Obj *obj = rt_all_objects;
    while (obj)
    {
        Obj *next = obj->next;
        free(obj);
        obj = next;
    }
    rt_all_objects = NULL;
    rt_shadow_stack = NULL;
    rt_num_objects = 0;
    free(rt_static_roots);
    rt_static_roots = NULL;
    rt_static_root_count = 0;
    rt_static_root_cap = 0;
}

static inline void *rt_alloc_instance(size_t size, const RtTypeInfo *type_info)
{
    Obj *obj = (Obj *)calloc(1, size);
    if (!obj)
    {
        fprintf(stderr, "moka runtime error: out of memory allocating %zu bytes\n", size);
        exit(1);
    }
    obj->type = OBJ_INSTANCE;
    obj->marked = false;
    obj->type_info = type_info;
    pthread_mutex_lock(&rt_gc_mutex);
    obj->next = rt_all_objects;
    rt_all_objects = obj;
    atomic_fetch_add(&rt_num_objects, 1);
    pthread_mutex_unlock(&rt_gc_mutex);
    return obj;
}

static inline void *rt_alloc_obj(size_t size, ObjType type)
{
    Obj *obj = (Obj *)calloc(1, size);
    if (!obj)
    {
        fprintf(stderr, "moka runtime error: out of memory allocating %zu bytes\n", size);
        exit(1);
    }
    obj->type = type;
    obj->marked = false;
    obj->type_info = NULL;
    pthread_mutex_lock(&rt_gc_mutex);
    obj->next = rt_all_objects;
    rt_all_objects = obj;
    atomic_fetch_add(&rt_num_objects, 1);
    pthread_mutex_unlock(&rt_gc_mutex);
    return obj;
}

static inline RtArray *rt_array_new(int32_t length, int32_t elem_size, bool is_ref)
{
    if (length < 0)
    {
        fprintf(stderr, "moka runtime error: negative array length %d\n", length);
        exit(1);
    }
    size_t data_size = (size_t)length * (size_t)elem_size;
    RtArray *arr = (RtArray *)rt_alloc_obj(
        sizeof(RtArray) + data_size,
        is_ref ? OBJ_REF_ARRAY : OBJ_VAL_ARRAY);
    arr->length = length;
    arr->elem_size = elem_size;
    return arr;
}

static inline RtArray *rt_array_new_iface(int32_t length, int32_t elem_size)
{
    if (length < 0)
    {
        fprintf(stderr, "moka runtime error: negative array length %d\n", length);
        exit(1);
    }
    size_t data_size = (size_t)length * (size_t)elem_size;
    RtArray *arr = (RtArray *)rt_alloc_obj(
        sizeof(RtArray) + data_size,
        OBJ_IFACE_ARRAY);
    arr->length = length;
    arr->elem_size = elem_size;
    return arr;
}

static inline RtArray *rt_byte_array_from_literal(const char *str, int32_t len)
{
    RtArray *arr = rt_array_new(len, (int32_t)sizeof(uint8_t), false);
    if (len > 0 && str)
    {
        memcpy(arr->data, str, (size_t)len);
    }
    return arr;
}

static inline int32_t rt_bounds_check(RtArray *arr, int32_t index)
{
    if (!arr)
    {
        fprintf(stderr, "moka runtime error: null array dereference\n");
        exit(1);
    }
    if (index < 0 || index >= arr->length)
    {
        fprintf(stderr, "moka runtime error: array index %d out of bounds (length %d)\n",
                index, arr->length);
        exit(1);
    }
    return index;
}

static inline void *rt_array_elem_ptr(RtArray *arr, int32_t index)
{
    rt_bounds_check(arr, index);
    return arr->data + (size_t)index * (size_t)arr->elem_size;
}

static inline void *rt_null_check(void *ptr, const char *context)
{
    if (!ptr)
    {
        fprintf(stderr, "moka runtime error: null pointer dereference (%s)\n",
                context ? context : "unknown");
        exit(1);
    }
    return ptr;
}

static inline bool rt_is_instance(const void *ptr, const RtTypeInfo *target)
{
    if (!ptr || !target)
    {
        return false;
    }
    const Obj *obj = (const Obj *)ptr;
    const RtTypeInfo *cur = obj->type_info;
    while (cur != NULL)
    {
        if (cur == target)
        {
            return true;
        }
        for (int32_t i = 0; i < cur->iface_count; i++)
        {
            if (cur->interfaces[i] == target)
            {
                return true;
            }
        }
        cur = cur->super;
    }
    return false;
}

static inline void *rt_cast_class(void *ptr, const RtTypeInfo *target)
{
    if (!ptr)
    {
        return NULL;
    }
    if (!rt_is_instance(ptr, target))
    {
        const Obj *obj = (const Obj *)ptr;
        const char *actual = (obj->type_info && obj->type_info->name) ? obj->type_info->name : "unknown";
        const char *expected = (target && target->name) ? target->name : "unknown";
        fprintf(stderr, "moka runtime error: ClassCastException: cannot cast '%s' to '%s'\n",
                actual, expected);
        exit(1);
    }
    return ptr;
}

static inline const void *rt_cast_iface_vtable(void *ptr, const RtTypeInfo *target)
{
    if (!ptr)
    {
        return NULL;
    }
    const Obj *obj = (const Obj *)ptr;
    const RtTypeInfo *cur = obj->type_info;
    while (cur != NULL)
    {
        for (int32_t i = 0; i < cur->iface_count; i++)
        {
            if (cur->interfaces[i] == target)
            {
                return cur->iface_vtables[i];
            }
        }
        cur = cur->super;
    }
    const char *actual = (obj->type_info && obj->type_info->name) ? obj->type_info->name : "unknown";
    const char *expected = (target && target->name) ? target->name : "unknown";
    fprintf(stderr, "moka runtime error: ClassCastException: cannot cast '%s' to '%s'\n",
            actual, expected);
    exit(1);
}

/* =========================================================================
 * Minimal Native I/O & System Helpers
 * ========================================================================= */

static inline void rt_print_int(int32_t val)
{
    printf("%d\n", val);
}

static inline void rt_print_bool(bool val)
{
    printf("%s\n", val ? "true" : "false");
}

static inline void rt_print_bytes(RtArray *bytes)
{
    if (!bytes)
    {
        return;
    }
    if (bytes->length > 0)
    {
        fwrite(bytes->data, 1, (size_t)bytes->length, stdout);
    }
}

static inline void rt_println_bytes(RtArray *bytes)
{
    rt_print_bytes(bytes);
    putchar('\n');
}

static inline void rt_eprint_bytes(RtArray *bytes)
{
    if (!bytes)
    {
        return;
    }
    if (bytes->length > 0)
    {
        fwrite(bytes->data, 1, (size_t)bytes->length, stderr);
    }
}

static inline RtArray *rt_read_line_bytes(void)
{
    size_t cap = 64;
    size_t len = 0;
    char *buf = (char *)malloc(cap);
    if (!buf)
    {
        fprintf(stderr, "moka runtime error: out of memory\n");
        exit(1);
    }
    int c;
    while ((c = fgetc(stdin)) != EOF)
    {
        if (c == '\n')
        {
            break;
        }
        if (len + 1 >= cap)
        {
            cap *= 2;
            char *nb = (char *)realloc(buf, cap);
            if (!nb)
            {
                free(buf);
                fprintf(stderr, "moka runtime error: out of memory\n");
                exit(1);
            }
            buf = nb;
        }
        buf[len++] = (char)c;
    }
    if (len > 0 && buf[len - 1] == '\r')
    {
        len--;
    }
    RtArray *arr = rt_byte_array_from_literal(buf, (int32_t)len);
    free(buf);
    return arr;
}

static inline int32_t rt_argc(void)
{
    return (int32_t)rt_cli_argc;
}

static inline RtArray *rt_argv_bytes(int32_t index)
{
    if (index < 0 || index >= rt_cli_argc || !rt_cli_argv)
    {
        return rt_array_new(0, (int32_t)sizeof(uint8_t), false);
    }
    const char *s = rt_cli_argv[index];
    int32_t len = (int32_t)strlen(s);
    return rt_byte_array_from_literal(s, len);
}

static inline char *rt_bytes_to_cstr(RtArray *bytes)
{
    if (!bytes)
    {
        fprintf(stderr, "moka runtime error: null path bytes\n");
        exit(1);
    }
    char *cstr = (char *)malloc((size_t)bytes->length + 1);
    if (!cstr)
    {
        fprintf(stderr, "moka runtime error: out of memory\n");
        exit(1);
    }
    if (bytes->length > 0)
    {
        memcpy(cstr, bytes->data, (size_t)bytes->length);
    }
    cstr[bytes->length] = '\0';
    return cstr;
}

static inline RtArray *rt_read_file_bytes(RtArray *path_bytes)
{
    char *path = rt_bytes_to_cstr(path_bytes);
    FILE *f = fopen(path, "rb");
    if (!f)
    {
        free(path);
        return NULL;
    }
    if (fseek(f, 0, SEEK_END) != 0)
    {
        fclose(f);
        free(path);
        return NULL;
    }
    long size = ftell(f);
    if (size < 0)
    {
        fclose(f);
        free(path);
        return NULL;
    }
    rewind(f);

    RtArray *arr = rt_array_new((int32_t)size, (int32_t)sizeof(uint8_t), false);
    if (size > 0)
    {
        size_t nread = fread(arr->data, 1, (size_t)size, f);
        if ((long)nread != size)
        {
            fclose(f);
            free(path);
            return NULL;
        }
    }
    fclose(f);
    free(path);
    return arr;
}

static inline bool rt_write_file_bytes(RtArray *path_bytes, RtArray *data_bytes)
{
    if (!data_bytes)
    {
        return false;
    }
    char *path = rt_bytes_to_cstr(path_bytes);
    FILE *f = fopen(path, "wb");
    if (!f)
    {
        free(path);
        return false;
    }
    bool ok = true;
    if (data_bytes->length > 0)
    {
        size_t written = fwrite(data_bytes->data, 1, (size_t)data_bytes->length, f);
        ok = ((int32_t)written == data_bytes->length);
    }
    fclose(f);
    free(path);
    return ok;
}

static inline void rt_exit(int32_t code)
{
    exit(code);
}

static inline int32_t rt_system(RtArray *cmd_bytes)
{
    char *cmd = rt_bytes_to_cstr(cmd_bytes);
    int rc = system(cmd);
    free(cmd);
    return (int32_t)rc;
}

static inline bool rt_remove_file(RtArray *path_bytes)
{
    char *path = rt_bytes_to_cstr(path_bytes);
    int rc = remove(path);
    free(path);
    return rc == 0;
}

static inline RtArray *rt_long_to_bytes(int64_t v)
{
    char buf[32];
    int n = snprintf(buf, sizeof(buf), "%lld", (long long)v);
    if (n < 0)
        n = 0;
    return rt_byte_array_from_literal(buf, (int32_t)n);
}

static inline RtArray *rt_double_to_bytes(double v)
{
    char buf[64];
    int n = snprintf(buf, sizeof(buf), "%.15g", v);
    if (n < 0)
        n = 0;
    return rt_byte_array_from_literal(buf, (int32_t)n);
}

static inline RtArray *rt_float_to_bytes(float v)
{
    char buf[64];
    int n = snprintf(buf, sizeof(buf), "%.7g", (double)v);
    if (n < 0)
        n = 0;
    return rt_byte_array_from_literal(buf, (int32_t)n);
}

static inline RtArray *rt_char_to_bytes(uint16_t c)
{
    uint8_t b = (uint8_t)(c & 0xFFu);
    return rt_byte_array_from_literal((const char *)&b, 1);
}

static inline void rt_throw(void *exc)
{
    if (!exc)
    {
        fprintf(stderr, "moka runtime error: cannot throw null exception\n");
        exit(1);
    }
    if (!rt_exc_top)
    {
        Obj *obj = (Obj *)exc;
        const char *tname = (obj->type_info && obj->type_info->name) ? obj->type_info->name : "Exception";
        fprintf(stderr, "moka uncaught exception: %s\n", tname);
        exit(1);
    }
    rt_exc_top->exception = exc;
    rt_shadow_stack = rt_exc_top->saved_shadow_stack;
    longjmp(rt_exc_top->env, 1);
}

static inline int32_t rt_parse_int(RtArray *bytes, bool *ok)
{
    *ok = false;
    if (!bytes || bytes->length == 0)
    {
        return 0;
    }
    char *s = rt_bytes_to_cstr(bytes);
    if (s[0] == '\0' || s[0] == ' ' || s[0] == '\t' || s[0] == '\n' || s[0] == '\r')
    {
        free(s);
        return 0;
    }
    errno = 0;
    char *end = NULL;
    long val = strtol(s, &end, 10);
    if (errno == ERANGE || end == s || *end != '\0' || val < INT32_MIN || val > INT32_MAX)
    {
        free(s);
        return 0;
    }
    free(s);
    *ok = true;
    return (int32_t)val;
}

static inline int64_t rt_parse_long(RtArray *bytes, bool *ok)
{
    *ok = false;
    if (!bytes || bytes->length == 0)
    {
        return 0;
    }
    char *s = rt_bytes_to_cstr(bytes);
    if (s[0] == '\0' || s[0] == ' ' || s[0] == '\t' || s[0] == '\n' || s[0] == '\r')
    {
        free(s);
        return 0;
    }
    errno = 0;
    char *end = NULL;
    long long val = strtoll(s, &end, 10);
    if (errno == ERANGE || end == s || *end != '\0')
    {
        free(s);
        return 0;
    }
    free(s);
    *ok = true;
    return (int64_t)val;
}

static inline double rt_parse_double(RtArray *bytes, bool *ok)
{
    *ok = false;
    if (!bytes || bytes->length == 0)
    {
        return 0.0;
    }
    char *s = rt_bytes_to_cstr(bytes);
    if (s[0] == '\0' || s[0] == ' ' || s[0] == '\t' || s[0] == '\n' || s[0] == '\r')
    {
        free(s);
        return 0.0;
    }
    errno = 0;
    char *end = NULL;
    double val = strtod(s, &end);
    if (errno == ERANGE || end == s || *end != '\0')
    {
        free(s);
        return 0.0;
    }
    free(s);
    *ok = true;
    return val;
}

static inline bool rt_is_valid_double(RtArray *bytes)
{
    bool ok = false;
    (void)rt_parse_double(bytes, &ok);
    return ok;
}

static inline double rt_parse_double_val(RtArray *bytes)
{
    bool ok = false;
    return rt_parse_double(bytes, &ok);
}

static inline float rt_parse_float(RtArray *bytes, bool *ok)
{
    *ok = false;
    if (!bytes || bytes->length == 0)
    {
        return 0.0f;
    }
    char *s = rt_bytes_to_cstr(bytes);
    if (s[0] == '\0' || s[0] == ' ' || s[0] == '\t' || s[0] == '\n' || s[0] == '\r')
    {
        free(s);
        return 0.0f;
    }
    errno = 0;
    char *end = NULL;
    float val = strtof(s, &end);
    if (errno == ERANGE || end == s || *end != '\0')
    {
        free(s);
        return 0.0f;
    }
    free(s);
    *ok = true;
    return val;
}

static inline bool rt_is_valid_float(RtArray *bytes)
{
    bool ok = false;
    (void)rt_parse_float(bytes, &ok);
    return ok;
}

static inline float rt_parse_float_val(RtArray *bytes)
{
    bool ok = false;
    return rt_parse_float(bytes, &ok);
}

/* =========================================================================
 * Concurrency: Threads, Mutexes & Object Monitors
 * ========================================================================= */

typedef struct RtThreadLaunch
{
    void *obj;
    void (*run_fn)(void *);
} RtThreadLaunch;

static inline void *rt_thread_entry(void *arg)
{
    RtThreadLaunch *launch = (RtThreadLaunch *)arg;
    void *obj = launch->obj;
    void (*run_fn)(void *) = launch->run_fn;
    free(launch);

    RtThreadNode node;
    rt_thread_register(&node);

    void *root_slot = obj;
    void *slots[1] = {&root_slot};
    RtShadowFrame sf = {.prev = NULL, .count = 1, .slots = slots};
    rt_shadow_stack = &sf;

    run_fn(root_slot);

    rt_shadow_stack = NULL;
    rt_thread_unregister(&node);
    return NULL;
}

static inline int64_t rt_thread_spawn_raw(void *obj, void (*run_fn)(void *))
{
    RtThreadLaunch *launch = (RtThreadLaunch *)malloc(sizeof(RtThreadLaunch));
    if (!launch)
    {
        fprintf(stderr, "moka runtime error: out of memory spawning thread\n");
        exit(1);
    }
    launch->obj = obj;
    launch->run_fn = run_fn;
    pthread_t *th = (pthread_t *)malloc(sizeof(pthread_t));
    if (!th)
    {
        free(launch);
        fprintf(stderr, "moka runtime error: out of memory allocating pthread_t\n");
        exit(1);
    }
    if (pthread_create(th, NULL, rt_thread_entry, launch) != 0)
    {
        free(launch);
        free(th);
        fprintf(stderr, "moka runtime error: pthread_create failed\n");
        exit(1);
    }
    return (int64_t)(intptr_t)th;
}

static inline void rt_thread_join(int64_t h)
{
    if (h == 0)
        return;
    pthread_t *th = (pthread_t *)(intptr_t)h;
    rt_gc_enter_blocking();
    pthread_join(*th, NULL);
    rt_gc_leave_blocking();
    free(th);
}

static inline void rt_thread_sleep(int32_t ms)
{
    if (ms <= 0)
        return;
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    rt_gc_enter_blocking();
    nanosleep(&ts, NULL);
    rt_gc_leave_blocking();
}

static inline int64_t rt_mutex_create(void)
{
    pthread_mutex_t *m = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
    if (!m)
    {
        fprintf(stderr, "moka runtime error: out of memory creating mutex\n");
        exit(1);
    }
    pthread_mutex_init(m, NULL);
    return (int64_t)(intptr_t)m;
}

static inline void rt_mutex_lock(int64_t h)
{
    if (h == 0)
        return;
    pthread_mutex_t *m = (pthread_mutex_t *)(intptr_t)h;
    rt_gc_enter_blocking();
    pthread_mutex_lock(m);
    rt_gc_leave_blocking();
}

static inline void rt_mutex_unlock(int64_t h)
{
    if (h == 0)
        return;
    pthread_mutex_t *m = (pthread_mutex_t *)(intptr_t)h;
    pthread_mutex_unlock(m);
}

typedef struct RtMonitor
{
    void *obj;
    pthread_mutex_t mtx;
    pthread_t owner;
    int32_t depth;
    struct RtMonitor *next;
} RtMonitor;

static RtMonitor *rt_monitor_list = NULL;
static pthread_mutex_t rt_monitor_table_mtx = PTHREAD_MUTEX_INITIALIZER;

static inline RtMonitor *rt_get_monitor(void *obj)
{
    pthread_mutex_lock(&rt_monitor_table_mtx);
    RtMonitor *cur = rt_monitor_list;
    while (cur != NULL)
    {
        if (cur->obj == obj)
        {
            pthread_mutex_unlock(&rt_monitor_table_mtx);
            return cur;
        }
        cur = cur->next;
    }
    RtMonitor *mon = (RtMonitor *)calloc(1, sizeof(RtMonitor));
    if (!mon)
    {
        pthread_mutex_unlock(&rt_monitor_table_mtx);
        fprintf(stderr, "moka runtime error: out of memory allocating monitor\n");
        exit(1);
    }
    mon->obj = obj;
    pthread_mutex_init(&mon->mtx, NULL);
    mon->depth = 0;
    mon->next = rt_monitor_list;
    rt_monitor_list = mon;
    pthread_mutex_unlock(&rt_monitor_table_mtx);
    return mon;
}

static inline void rt_monitor_enter(void *obj)
{
    if (!obj)
    {
        fprintf(stderr, "moka runtime error: synchronized on null\n");
        exit(1);
    }
    RtMonitor *mon = rt_get_monitor(obj);
    pthread_t self = pthread_self();
    if (mon->depth > 0 && pthread_equal(mon->owner, self))
    {
        mon->depth++;
        return;
    }
    rt_gc_enter_blocking();
    pthread_mutex_lock(&mon->mtx);
    rt_gc_leave_blocking();
    mon->owner = self;
    mon->depth = 1;
}

static inline void rt_monitor_exit(void *obj)
{
    if (!obj)
        return;
    RtMonitor *mon = rt_get_monitor(obj);
    mon->depth--;
    if (mon->depth == 0)
    {
        pthread_mutex_unlock(&mon->mtx);
    }
}

/* =========================================================================
 * Math helpers
 * ========================================================================= */

static inline double rt_math_sqrt(double x) { return sqrt(x); }
static inline double rt_math_pow(double a, double b) { return pow(a, b); }
static inline double rt_math_sin(double x) { return sin(x); }
static inline double rt_math_cos(double x) { return cos(x); }
static inline double rt_math_tan(double x) { return tan(x); }
static inline double rt_math_log(double x) { return log(x); }
static inline double rt_math_exp(double x) { return exp(x); }
static inline double rt_math_floor(double x) { return floor(x); }
static inline double rt_math_ceil(double x) { return ceil(x); }
static inline double rt_math_round(double x) { return round(x); }
static inline double rt_math_fabs(double x) { return fabs(x); }

#endif /* MOKA_RUNTIME_H */
