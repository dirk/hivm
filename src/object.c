#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <assert.h>

#include <glib.h>

#include "vm.h"
#include "symbol.h"
#include "object.h"
#include "frame.h"
#include "exception.h"

// TODO: Flywheelize this.
hvm_obj_string *hvm_new_obj_string() {
  hvm_obj_string *str = malloc(sizeof(hvm_obj_string));
  str->data = NULL;
  return str;
}

// BOOLEANS -------------------------------------------------------------------

bool hvm_obj_is_falsey(hvm_obj_ref *ref) {
  // Falsey values are null and integer zero
  if(ref->type == HVM_NULL) {
    return true;
  } else if(ref->type == HVM_INTEGER && ref->data.i64 == 0) {
    return true;
  } else {
    return false;
  }
}
bool hvm_obj_is_truthy(hvm_obj_ref *ref) {
  return !hvm_obj_is_falsey(ref);
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
  guint len;
  if(lenref->type == HVM_INTEGER) {
    len = (guint)(lenref->data.i64);
  } else if(lenref->type == HVM_NULL) {
    len = 0;
  } else {
    fprintf(stderr, "Invalid object type for length\n");
    assert(false);
  }
  // printf("new array len: %d\n", len);
  arr->array = g_array_sized_new(TRUE, TRUE, sizeof(hvm_obj_ref*), len);
  // printf("arr->array->len: %d\n", arr->array->len);
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

hvm_obj_ref* hvm_obj_array_len(hvm_obj_ref *a) {
  hvm_obj_array *arr = a->data.v;
  guint len = arr->array->len;
  hvm_obj_ref *intval = hvm_new_obj_int();
  intval->data.i64 = (int64_t)len;
  return intval;
}

hvm_obj_ref* hvm_obj_array_get(hvm_obj_ref *arrref, hvm_obj_ref *idxref) {
  assert(arrref->type == HVM_ARRAY); assert(idxref->type == HVM_INTEGER);
  return hvm_obj_array_internal_get(arrref->data.v, (uint64_t)(idxref->data.i64));
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

uint64_t hvm_obj_array_internal_len(hvm_obj_array *arr) {
  guint len = arr->array->len;
  return (uint64_t)len;
}
hvm_obj_ref* hvm_obj_array_internal_get(hvm_obj_array *arr, uint64_t _idx) {
  guint idx, len;
  idx = (guint)_idx;
  len = arr->array->len;
  assert(idx < len);
  hvm_obj_ref *ptr = g_array_index(arr->array, hvm_obj_ref*, idx);
  return ptr;
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
hvm_obj_ref* hvm_obj_struct_delete(hvm_obj_ref *sref, hvm_obj_ref *key) {
  assert(sref->type == HVM_STRUCTURE); assert(key->type == HVM_SYMBOL);
  hvm_obj_struct *strct = sref->data.v;
  hvm_symbol_id   sym = key->data.u64;
  hvm_obj_ref    *val = hvm_obj_struct_internal_get(strct, sym);
  hvm_obj_struct_internal_set(strct, sym, hvm_const_null);
  return val;
}


hvm_obj_ref *hvm_new_obj_int() {
  static int64_t zero = 0;
  hvm_obj_ref *ref = hvm_new_obj_ref();
  ref->type = HVM_INTEGER;
  ref->data.i64 = zero;
  return ref;
}

#define INT_TYPE_CHECK assert(a != NULL); \
                       assert(b != NULL); \
                       if(a->type != HVM_INTEGER || b->type != HVM_INTEGER) { return NULL; }

hvm_obj_ref *hvm_obj_int_add(hvm_obj_ref *a, hvm_obj_ref *b) {
  INT_TYPE_CHECK;
  hvm_obj_ref *c = hvm_new_obj_ref();
  int64_t av, bv, cv;
  av = a->data.i64;
  bv = b->data.i64;
  cv = av + bv;
  c->type = HVM_INTEGER;
  c->data.i64 = cv;
  return c;
}
hvm_obj_ref *hvm_obj_int_sub(hvm_obj_ref *a, hvm_obj_ref *b) {
  INT_TYPE_CHECK;
  hvm_obj_ref *c = hvm_new_obj_ref();
  int64_t av, bv, cv;
  av = a->data.i64;
  bv = b->data.i64;
  cv = av - bv;
  c->type = HVM_INTEGER;
  c->data.i64 = cv;
  return c;
}
hvm_obj_ref *hvm_obj_int_mul(hvm_obj_ref *a, hvm_obj_ref *b) {
  INT_TYPE_CHECK;
  hvm_obj_ref *c = hvm_new_obj_ref();
  int64_t av, bv, cv;
  av = a->data.i64;
  bv = b->data.i64;
  cv = av * bv;
  c->type = HVM_INTEGER;
  c->data.i64 = cv;
  return c;
}
hvm_obj_ref *hvm_obj_int_div(hvm_obj_ref *a, hvm_obj_ref *b) {
  INT_TYPE_CHECK;
  hvm_obj_ref *c = hvm_new_obj_ref();
  int64_t av, bv, cv;
  av = a->data.i64;
  bv = b->data.i64;
  cv = av / bv;
  c->type = HVM_INTEGER;
  c->data.i64 = cv;
  return c;
}
hvm_obj_ref *hvm_obj_int_mod(hvm_obj_ref *a, hvm_obj_ref *b) {
  INT_TYPE_CHECK;
  hvm_obj_ref *c = hvm_new_obj_ref();
  int64_t av, bv, cv;
  av = a->data.i64;
  bv = b->data.i64;
  cv = av % bv;
  c->type = HVM_INTEGER;
  c->data.i64 = cv;
  return c;
}
#define INT_COMPARISON_OP_HEAD hvm_obj_ref *c = hvm_new_obj_ref(); \
                               c->type = HVM_INTEGER; \
                               int64_t av, bv, cv; \
                               av = a->data.i64; \
                               bv = b->data.i64;
hvm_obj_ref *hvm_obj_int_lt(hvm_obj_ref *a, hvm_obj_ref *b) {
  INT_TYPE_CHECK;
  INT_COMPARISON_OP_HEAD;
  cv = av < bv;
  c->data.i64 = cv;
  return c;
}
hvm_obj_ref *hvm_obj_int_gt(hvm_obj_ref *a, hvm_obj_ref *b) {
  INT_TYPE_CHECK;
  INT_COMPARISON_OP_HEAD;
  cv = av > bv;
  c->data.i64 = cv;
  return c;
}
hvm_obj_ref *hvm_obj_int_lte(hvm_obj_ref *a, hvm_obj_ref *b) {
  INT_TYPE_CHECK;
  INT_COMPARISON_OP_HEAD;
  cv = av <= bv;
  c->data.i64 = cv;
  return c;
}
hvm_obj_ref *hvm_obj_int_gte(hvm_obj_ref *a, hvm_obj_ref *b) {
  INT_TYPE_CHECK;
  INT_COMPARISON_OP_HEAD;
  cv = av >= bv;
  c->data.i64 = cv;
  return c;
}
hvm_obj_ref *hvm_obj_int_eq(hvm_obj_ref *a, hvm_obj_ref *b) {
  INT_TYPE_CHECK;
  INT_COMPARISON_OP_HEAD;
  cv = av == bv;
  c->data.i64 = cv;
  return c;
}

// STRUCTS --------------------------------------------------------------------

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
  while(idx > 0 && id < strct->heap[idx / 2]->id) {
    strct->heap[idx] = strct->heap[idx / 2];
    idx = idx / 2;
  }
  strct->heap[idx] = pair;
  strct->heap_length += 1;
}
hvm_obj_ref *hvm_obj_struct_internal_get(hvm_obj_struct *strct, hvm_symbol_id id) {
  // Definitely not there if it's empty
  if(strct->heap_length == 0) { return NULL; }

  // Using binary search
  unsigned int min, max, mid;
  hvm_symbol_id i;

  min = 0;
  max = strct->heap_length - 1;
  while(min < max) {
    // Calculate midpoint of current range
    mid = min + ((max - min) / 2);
    // Sanity check
    assert(mid < max);
    // Branching to halves
    i = strct->heap[mid]->id;
    if(i < id) {
      // If the middle is less than the target take the right half.
      min = mid + 1;
    } else {
      max = mid;
    }
  }
  if(max == min) {
    i = strct->heap[min]->id;
    if(i == id) {
      return strct->heap[min]->obj;
    }
  }
  return NULL;
}

void hvm_obj_print_structure(hvm_vm *vm, hvm_obj_struct *strct) {
  hvm_obj_struct_heap_pair *pair;
  unsigned int idx = 0;
  fprintf(stderr, "struct(%p):\n", strct);
  while(idx < strct->heap_length) {
    pair = strct->heap[idx];
    fprintf(stderr, "  %llu = %p (sym: %s)\n", pair->id, pair->obj, hvm_desymbolicate(vm->symbols, pair->id));
    idx++;
  }
}

hvm_obj_ref *hvm_new_obj_ref() {
  hvm_obj_ref *ref = malloc(sizeof(hvm_obj_ref));
  ref->type = HVM_NULL;
  ref->data.u64 = 0;
  ref->flags = 0;
  ref->entry = NULL;
  return ref;
}
void hvm_obj_ref_set_string(hvm_obj_ref *ref, hvm_obj_string *str) {
  ref->type = HVM_STRING;
  ref->data.v = str;
}

hvm_obj_ref *hvm_new_obj_ref_string_data(char *data) {
  hvm_obj_ref    *obj = hvm_new_obj_ref();
  hvm_obj_string *str = hvm_new_obj_string();
  str->data = data;
  hvm_obj_ref_set_string(obj, str);
  return obj;
}

// DESTRUCTORS
void hvm_obj_free(hvm_obj_ref *ref) {
  // Make sure it's not a special data type
  assert(ref->type != HVM_NULL && ref->type != HVM_SYMBOL && ref->type != HVM_INTERNAL);
  // Complex data structures need their underpinnings freed first
  if(ref->type == HVM_STRUCTURE) {
    hvm_obj_struct_free(ref->data.v);
  } else if(ref->type == HVM_ARRAY) {
    fprintf(stderr, "hvm_obj_array_free not implemented yet\n");
    assert(false);
  } else if(ref->type == HVM_EXCEPTION) {
    fprintf(stderr, "HVM_EXCEPTION is deprecated\n");
    assert(false);
    // hvm_exception *exc = ref->data.v;
    // free(exc);
  } else if(ref->type == HVM_STRING) {
    fprintf(stderr, "hvm_obj_string_free not implemented yet\n");
    assert(false);
  }
  free(ref);
}
void hvm_obj_struct_free(hvm_obj_struct *strct) {
  // Free the struct's internal heap
  free(strct->heap);
}


// UTILITIES

const char *hvm_human_name_for_obj_type(hvm_obj_type type) {
  static char *string  = "string",
              *unknown = "unknown",
              *symbol  = "symbol",
              *integer = "integer",
              *null = "null",
              *structure = "structure",
              *array = "array",
              *flot = "float";
  switch(type) {
    case HVM_STRING:
      return string;
    case HVM_SYMBOL:
      return symbol;
    case HVM_INTEGER:
      return integer;
    case HVM_NULL:
      return null;
    case HVM_STRUCTURE:
      return structure;
    case HVM_ARRAY:
      return array;
    case HVM_FLOAT:
      return flot;
    default:
      return unknown;
  }
}
