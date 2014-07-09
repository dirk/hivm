#ifndef HVM_GC1_H
#define HVM_GC1_H

// Communicates with the VM, object space, and heap to mark and sweep memory
// in the object space and heap.

// Ideally this will run in a separate thread and do smart locking with the
// object space and heap to sweep and defrag/compact both with minimal
// locking with the VM.

#define HVM_GC1_INITIAL_HEAP_SIZE 1024
#define HVM_GC1_HEAP_GROW_FUNCTION(V) (V * 2)
#define HVM_GC1_HEAP_MEMORY_SIZE(S) (S * sizeof(hvm_gc1_heap_entry))

typedef struct hvm_gc1_heap {
  struct hvm_gc1_heap_entry *entries;
  /// Number of entries allocated
  unsigned int size;
  /// Number of entries in the heap
  unsigned int length;
} hvm_gc1_heap;

typedef struct hvm_gc1_heap_entry {
  hvm_obj_ref  *obj;
  byte flags;
} hvm_gc1_heap_entry;

typedef struct hvm_gc1_obj_space {
  hvm_gc1_heap heap;
} hvm_gc1_obj_space;

hvm_gc1_obj_space *hvm_new_obj_space();
void hvm_obj_space_add_obj_ref(hvm_gc1_obj_space *space, hvm_obj_ref *obj);

void hvm_gc1_run(hvm_vm *vm, hvm_gc1_obj_space *space);
/// Traverse the object space and reset the mark bits.
void hvm_gc1_obj_space_mark_reset(hvm_gc1_obj_space *space);
void hvm_gc1_obj_space_mark(hvm_vm *vm, hvm_gc1_obj_space *space);

#endif
