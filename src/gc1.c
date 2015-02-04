#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>

#include "vm.h"
#include "symbol.h"
#include "object.h"
#include "frame.h"
#include "gc1.h"
#include "exception.h"

#define FLAGTRUE(v, f)  (v & f) == f
#define FLAGFALSE(v, f) (v & f) == 0

#define FLAG_GC_MARKED 0x1

// 0000 0001
#define MARK_ENTRY(E)   E->flags = E->flags | 0x1;
// 1111 1110
#define UNMARK_ENTRY(E) E->flags = E->flags & 0xFE;

// Dereferences an entry pointer and returns the first byte of the struct.
#define FIRST_BYTE_OF_ENTRY(V) *(char*)(V)


hvm_gc1_obj_space *hvm_new_obj_space() {
  hvm_gc1_obj_space *space = malloc(sizeof(hvm_gc1_obj_space));
  space->heap.size    = HVM_GC1_INITIAL_HEAP_SIZE;
  space->heap.entries = calloc(HVM_GC1_HEAP_MEMORY_SIZE(space->heap.size), sizeof(hvm_gc1_heap_entry));
  space->heap.length  = 0;
  return space;
}

static inline void obj_space_reset_marks(hvm_gc1_obj_space *space) {
  unsigned int id = 0;
  while(id < space->heap.length) {
    hvm_gc1_heap_entry *entry = &space->heap.entries[id];
    // entry->flags = entry->flags & 0xFE;
    UNMARK_ENTRY(entry);
    id += 1;
  }
}

// Forward declaration
static inline void mark_struct(hvm_obj_struct *strct);
static inline void mark_array(hvm_obj_array *arr);

static inline void mark_obj_ref(hvm_obj_ref *obj) {
  if(FLAGTRUE(obj->flags, HVM_OBJ_FLAG_CONSTANT) ||
     FLAGFALSE(obj->flags, HVM_OBJ_FLAG_GC_TRACKED)
  ) {
    return;// Don't process constants or untracked objects
  }
  assert(obj->entry != NULL); // Make sure there is a GC entry
  hvm_gc1_heap_entry *entry = obj->entry;
  // Check if already marked
  if(FLAGTRUE(entry->flags, FLAG_GC_MARKED)) { return; }
  // Mark the GC entry
  MARK_ENTRY(entry);
  // Handle complex data structures
  if(obj->type == HVM_STRUCTURE) {
    mark_struct(obj->data.v);
  } else if(obj->type == HVM_ARRAY) {
    mark_array(obj->data.v);
  } else if(obj->type == HVM_EXCEPTION) {
    hvm_exception *exc = obj->data.v;
    mark_obj_ref(exc->data);
  }
}

void mark_struct(hvm_obj_struct *strct) {
  unsigned int idx;
  for(idx = 0; idx < strct->heap_length; idx++) {
    hvm_obj_struct_heap_pair *pair = strct->heap[idx];
    hvm_obj_ref *obj = pair->obj;
    mark_obj_ref(obj);
  }
}
void mark_array(hvm_obj_array *arr) {
  uint64_t idx, len;
  len = hvm_array_len(arr);
  for(idx = 0; idx < len; idx++) {
    hvm_obj_ref *ptr = hvm_obj_array_internal_get(arr, idx);
    mark_obj_ref(ptr);
  }
}

static inline void mark_registers(hvm_vm *vm) {
  uint32_t i;
  for(i = 0; i < HVM_GENERAL_REGISTERS; i++) {
    hvm_obj_ref* obj = vm->general_regs[i];
    if(obj != NULL) { mark_obj_ref(obj); }
  }
  for(i = 0; i < HVM_ARGUMENT_REGISTERS; i++) {
    hvm_obj_ref* obj = vm->arg_regs[i];
    if(obj != NULL) { mark_obj_ref(obj); }
  }
  for(i = 0; i < HVM_PARAMETER_REGISTERS; i++) {
    hvm_obj_ref* obj = vm->param_regs[i];
    if(obj != NULL) { mark_obj_ref(obj); }
  }
}

static inline void mark_stack(hvm_vm *vm) {
  uint32_t i;
  for(i = 0; i <= vm->stack_depth; i++) {
    struct hvm_frame *frame = &vm->stack[i];
    hvm_obj_struct *locals = frame->locals;
    mark_struct(locals);
  }
}

