#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <assert.h>

#include <glib.h>
#include <jemalloc/jemalloc.h>

#include "vm.h"
#include "symbol.h"
#include "object.h"
#include "frame.h"
#include "exception.h"

// Prefix to force inlining
#define ALWAYS_INLINE __attribute__((always_inline))

// TODO: Set up flywheeling of string objects to reduce memory overhead

hvm_obj_string *hvm_new_obj_string() {
  hvm_obj_string *str = je_malloc(sizeof(hvm_obj_string));
  str->data = NULL;
  return str;
}

// BOOLEANS -------------------------------------------------------------------

ALWAYS_INLINE bool _hvm_obj_is_falsey(hvm_obj_ref *ref) {
  // Falsey values are null and integer zero
  if(ref->type == HVM_NULL) {
    return true;
  } else if(ref->type == HVM_INTEGER && ref->data.i64 == 0) {
    return true;
  } else {
    return false;
  }
}
ALWAYS_INLINE bool _hvm_obj_is_truthy(hvm_obj_ref *ref) {
  return !_hvm_obj_is_falsey(ref);
}

bool hvm_obj_is_falsey(hvm_obj_ref *ref) {
  return _hvm_obj_is_falsey(ref);
}
bool hvm_obj_is_truthy(hvm_obj_ref *ref) {
  return _hvm_obj_is_truthy(ref);
}

// Internal array API

uint64_t hvm_array_len(hvm_obj_array *arr) {
  guint len = arr->array->len;
  return (uint64_t)len;
}

ALWAYS_INLINE hvm_obj_ref* _hvm_obj_array_internal_get(hvm_obj_array *arr, uint64_t _idx) {
  guint idx, len;
  idx = (guint)_idx;
  len = arr->array->len;
  assert(idx < len);
  hvm_obj_ref *ptr = g_array_index(arr->array, hvm_obj_ref*, idx);
  return ptr;
}
hvm_obj_ref* hvm_obj_array_internal_get(hvm_obj_array *arr, uint64_t _idx) {
  return _hvm_obj_array_internal_get(arr, _idx);
}

// Public array API

