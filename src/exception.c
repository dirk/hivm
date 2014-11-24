#include <stdlib.h>
#include <stdbool.h>
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

/*
hvm_exception *hvm_new_exception() {
  hvm_exception *exc = malloc(sizeof(hvm_exception));
  exc->message   = NULL;
  exc->data      = hvm_const_null;
  exc->backtrace = g_array_new(TRUE, TRUE, sizeof(hvm_location*));
  return exc;
}
*/

hvm_obj_ref *hvm_exception_new(hvm_vm *vm, hvm_obj_ref *message) {
  hvm_obj_ref *exc = hvm_new_obj_ref();
  hvm_obj_struct *excstruct = hvm_new_obj_struct();
  exc->type = HVM_STRUCTURE;
  exc->data.v = excstruct;
  hvm_obj_space_add_obj_ref(vm->obj_space, exc);

  if(message != NULL) {
    hvm_symbol_id sym = hvm_symbolicate(vm->symbols, "message");
    assert(message->type == HVM_STRING);
    hvm_obj_struct_internal_set(excstruct, sym, message);
  }
  return exc;
}

void hvm_exception_push_location(hvm_vm *vm, hvm_obj_ref *exc, hvm_location *loc) {
  hvm_symbol_id sym = hvm_symbolicate(vm->symbols, "backtrace");
  hvm_obj_ref *locations = hvm_obj_struct_internal_get(exc->data.v, sym);
  if(locations == NULL) {
    // If there's no locations array then we need to make one and add it
    // to the exception struct.
    locations = hvm_new_obj_ref();
    locations->type = HVM_ARRAY;
    locations->data.v = hvm_new_obj_array();
    hvm_obj_struct_internal_set(exc->data.v, sym, locations);
  } else {
    assert(locations->type == HVM_ARRAY);
  }
  // Now let's create an internal object ref for the location and push it
  // onto the array.
  hvm_obj_ref *locref = hvm_new_obj_ref();
  locref->type = HVM_INTERNAL;
  locref->data.v = loc;
  hvm_obj_array_push(locations, locref);
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

void hvm_exception_build_backtrace(hvm_obj_ref *exc, hvm_vm *vm) {
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
    hvm_exception_push_location(vm, exc, loc);
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

void hvm_print_backtrace_array(hvm_obj_ref *backtrace) {
  assert(backtrace->type = HVM_ARRAY);
  hvm_obj_array *arr = backtrace->data.v;
  unsigned int i;
  uint64_t len = hvm_obj_array_internal_len(arr);

  for(i = 0; i < len; i++) {
    hvm_obj_ref *locref = hvm_obj_array_internal_get(arr, i);
    assert(locref->type == HVM_INTERNAL);
    hvm_location *loc = locref->data.v;
    if(loc->name != NULL) {
      fprintf(stderr, "    %s (%d:%s)\n", loc->name, loc->line, loc->file);
    } else {
      fprintf(stderr, "    unknown (%d:%s)\n", loc->line, loc->file);
    }
  }
}

void hvm_exception_print(hvm_vm *vm, hvm_obj_ref *exc) {
  hvm_symbol_id messagesym = hvm_symbolicate(vm->symbols, "message");
  hvm_obj_struct *excstruct = exc->data.v;
  hvm_obj_ref *message = hvm_obj_struct_internal_get(excstruct, messagesym);
  char *msg = "(unknown)";
  if(message != NULL) {
    assert(message->type == HVM_STRING);
    hvm_obj_string *messagestr = message->data.v;
    msg = messagestr->data;
  }
  fprintf(stderr, "Exception: %s\n", msg);

  // See if there's a backtrace array
  hvm_symbol_id backtracesym = hvm_symbolicate(vm->symbols, "backtrace");
  hvm_obj_ref *backtrace = hvm_obj_struct_internal_get(excstruct, backtracesym);
  if(backtrace != NULL) {
    fprintf(stderr, "Backtrace:\n");
    hvm_print_backtrace_array(backtrace);
  }
}

/*
hvm_obj_ref *hvm_obj_for_exception(hvm_vm *vm, hvm_exception *exc) {
  // Create the reference to the internal exception
  hvm_obj_ref *excref = hvm_new_obj_ref();
  excref->type = HVM_EXCEPTION;
  excref->data.v = exc;
  excref->flags |= HVM_OBJ_FLAG_NO_FOLLOW;
  hvm_obj_space_add_obj_ref(vm->obj_space, excref);
  return excref;
}
*/
