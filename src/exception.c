#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include <glib.h>

#include "vm.h"
#include "symbol.h"
#include "object.h"
#include "frame.h"
#include "chunk.h"
#include "gc1.h"
#include "exception.h"

hvm_exception *hvm_new_exception() {
  hvm_exception *exc = malloc(sizeof(hvm_exception));
  exc->message   = NULL;
  exc->data      = hvm_const_null;
  exc->backtrace = g_array_new(TRUE, TRUE, sizeof(hvm_location*));
  return exc;
}

void hvm_exception_push_location(hvm_exception *exc, hvm_location *loc) {
  g_array_append_val(exc->backtrace, loc);
}

hvm_chunk_debug_entry *hvm_vm_find_debug_entry(hvm_vm *vm, uint64_t ip) {
  hvm_chunk_debug_entry *de;
  for(uint64_t i = 0; i < vm->debug_entries_size; i++) {
    de = &vm->debug_entries[i];
    // printf("ip: %llu, start: %llu; end: %llu\n", ip, de->start, de->end);
    if(de->start <= ip && ip <= de->end) {
      return de;
    }
  }
  return NULL;
}

#define FLAG_IS_SET(VAL, FLAG) (VAL & FLAG) == FLAG

void hvm_exception_build_backtrace(hvm_exception *exc, hvm_vm *vm) {
  hvm_chunk_debug_entry *de;

  // for(uint64_t i = 0; i < vm->debug_entries_size; i++) {
  //   de = &vm->debug_entries[i];
  //   fprintf(stderr, "entry:\n");
  //   fprintf(stderr, "  start: %llu\n", de->start);
  //   fprintf(stderr, "  end: %llu\n", de->end);
  //   fprintf(stderr, "  line: %llu\n", de->line);
  //   fprintf(stderr, "  name: %s\n", de->name);
  //   fprintf(stderr, "  file: %s\n", de->file);
  //   fprintf(stderr, "  flags: %x\n", de->flags);
  // }

  uint32_t i = vm->stack_depth;
  while(1) {
    hvm_frame* frame = &vm->stack[i];
    uint64_t ip = frame->current_addr;

    de = hvm_vm_find_debug_entry(vm, ip);
    if(de != NULL && FLAG_IS_SET(de->flags, HVM_DEBUG_FLAG_HIDE_BACKTRACE)) {
      goto tail;
    }
    hvm_location *loc = malloc(sizeof(hvm_location));
    loc->frame = frame;
    if(de != NULL) {
      loc->name = de->name;
      loc->file = de->file;
      loc->line = (unsigned int)(de->line);
    } else {
      loc->name = "(unknown)";
      loc->file = "(unknown)";
      loc->line = 0;
    }
    hvm_exception_push_location(exc, loc);
  tail:
    // Preventing integer overflow wrap-around.
    if(i == 0) { break; }
    i--;
  }
}

void hvm_print_backtrace(void *backtrace_ptr) {
  GArray *backtrace = (GArray *)backtrace_ptr;
  unsigned int i;
  hvm_location *loc;
  for(i = 0; i < backtrace->len; i++) {
    loc = g_array_index(backtrace, hvm_location*, i);
    if(loc->name != NULL) {
      fprintf(stderr, "    %s (%d:%s)\n", loc->name, loc->line, loc->file);
    } else {
      fprintf(stderr, "    unknown (%d:%s)\n", loc->line, loc->file);
    }
  }
}

void hvm_exception_print(hvm_exception *exc) {
  hvm_obj_ref    *ref = exc->message;
  hvm_obj_string *str = ref->data.v;
  fprintf(stderr, "Exception: %s\n", str->data);
  fprintf(stderr, "Backtrace:\n");
  hvm_print_backtrace(exc->backtrace);
}

hvm_obj_ref *hvm_obj_for_exception(hvm_vm *vm, hvm_exception *exc) {
  // Create the reference to the internal exception
  hvm_obj_ref *excref = hvm_new_obj_ref();
  excref->type = HVM_EXCEPTION;
  excref->data.v = exc;
  excref->flags |= HVM_OBJ_FLAG_NO_FOLLOW;
  hvm_obj_space_add_obj_ref(vm->obj_space, excref);
  return excref;
  /*
  hvm_obj_struct *s = hvm_new_obj_struct();
  hvm_obj_struct_internal_set(s, hvm_symbolicate(vm->symbols, "hvm_exception"), excref);
  // Create the object to return
  hvm_obj_ref *obj;
  obj = hvm_new_obj_ref();
  obj->type = HVM_STRUCTURE;
  obj->data.v = s;
  hvm_obj_space_add_obj_ref(vm->obj_space, obj);
  return obj;
  */
}