hvm_obj_array *hvm_new_obj_array() {
  hvm_obj_array *arr = je_malloc(sizeof(hvm_obj_array));
  //arr->data = malloc(sizeof(hvm_obj_ref*) * 1);
  //arr->data[0] = NULL;
  //arr->length = 0;
  arr->array = g_array_new(TRUE, TRUE, sizeof(hvm_obj_ref*));
  return arr;
}
hvm_obj_array *hvm_new_obj_array_with_length(hvm_obj_ref *lenref) {
  hvm_obj_array *arr = je_malloc(sizeof(hvm_obj_array));
  guint len;
  if(lenref->type == HVM_INTEGER) {
    len = (guint)(lenref->data.i64);
  } else if(lenref->type == HVM_NULL) {
    len = 0;
  } else {
    fprintf(stderr, "Invalid object type for length\n");
    assert(false);
  }
  arr->array = g_array_sized_new(TRUE, TRUE, sizeof(hvm_obj_ref*), len);
  // Pre-fill the array with nulls
  // TODO: See if we can use `g_array_append_vals` to make this faster
  for(guint i = 0; i < len; i++) {
    g_array_append_val(arr->array, hvm_const_null);
  }
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

hvm_obj_ref* hvm_obj_array_len(hvm_vm *vm, hvm_obj_ref *a) {
  hvm_obj_array *arr = a->data.v;
  guint len = arr->array->len;
  hvm_obj_ref *intval = hvm_new_obj_int(vm);
  intval->data.i64 = (int64_t)len;
  return intval;
}

hvm_obj_ref* hvm_obj_array_get(hvm_obj_ref *arrref, hvm_obj_ref *idxref) {
  assert(arrref->type == HVM_ARRAY); assert(idxref->type == HVM_INTEGER);
  return _hvm_obj_array_internal_get(arrref->data.v, (uint64_t)(idxref->data.i64));
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
  assert(arrref->type == HVM_ARRAY);
  assert(idxref->type == HVM_INTEGER);
  hvm_obj_array *arr = arrref->data.v;
  guint idx, len;
  idx = (guint)(idxref->data.i64);
  // Look up the length of the array
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
hvm_obj_ref* hvm_obj_struct_delete(hvm_obj_ref *sref, hvm_obj_ref *key) {
  assert(sref->type == HVM_STRUCTURE); assert(key->type == HVM_SYMBOL);
  hvm_obj_struct *strct = sref->data.v;
  hvm_symbol_id   sym = key->data.u64;
  hvm_obj_ref    *val = hvm_obj_struct_internal_get(strct, sym);
  hvm_obj_struct_internal_set(strct, sym, hvm_const_null);
  return val;
}


hvm_obj_ref *hvm_new_obj_int(hvm_vm *vm) {
  static int64_t zero = 0;
  hvm_obj_ref *ref = hvm_obj_ref_new_from_pool(vm);
  ref->type = HVM_INTEGER;
  ref->data.i64 = zero;
  ref->flags = 0x0;
  return ref;
}

hvm_obj_ref *hvm_obj_cmp_and(hvm_vm *vm, hvm_obj_ref *a, hvm_obj_ref *b) {
  // Integer value for the result
  hvm_obj_ref *val = hvm_new_obj_int(vm);
  // Do our truthy test (using always-inlined _hvm_obj_is_truthy)
  val->data.i64 = (int64_t)((_hvm_obj_is_truthy(a) && _hvm_obj_is_truthy(b)) ? 1 : 0);
  return val;
}

#define INT_TYPE_CHECK assert(a != NULL); \
                       assert(b != NULL); \
                       if(a->type != HVM_INTEGER || b->type != HVM_INTEGER) { return NULL; }

hvm_obj_ref *hvm_obj_int_add(hvm_vm *vm, hvm_obj_ref *a, hvm_obj_ref *b) {
  INT_TYPE_CHECK;
  hvm_obj_ref *c = hvm_obj_ref_new_from_pool(vm);
  int64_t av, bv, cv;
  av = a->data.i64;
  bv = b->data.i64;
  cv = av + bv;
  c->type = HVM_INTEGER;
  c->data.i64 = cv;
  c->flags = 0x0;
  return c;
}
hvm_obj_ref *hvm_obj_int_sub(hvm_vm *vm, hvm_obj_ref *a, hvm_obj_ref *b) {
  INT_TYPE_CHECK;
  hvm_obj_ref *c = hvm_obj_ref_new_from_pool(vm);
  int64_t av, bv, cv;
  av = a->data.i64;
  bv = b->data.i64;
  cv = av - bv;
  c->type = HVM_INTEGER;
  c->data.i64 = cv;
  c->flags = 0x0;
  return c;
}
hvm_obj_ref *hvm_obj_int_mul(hvm_vm *vm, hvm_obj_ref *a, hvm_obj_ref *b) {
  INT_TYPE_CHECK;
  hvm_obj_ref *c = hvm_obj_ref_new_from_pool(vm);
  int64_t av, bv, cv;
  av = a->data.i64;
  bv = b->data.i64;
  cv = av * bv;
  c->type = HVM_INTEGER;
  c->data.i64 = cv;
  c->flags = 0x0;
  return c;
}
hvm_obj_ref *hvm_obj_int_div(hvm_vm *vm, hvm_obj_ref *a, hvm_obj_ref *b) {
  INT_TYPE_CHECK;
  hvm_obj_ref *c = hvm_obj_ref_new_from_pool(vm);
  int64_t av, bv, cv;
  av = a->data.i64;
  bv = b->data.i64;
  cv = av / bv;
  c->type = HVM_INTEGER;
  c->data.i64 = cv;
  c->flags = 0x0;
  return c;
}
hvm_obj_ref *hvm_obj_int_mod(hvm_vm *vm, hvm_obj_ref *a, hvm_obj_ref *b) {
  INT_TYPE_CHECK;
  hvm_obj_ref *c = hvm_obj_ref_new_from_pool(vm);
  int64_t av, bv, cv;
  av = a->data.i64;
  bv = b->data.i64;
  cv = av % bv;
  c->type = HVM_INTEGER;
  c->data.i64 = cv;
  c->flags = 0x0;
  return c;
}
#define INT_COMPARISON_OP_HEAD hvm_obj_ref *c = hvm_obj_ref_new_from_pool(vm); \
                               c->type  = HVM_INTEGER; \
                               c->flags = 0x0; \
                               int64_t av, bv, cv; \
                               av = a->data.i64; \
                               bv = b->data.i64;
hvm_obj_ref *hvm_obj_int_lt(hvm_vm *vm, hvm_obj_ref *a, hvm_obj_ref *b) {
  INT_TYPE_CHECK;
  INT_COMPARISON_OP_HEAD;
  cv = av < bv;
  c->data.i64 = cv;
  return c;
}
hvm_obj_ref *hvm_obj_int_gt(hvm_vm *vm, hvm_obj_ref *a, hvm_obj_ref *b) {
  INT_TYPE_CHECK;
  INT_COMPARISON_OP_HEAD;
  cv = av > bv;
  c->data.i64 = cv;
  return c;
}
hvm_obj_ref *hvm_obj_int_lte(hvm_vm *vm, hvm_obj_ref *a, hvm_obj_ref *b) {
  INT_TYPE_CHECK;
  INT_COMPARISON_OP_HEAD;
  cv = av <= bv;
  c->data.i64 = cv;
  return c;
}
hvm_obj_ref *hvm_obj_int_gte(hvm_vm *vm, hvm_obj_ref *a, hvm_obj_ref *b) {
  INT_TYPE_CHECK;
  INT_COMPARISON_OP_HEAD;
  cv = av >= bv;
  c->data.i64 = cv;
  return c;
}
hvm_obj_ref *hvm_obj_int_eq(hvm_vm *vm, hvm_obj_ref *a, hvm_obj_ref *b) {
  INT_TYPE_CHECK;
  INT_COMPARISON_OP_HEAD;
  cv = av == bv;
  c->data.i64 = cv;
  return c;
}

// STRUCTS --------------------------------------------------------------------

hvm_obj_struct *hvm_new_obj_struct() {
  hvm_obj_struct *strct = je_malloc(sizeof(hvm_obj_struct));
  strct->heap_length = 0;
  strct->heap_size = HVM_STRUCT_INITIAL_HEAP_SIZE;
  strct->heap = je_malloc(HVM_STRUCT_HEAP_MEMORY_SIZE(strct->heap_size));
  strct->heap[0] = NULL;
  return strct;
}
void hvm_obj_struct_internal_grow_heap(hvm_obj_struct *strct) {
  strct->heap_size = strct->heap_size * HVM_STRUCT_HEAP_GROWTH_RATE;
  strct->heap = je_realloc(strct->heap, HVM_STRUCT_HEAP_MEMORY_SIZE(strct->heap_size));
}
void hvm_obj_struct_internal_set(hvm_obj_struct *strct, hvm_symbol_id id, hvm_obj_ref *obj) {
  while(strct->heap_length >= strct->heap_size) {
    hvm_obj_struct_internal_grow_heap(strct);
  }
  // Create the pair
  hvm_obj_struct_heap_pair *pair = je_malloc(sizeof(hvm_obj_struct_heap_pair));
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

// OBJECT REFERENCES ----------------------------------------------------------

hvm_obj_ref *hvm_new_obj_ref() {
  hvm_obj_ref *ref = je_malloc(sizeof(hvm_obj_ref));
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


#define ZONE hvm_obj_ref_pool_zone
#define POOL hvm_obj_ref_pool

// Forward delcarations of private functions
static ZONE *pool_find_next_earliest_free(POOL *pool, ZONE *zone);

ZONE *hvm_obj_ref_pool_zone_new() {
  ZONE *zone          = je_malloc(sizeof(ZONE));
  zone->refs          = je_malloc(sizeof(hvm_obj_ref) * HVM_OBJ_REF_POOL_ZONE_SIZE);
  // Earliest free index in the zone is the first since it's empty
  zone->earliest_free = 0;
  zone->prev          = NULL;
  zone->next          = NULL;
  return zone;
}

hvm_obj_ref_pool *hvm_obj_ref_pool_new() {
  hvm_obj_ref_pool *pool = je_malloc(sizeof(hvm_obj_ref_pool));
  // Create the initial zone for the pool too
  hvm_obj_ref_pool_zone *zone = hvm_obj_ref_pool_zone_new();
  // And set it to all the right slots
  pool->head          = zone;
  pool->tail          = zone;
  pool->earliest_free = zone;
  return pool;
}
hvm_obj_ref *hvm_obj_ref_new_from_pool(hvm_vm *vm) {
  return je_malloc(sizeof(hvm_obj_ref));
  // TODO: Switch to a fast pool allocator
  /*
  hvm_obj_ref_pool *pool = vm->ref_pool;
  // Get the earliest zone with free slots
  ZONE *zone = pool->earliest_free;
  // Get the earliest free slot of that zone as our reference
  hvm_obj_ref *ref = &zone->refs[zone->earliest_free];
  // Calculate the next earliest free zone since the current is now invalid
  pool->earliest_free = pool_find_next_earliest_free(pool, zone);
  // Make sure the ref is all sanitized
  if(ref->type != HVM_NULL) {
    fprintf(stderr, "Encountered non-sanitized object reference\n");
    assert(ref->type == HVM_NULL);
  }
  ref->data.v = 0x0;
  ref->flags  = 0x0;
  ref->entry  = 0x0;
  return ref;
  */
}

ZONE *pool_find_next_earliest_free(POOL *pool, ZONE *zone) {
  ZONE *last_zone = zone;
  // Bump it forward since we just used the current free slot in
  // the caller (`hvm_obj_ref_new_from_pool`).
  zone->earliest_free += 1;
  while(zone != NULL) {
    // Make sure this zone is recorded as the last being processed in case
    // it turns out to be the last in the list
    last_zone = zone;
    // Check if we're out of space
    if(zone->earliest_free == HVM_OBJ_REF_POOL_ZONE_FULL) {
      // Advance to the next zone if we're out of space
      zone = zone->next;
      continue;
    }
    hvm_obj_ref *ref = &zone->refs[zone->earliest_free];
    if(ref->type == HVM_NULL) {
      return zone;
    }
    zone->earliest_free += 1;
  }
  // Make sure we've actually run out of zones
  if(zone != NULL) {
    fprintf(stderr, "Failed to fully scan pool zones\n");
    assert(zone == NULL);
  }
  zone = hvm_obj_ref_pool_zone_new();
  // Update the double-links
  zone->prev = last_zone;
  assert(last_zone->next == NULL);
  last_zone->next = zone;
  // Update the pool
  pool->tail = zone;
  return zone;
}

void hvm_obj_ref_free(hvm_vm *vm, hvm_obj_ref *ref) {
  POOL *pool = vm->ref_pool;
  // Now we need to find the zone for this ref; going to start backwards
  // from the latest-zone
  ZONE *zone = pool->tail;
  while(zone != NULL) {
    hvm_obj_ref *start = zone->refs;
    hvm_obj_ref *end   = zone->refs + HVM_OBJ_REF_POOL_ZONE_SIZE;
    if(ref >= start && ref < end) {
      // Found the right zone!
      break;
    }
    zone = zone->prev;
  }
  if(zone == NULL) {
    fprintf(stderr, "Failed to find zone containing object reference\n");
    assert(zone != NULL);
  }
  // Use the address difference from the base of the zone to the pointer to
  // calculate the index of that pointer
  intptr_t diff = ref - zone->refs;

  bool valid_diff = (diff >= 0 && diff < HVM_OBJ_REF_POOL_ZONE_SIZE);
  if(!valid_diff) {
    fprintf(stderr, "Invalid reference index into zone\n");
    assert(valid_diff);
  }
  // Convert the pointer to an integer offset (ie. index)
  unsigned int idx = (unsigned int)diff;
  // Now let's mark it as HVM_NULL and check if we need to update the
  // .earliest_free of the zone
  ref->type = HVM_NULL;
  if(idx < zone->earliest_free) {
    zone->earliest_free = idx;
  }
  // Check if one of this zone's successors is marked as the earliest free zone
  bool is_earliest = false;
  ZONE *succ = zone->next;
  while(succ != NULL) {
    // Successor is marked as earliest, so this means we're the new earliest
    if(succ == pool->earliest_free) {
      is_earliest = true;
      break;
    }
    succ = succ->next;
  }
  // We are the earliest free zone in the whole pool
  if(is_earliest) {
    pool->earliest_free = zone;
  }
}

// DESTRUCTORS ----------------------------------------------------------------

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
  je_free(ref);
}
void hvm_obj_struct_free(hvm_obj_struct *strct) {
  // Free the struct's internal heap
  je_free(strct->heap);
}


// UTILITIES ------------------------------------------------------------------

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
