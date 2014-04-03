#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "vm.h"
#include "symbol.h"
#include "object.h"
#include "gc1.h"

hvm_gc1_obj_space *hvm_new_obj_space() {
  hvm_gc1_obj_space *space = malloc(sizeof(hvm_gc1_obj_space));
  space->heap.size    = HVM_GC1_INITIAL_HEAP_SIZE;
  space->heap.entries = malloc(HVM_GC1_HEAP_MEMORY_SIZE(space->heap.size));
  space->heap.length  = 0;
  return space;
}

void hvm_obj_space_grow(hvm_gc1_obj_space *space) {
  space->heap.size = HVM_GC1_HEAP_GROW_FUNCTION(space->heap.size);
  space->heap.entries = realloc(space->heap.entries, HVM_GC1_HEAP_MEMORY_SIZE(space->heap.size));
}

void hvm_obj_space_add_obj_ref(hvm_gc1_obj_space *space, hvm_obj_ref *obj) {
  unsigned int next_id = space->heap.length;
  if(next_id >= space->heap.size) {
    hvm_obj_space_grow(space);
  }
  hvm_gc1_heap_entry *entry = &space->heap.entries[next_id];
  entry->obj = obj;
  entry->flags = 0x0;
}
