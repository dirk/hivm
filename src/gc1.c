#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "vm.h"
#include "symbol.h"
#include "object.h"
#include "frame.h"
#include "gc1.h"

#define FLAGTRUE(v, f)  (v & f) == f
#define FLAGFALSE(v, f) (v & f) == 0

hvm_gc1_obj_space *hvm_new_obj_space() {
  hvm_gc1_obj_space *space = malloc(sizeof(hvm_gc1_obj_space));
  space->heap.size    = HVM_GC1_INITIAL_HEAP_SIZE;
  space->heap.entries = malloc(HVM_GC1_HEAP_MEMORY_SIZE(space->heap.size));
  space->heap.length  = 0;
  return space;
}

void hvm_gc1_obj_space_mark_reset(hvm_gc1_obj_space *space) {
  unsigned int id = 0;
  while(id < space->heap.length) {
    hvm_gc1_heap_entry *entry = &space->heap.entries[id];
    entry->flags = entry->flags & 0xFE; // 1111 1110
    id += 1;
  }
}

// Forward declaration
static inline void _gc1_mark_struct(hvm_obj_struct *strct);

static inline void _gc1_mark_obj_ref(hvm_obj_ref *obj) {
  if(FLAGTRUE(obj->flags, HVM_OBJ_FLAG_CONSTANT) ||
     FLAGFALSE(obj->flags, HVM_OBJ_FLAG_GC_TRACKED)
  ) {
    return;// Don't process constants or untracked objects
  }
  assert(obj->entry != NULL); // Make sure there is a GC entry
  hvm_gc1_heap_entry *entry = obj->entry;
  // Check if already marked
  if(FLAGTRUE(entry->flags, 0x1)) { return; }
  // Mark the GC entry
  entry->flags = entry->flags | 0x1; // 0000 0001
  fprintf(stderr, "mark_obj_ref.marked: obj_ref = %p (%s)\n", obj, hvm_human_name_for_obj_type(obj->type));
  // Handle complex data structures
  if(obj->type == HVM_STRUCTURE) {
    _gc1_mark_struct(obj->data.v);
  } else if(obj->type == HVM_ARRAY) {
    fprintf(stderr, "mark_obj_ref.unmarked: obj_ref = %p (%s)\n", obj, hvm_human_name_for_obj_type(obj->type));
  }
}

void _gc1_mark_struct(hvm_obj_struct *strct) {
  unsigned int idx;
  for(idx = 0; idx < strct->heap_length; idx++) {
    hvm_obj_struct_heap_pair *pair = strct->heap[idx];
    hvm_obj_ref *obj = pair->obj;
    _gc1_mark_obj_ref(obj);
  }
}

static inline void _gc1_mark_registers(hvm_vm *vm) {
  uint32_t i;
  for(i = 0; i < HVM_GENERAL_REGISTERS; i++) {
    hvm_obj_ref* obj = vm->general_regs[i];
    if(obj != NULL) { _gc1_mark_obj_ref(obj); }
  }
  for(i = 0; i < HVM_ARGUMENT_REGISTERS; i++) {
    hvm_obj_ref* obj = vm->arg_regs[i];
    if(obj != NULL) { _gc1_mark_obj_ref(obj); }
  }
  for(i = 0; i < HVM_PARAMETER_REGISTERS; i++) {
    hvm_obj_ref* obj = vm->param_regs[i];
    if(obj != NULL) { _gc1_mark_obj_ref(obj); }
  }
}

void hvm_gc1_obj_space_mark(hvm_vm *vm, hvm_gc1_obj_space *space) {
  uint32_t i;
  // Go through the registers and mark
  _gc1_mark_registers(vm);
  // Climb through each of the stack frames and mark objects
  for(i = 0; i <= vm->stack_depth; i++) {
    struct hvm_frame *frame = &vm->stack[i];
    hvm_obj_struct *locals = frame->locals;
    _gc1_mark_struct(locals);
  }
}

void hvm_gc1_run(hvm_vm *vm, hvm_gc1_obj_space *space) {
  fprintf(stderr, "gc1_run.start\n");
  // Reset all of our markings
  hvm_gc1_obj_space_mark_reset(space);
  // Mark objects
  hvm_gc1_obj_space_mark(vm, space);
  fprintf(stderr, "gc1_run.end\n");
}

void hvm_obj_space_grow(hvm_gc1_obj_space *space) {
  space->heap.size = HVM_GC1_HEAP_GROW_FUNCTION(space->heap.size);
  space->heap.entries = realloc(space->heap.entries, HVM_GC1_HEAP_MEMORY_SIZE(space->heap.size));
}

void hvm_obj_space_add_obj_ref(hvm_gc1_obj_space *space, hvm_obj_ref *obj) {
  if(FLAGTRUE(obj->flags, HVM_OBJ_FLAG_CONSTANT)) {
    return;// Don't track constants
  }
  if(FLAGTRUE(obj->flags, HVM_OBJ_FLAG_GC_TRACKED)) {
    return;// Already in the GC system
  } else {
    obj->flags |= HVM_OBJ_FLAG_GC_TRACKED;
  }
  // TODO: Get the next ID intelligently (ie. be able to decrease the length).
  unsigned int next_id = space->heap.length;
  space->heap.length += 1;
  // Check if we still have space
  if(next_id >= space->heap.size) {
    hvm_obj_space_grow(space);
  }
  hvm_gc1_heap_entry *entry = &space->heap.entries[next_id];
  fprintf(stderr, "obj_space_add: obj_ref = %p (%s)\n", obj, hvm_human_name_for_obj_type(obj->type));
  // Set up the entry to point to the object
  entry->obj = obj;
  entry->flags = 0x0;
  // Set the object to point back to the entry
  obj->entry = entry;
}
