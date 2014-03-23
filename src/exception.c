#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include <glib.h>

#include "vm.h"
#include "symbol.h"
#include "object.h"
#include "frame.h"
#include "chunk.h"
#include "exception.h"

hvm_exception *hvm_new_exception() {
  hvm_exception *exc = malloc(sizeof(hvm_exception));
  exc->message = NULL;
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

void hvm_exception_build_backtrace(hvm_exception *exc, hvm_vm *vm) {
  uint32_t i = vm->stack_depth;
  while(1) {
    hvm_frame* frame = &vm->stack[i];
    uint64_t ip = frame->current_addr;

    hvm_chunk_debug_entry *de = hvm_vm_find_debug_entry(vm, ip);
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

    // Preventing integer overflow wrap-around.
    if(i == 0) { break; }
    i--;
  }
}

void hvm_exception_print(hvm_exception *exc) {
  hvm_obj_ref    *ref = exc->message;
  hvm_obj_string *str = ref->data.v;
  fprintf(stderr, "Exception: %s\n", str->data);
  fprintf(stderr, "Backtrace:\n");
  unsigned int i;
  hvm_location *loc;
  for(i = 0; i < exc->backtrace->len; i++) {
    loc = g_array_index(exc->backtrace, hvm_location*, i);
    if(loc->name != NULL) {
      fprintf(stderr, "    %s (%d:%s)\n", loc->name, loc->line, loc->file);
    } else {
      fprintf(stderr, "    unknown (%d:%s)\n", loc->line, loc->file);
    }
  }
}
