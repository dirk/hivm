#include <stdlib.h>
#include <assert.h>

#include <glib.h>

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
  //arr->data = malloc(sizeof(hvm_obj_ref*) * 1);
  //arr->data[0] = NULL;
  //arr->length = 0;
  arr->array = g_array_new(TRUE, TRUE, sizeof(hvm_obj_ref*));
  return arr;
}
hvm_obj_array *hvm_new_obj_array_with_length(hvm_obj_ref *lenref) {
  hvm_obj_array *arr = malloc(sizeof(hvm_obj_array));
  assert(lenref->type == HVM_INTEGER);
  guint len = (guint)(lenref->data.i64);
  arr->array = g_array_sized_new(TRUE, TRUE, sizeof(hvm_obj_ref*), len);
  return arr;
}

// Push B onto the end of A
void hvm_obj_array_push(hvm_obj_ref *a, hvm_obj_ref *b) {
  assert(a->type == HVM_ARRAY);
  hvm_obj_array *arr = a->data.v;
  g_array_append_val(arr->array, b);
}
void hvm_obj_array_unshift(hvm_obj_ref *a, hvm_obj_ref *b) {
  assert(a->type == HVM_ARRAY);
  hvm_obj_array *arr = a->data.v;
  g_array_prepend_val(arr->array, b);
}

hvm_obj_ref* hvm_obj_array_shift(hvm_obj_ref *a) {
  assert(a->type == HVM_ARRAY);
  hvm_obj_array *arr = a->data.v;
  hvm_obj_ref *ptr = g_array_index(arr->array, hvm_obj_ref*, 0);
  g_array_remove_index(arr->array, 0);
  return ptr;
}
hvm_obj_ref* hvm_obj_array_pop(hvm_obj_ref *a) {
  assert(a->type == HVM_ARRAY);
  hvm_obj_array *arr = a->data.v;
  guint end = arr->array->len - 1;
  hvm_obj_ref *ptr = g_array_index(arr->array, hvm_obj_ref*, end);
  g_array_remove_index(arr->array, end);
  return ptr;
}

hvm_obj_ref* hvm_obj_array_get(hvm_obj_ref *arrref, hvm_obj_ref *idxref) {
  assert(arrref->type == HVM_ARRAY); assert(idxref->type == HVM_INTEGER);
  hvm_obj_array *arr = arrref->data.v;
  guint idx, len;
  idx = (guint)(idxref->data.i64);
  len = arr->array->len;
  assert(idx < len);
  hvm_obj_ref *ptr = g_array_index(arr->array, hvm_obj_ref*, idx);
  return ptr;
}

hvm_obj_ref* hvm_obj_array_remove(hvm_obj_ref *arrref, hvm_obj_ref *idxref) {
  assert(arrref->type == HVM_ARRAY); assert(idxref->type == HVM_INTEGER);
  hvm_obj_array *arr = arrref->data.v;
  guint idx, len;
  idx = (guint)(idxref->data.i64);
  len = arr->array->len;
  assert(idx < len);
  hvm_obj_ref *ptr = g_array_index(arr->array, hvm_obj_ref*, idx);
  g_array_remove_index(arr->array, idx);
  return ptr;
}

void hvm_obj_array_set(hvm_obj_ref *arrref, hvm_obj_ref *idxref, hvm_obj_ref *valref) {
  assert(arrref->type == HVM_ARRAY); assert(idxref->type == HVM_INTEGER);
  hvm_obj_array *arr = arrref->data.v;
  guint idx, len;
  idx = (guint)(idxref->data.i64);
  len = arr->array->len;
  assert(idx < len);
  hvm_obj_ref **el = &g_array_index(arr->array, hvm_obj_ref*, idx);
  *el = valref;
}

void hvm_obj_struct_set(hvm_obj_ref *sref, hvm_obj_ref *key, hvm_obj_ref *val) {
  assert(sref->type == HVM_STRUCTURE); assert(key->type == HVM_SYMBOL);
  hvm_obj_struct *strct = sref->data.v;
  hvm_obj_struct_internal_set(strct, (hvm_symbol_id)(key->data.u64), val);
}
hvm_obj_ref* hvm_obj_struct_get(hvm_obj_ref *sref, hvm_obj_ref *key) {
  assert(sref->type == HVM_STRUCTURE); assert(key->type == HVM_SYMBOL);
  hvm_obj_struct *strct = sref->data.v;
  return hvm_obj_struct_internal_get(strct, (hvm_symbol_id)(key->data.u64));
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
void hvm_obj_struct_internal_grow_heap(hvm_obj_struct *strct) {
  strct->heap_size = strct->heap_size * HVM_STRUCT_HEAP_GROWTH_RATE;
  strct->heap = realloc(strct->heap, HVM_STRUCT_HEAP_MEMORY_SIZE(strct->heap_size));
}
void hvm_obj_struct_internal_set(hvm_obj_struct *strct, hvm_symbol_id id, hvm_obj_ref *obj) {
  while(strct->heap_length >= strct->heap_size) {
    hvm_obj_struct_internal_grow_heap(strct);
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
hvm_obj_ref *hvm_obj_struct_internal_get(hvm_obj_struct *strct, hvm_symbol_id id) {
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
