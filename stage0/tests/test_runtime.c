#include "../src/runtime.h"
#include <assert.h>

typedef struct FakeNode {
    Obj header;
    int32_t value;
    struct FakeNode* next;
} FakeNode;

int main(int argc, char** argv) {
    rt_init(argc, argv);

    /* Test 1: Byte array from literal */
    RtArray* s = rt_byte_array_from_literal("hello", 5);
    assert(s->header.type == OBJ_VAL_ARRAY);
    assert(s->header.marked == false);
    assert(s->length == 5);
    assert(s->elem_size == 1);
    assert(memcmp(s->data, "hello", 5) == 0);

    /* Test 2: Reference array & zero-initialization */
    RtArray* refs = rt_array_new(4, (int32_t)sizeof(Obj*), true);
    assert(refs->header.type == OBJ_REF_ARRAY);
    assert(refs->length == 4);
    Obj** slots = (Obj**)refs->data;
    for (int i = 0; i < 4; i++) {
        assert(slots[i] == NULL);
    }

    /* Test 3: Instance allocation & 8-byte alignment */
    FakeNode* n1 = (FakeNode*)rt_alloc_obj(sizeof(FakeNode), OBJ_INSTANCE);
    FakeNode* n2 = (FakeNode*)rt_alloc_obj(sizeof(FakeNode), OBJ_INSTANCE);
    assert(((uintptr_t)n1 & 7u) == 0);
    assert(((uintptr_t)n2 & 7u) == 0);
    assert(n1->value == 0);
    assert(n1->next == NULL);
    assert(rt_all_objects == (Obj*)n2);
    assert(n2->header.next == (Obj*)n1);

    /* Test 4: Bounds check */
    assert(rt_bounds_check(s, 0) == 0);
    assert(rt_bounds_check(s, 4) == 4);

    /* Test 5: CLI args */
    assert(rt_argc() >= 1);
    RtArray* arg0 = rt_argv_bytes(0);
    assert(arg0->length > 0);

    rt_shutdown();
    printf("All runtime tests passed!\n");
    return 0;
}
