#include <stdlib.h>
#include <assert.h>

#include "symbol.h"
#include "object.h"


// TODO: Flywheelize this.
hvm_obj_string *hvm_new_obj_string() {
  hvm_obj_string *str = malloc(sizeof(hvm_obj_string));
  str->data = NULL;
  return str;
}

hvm_obj_array *hvm_new_obj_array() {
  hvm_obj_array *arr = malloc(sizeof(hvm_obj_array));
  arr->data = malloc(sizeof(hvm_obj_ref*) * 1);
  arr->data[0] = NULL;
  arr->length = 0;
  return arr;
}

hvm_obj_ref *hvm_new_obj_int() {
  static int64_t zero = 0;
  hvm_obj_ref *ref = hvm_new_obj_ref();
  ref->type = HVM_INTEGER;
  ref->data.i64 = zero;
  return ref;
}

hvm_obj_ref *hvm_obj_int_add(hvm_obj_ref *a, hvm_obj_ref *b) {
  // Type-checks
  assert(a->type == HVM_INTEGER);
  assert(b->type == HVM_INTEGER);
  
  hvm_obj_ref *c = hvm_new_obj_ref();
  int64_t av, bv, cv;
  av = a->data.i64;
  bv = b->data.i64;
  cv = av + bv;
  c->type = HVM_INTEGER;
  c->data.i64 = cv;
  return c;
}

hvm_obj_struct *hvm_new_obj_struct() {
  hvm_obj_struct *strct = malloc(sizeof(hvm_obj_struct));
  strct->heap_length = 0;
  strct->heap_size = HVM_STRUCT_INITIAL_HEAP_SIZE;
  strct->heap = malloc(HVM_STRUCT_HEAP_MEMORY_SIZE(strct->heap_size));
  strct->heap[0] = NULL;
  return strct;
}
void hvm_obj_struct_grow_heap(hvm_obj_struct *strct) {
  strct->heap_size = strct->heap_size * HVM_STRUCT_HEAP_GROWTH_RATE;
  strct->heap = realloc(strct->heap, HVM_STRUCT_HEAP_MEMORY_SIZE(strct->heap_size));
}
void hvm_obj_struct_set(hvm_obj_struct *strct, hvm_symbol_id id, hvm_obj_ref *obj) {
  while(strct->heap_length >= strct->heap_size) {
    hvm_obj_struct_grow_heap(strct);
  }
  // Create the pair
  hvm_obj_struct_heap_pair *pair = malloc(sizeof(hvm_obj_struct_heap_pair));
  pair->id = id;
  pair->obj = obj;
  // Start at the bottom
  unsigned int idx = strct->heap_length;
  // Swap with parent if it's less than its parent
  while(idx > 0 && strct->heap[idx / 2]->id < id) {
    strct->heap[idx] = strct->heap[idx / 2];
    idx = idx / 2;
  }
  strct->heap[idx] = pair;
  strct->heap_length += 1;
}
hvm_obj_ref *hvm_obj_struct_get(hvm_obj_struct *strct, hvm_symbol_id id) {
  // Start at the top
  unsigned int idx = 0;
  hvm_symbol_id i;
  while(idx < strct->heap_length) {
    i = strct->heap[idx]->id;
    if(i == id) {
      return strct->heap[idx]->obj;
    } else if(i < id) {
      idx = 2 * idx;// Left child
    } else {
      idx = (2 * idx) + 1;// Right child
    }
  }
  return NULL;
}

hvm_obj_ref *hvm_new_obj_ref() {
  hvm_obj_ref *ref = malloc(sizeof(hvm_obj_ref));
  ref->type = HVM_NULL;
  ref->data.u64 = 0;
  return ref;
}
void hvm_obj_ref_set_string(hvm_obj_ref *ref, hvm_obj_string *str) {
  ref->type = HVM_STRING;
  ref->data.v = str;
}