void hvm_gc1_free(hvm_gc1_heap_entry *entry) {
  // Free the object referenced
  hvm_obj_free(entry->obj);
  // Then clear out the entry
  // memset(entry, 0, sizeof(hvm_gc1_heap_entry));
  // Only setting the first byte since that's what's checked.
  FIRST_BYTE_OF_ENTRY(entry) = 0;
}

static inline void sweep_space(hvm_gc1_obj_space *space) {
  for(uint32_t id = 0; id < space->heap.length; id++) {
    hvm_gc1_heap_entry *entry = &space->heap.entries[id];
    if(FLAGFALSE(entry->flags, FLAG_GC_MARKED)) {
      fprintf(stderr, "freeing:%d\n", id);
      hvm_gc1_free(entry);
    }
  }
}

static inline bool entry_is_null(hvm_gc1_obj_space *space, uint32_t idx) {
  hvm_gc1_heap_entry *entry = &space->heap.entries[idx];
  return FIRST_BYTE_OF_ENTRY(entry) == 0;
}

static inline void find_next_free_entry(hvm_gc1_obj_space *space, uint32_t *free_entry) {
  uint32_t idx = *free_entry;
  while(idx < space->heap.length && !entry_is_null(space, idx)) {
    idx += 1;
  }
  *free_entry = idx;
}
static inline bool has_used_entry_after(hvm_gc1_obj_space *space, uint32_t free_entry, uint32_t *used_entry) {
  // Start at the index after the free entry index
  uint32_t idx = free_entry + 1;
  // Make sure we have space left to search
  while(idx < space->heap.length) {
    if(entry_is_null(space, idx)) {
      // Pass over free entry
      idx += 1;
      continue;
    } else {
      // Non-null entry, so it's used
      *used_entry = idx;
      return true;
    }
  }
  return false; // No space left
}

static inline void relocate_entry(hvm_gc1_obj_space *space, uint32_t free_entry, uint32_t used_entry) {
  assert(free_entry < used_entry);
  hvm_gc1_heap_entry *dest   = &space->heap.entries[free_entry];
  hvm_gc1_heap_entry *source = &space->heap.entries[used_entry];
  // Copy the entries
  memcpy(dest, source, sizeof(hvm_gc1_heap_entry));
  // "Delete" the old entry
  FIRST_BYTE_OF_ENTRY(source) = 0;
  // Update the object reference to point to the right place
  hvm_obj_ref *obj = dest->obj;
  obj->entry = dest;
}

static inline void compact_space(hvm_gc1_obj_space *space) {
  uint32_t free_entry = 0;
  uint32_t used_entry = 0;
  find_next_free_entry(space, &free_entry);
  while(has_used_entry_after(space, free_entry, &used_entry)) {
    // Move the used entry to the free entry
    relocate_entry(space, free_entry, used_entry);
    fprintf(stderr, "relocating from used:%d to free:%d\n", used_entry, free_entry);
    // Find the next free entry after this one
    free_entry += 1;
    find_next_free_entry(space, &free_entry);
  }
  fprintf(stderr, "free entry:%d\n", free_entry);
  space->heap.length = free_entry;
}

static inline void obj_space_mark(hvm_vm *vm) {
  // Go through the registers
  mark_registers(vm);
  // Climb through each of the stack frames
  mark_stack(vm);
}

void hvm_gc1_run(hvm_vm *vm, hvm_gc1_obj_space *space) {
  // fprintf(stderr, "gc1_run.start\n");
  // Reset all of our markings
  obj_space_reset_marks(space);
  // Mark objects
  obj_space_mark(vm);
  // Free unmarked objects
  sweep_space(space);
  // Compact the object space
  compact_space(space);
  // fprintf(stderr, "gc1_run.end\n");
  // TODO: Shrink the object space
}

void hvm_obj_space_grow(hvm_gc1_obj_space *space) {
  unsigned int old_size = space->heap.size;
  space->heap.size = HVM_GC1_HEAP_GROW_FUNCTION(space->heap.size);
  fprintf(stderr, "gc1: growing from %u to %u\n", old_size, space->heap.size);
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
  unsigned int next_id = space->heap.length;
  space->heap.length += 1;
  // Check if we still have space
  if(next_id >= space->heap.size) {
    hvm_obj_space_grow(space);
  }
  hvm_gc1_heap_entry *entry = &space->heap.entries[next_id];
  // fprintf(stderr, "obj_space_add: obj_ref = %p (%s)\n", obj, hvm_human_name_for_obj_type(obj->type));
  // Set up the entry to point to the object
  entry->obj = obj;
  entry->flags = 0x0;
  // Set the object to point back to the entry
  obj->entry = entry;
}
