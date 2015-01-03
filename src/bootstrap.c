#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <sys/time.h>

#include <glib.h>

#include "vm.h"
#include "object.h"
#include "symbol.h"
#include "bootstrap.h"
#include "frame.h"
#include "exception.h"
#include "gc1.h"
#include "chunk.h"
#include "jit-tracer.h"

#define SYM(V) hvm_symbolicate(vm->symbols, V)
#define PRIM_SET(K, V) hvm_obj_struct_internal_set(vm->primitives, SYM(K), (void*)V);

void hvm_bootstrap_primitives(hvm_vm *vm) {
  // hvm_obj_ref* (*prim)(hvm_vm *vm);
  // prim = hvm_prim_print;
  PRIM_SET("print", hvm_prim_print);
  PRIM_SET("print_char", hvm_prim_print_char);
  PRIM_SET("print_exception", hvm_prim_print_exception);
  PRIM_SET("int_to_string", hvm_prim_int_to_string);
  PRIM_SET("array_clone", hvm_prim_array_clone);
  PRIM_SET("exit", hvm_prim_exit);

  PRIM_SET("time_as_int", hvm_prim_time_as_int);

  PRIM_SET("gc_run", hvm_prim_gc_run);
  PRIM_SET("rand", hvm_prim_rand);

  PRIM_SET("debug_print_struct", hvm_prim_debug_print_struct);
  PRIM_SET("debug_print_current_frame_trace", hvm_prim_debug_print_current_frame_trace);
}

hvm_obj_ref *hvm_prim_exit(hvm_vm *vm) {
  exit(0);
}

hvm_obj_ref *hvm_prim_print_exception(hvm_vm *vm) {
  hvm_obj_ref *excref = vm->param_regs[0];
  assert(excref->type == HVM_STRUCTURE);

  hvm_symbol_id sym = hvm_symbolicate(vm->symbols, "message");
  hvm_obj_ref *messageref = hvm_obj_struct_internal_get(excref->data.v, sym);
  assert(messageref->type == HVM_STRING);

  // TODO: Make exceptions be plain structures?
  // hvm_obj_ref *excstruct = vm->param_regs[0];
  // assert(excstruct != NULL);
  // assert(excstruct->type == HVM_STRUCTURE);
  // excref = hvm_obj_struct_internal_get(excstruct->data.v, hvm_symbolicate(vm->symbols, "hvm_exception"));
  
  // assert(excref != NULL);
  // assert(excref->type == HVM_EXCEPTION);
  // hvm_exception *exc = excref->data.v;
  hvm_exception_print(vm, excref);
  return hvm_const_null;
}

bool hvm_type_check(char *name, hvm_obj_type type, hvm_obj_ref* ref, hvm_vm *vm) {
  if(ref->type != type) {
    char buff[256];
    buff[0] = '\0';
    strcat(buff, "`");
    strcat(buff, name);
    strcat(buff, "` expects ");
    strcat(buff, hvm_human_name_for_obj_type(type));
    strcat(buff, ", got ");
    strcat(buff, hvm_human_name_for_obj_type(ref->type));
    hvm_obj_ref *message = hvm_new_obj_ref_string_data(hvm_util_strclone(buff));
    hvm_obj_ref *exc = hvm_exception_new(vm, message);
    // Push the primitive as the first location
    hvm_location *loc = hvm_new_location();
    loc->name = hvm_util_strclone(name);
    hvm_exception_push_location(vm, exc, loc);
    vm->exception = exc;
    return 0;
  } else {
    return 1;
  }
}

hvm_obj_ref *hvm_prim_print(hvm_vm *vm) {
  hvm_obj_ref *strref = vm->param_regs[0];
  if(strref == NULL) {
    // Missing parameter
    static char *buff = "`print` expects 1 argument";
    hvm_obj_ref *message = hvm_new_obj_ref_string_data(hvm_util_strclone(buff));
    hvm_obj_ref *exc = hvm_exception_new(vm, message);

    hvm_location *loc = hvm_new_location();
    loc->name = hvm_util_strclone("hvm_prim_print");
    hvm_exception_push_location(vm, exc, loc);

    vm->exception = exc;
    return NULL;
  }
  if(!hvm_type_check("print", HVM_STRING, strref, vm)) { return NULL; }
  hvm_obj_string *str = strref->data.v;
  fputs(str->data, stdout);
  return hvm_const_null;
}
hvm_obj_ref *hvm_prim_print_char(hvm_vm *vm) {
  hvm_obj_ref *intref = vm->param_regs[0];
  if(!hvm_type_check("print_char", HVM_INTEGER, intref, vm)) { return NULL; }
  int64_t i = intref->data.i64;
  // fprintf(stderr, "char: %lld\n", i);
  char    c = (char)i;
  fputc(c, stdout);
  return hvm_const_null;
}

hvm_obj_ref *hvm_prim_array_clone(hvm_vm *vm) {
  hvm_obj_ref *arrref = vm->param_regs[0];
  if(!hvm_type_check("array_clone", HVM_ARRAY, arrref, vm)) { return NULL; }
  hvm_obj_array *arr = arrref->data.v;
  guint len = arr->array->len;
  // Set up the new array and copy over
  hvm_obj_array *newarr = malloc(sizeof(hvm_obj_array));
  newarr->array = g_array_sized_new(TRUE, TRUE, sizeof(hvm_obj_ref*), len);
  newarr->array->len = len;
  for(guint idx = 0; idx < len; idx++) {
    // Copy source pointer from original array into destination in new array
    hvm_obj_ref *src   = g_array_index(arr->array, hvm_obj_ref*, idx);
    hvm_obj_ref **dest = &g_array_index(newarr->array, hvm_obj_ref*, idx);
    *dest = src;
  }
  hvm_obj_ref *newarrref = hvm_new_obj_ref();
  newarrref->type = HVM_ARRAY;
  newarrref->data.v = newarr;
  return newarrref;
}

hvm_obj_ref *hvm_prim_int_to_string(hvm_vm *vm) {
  hvm_obj_ref *intref = vm->param_regs[0];
  assert(intref != NULL);
  assert(intref->type == HVM_INTEGER);
  int64_t intval = intref->data.i64;
  char buff[24];// Enough to show a 64-bit signed integer in base 10
  int err = sprintf(buff, "%lld", intval);
  assert(err >= 0);
  hvm_obj_ref *str = hvm_new_obj_ref_string_data(hvm_util_strclone(buff));
  hvm_obj_space_add_obj_ref(vm->obj_space, str);
  return str;
}

// Returns microseconds since epoch as 64-bit integer
hvm_obj_ref *hvm_prim_time_as_int(hvm_vm *vm) {
  int64_t sec;
  hvm_obj_ref * ret;
  struct timeval tv;
  gettimeofday(&tv, NULL);
  sec = (1000000 * tv.tv_sec) + tv.tv_usec;
  ret = hvm_new_obj_int(vm);
  ret->data.i64 = sec;
  return ret;
}

hvm_obj_ref *hvm_prim_debug_print_struct(hvm_vm *vm) {
  hvm_obj_ref *structref = vm->param_regs[0];
  if(!hvm_type_check("debug_print_struct", HVM_STRUCTURE, structref, vm)) { return NULL; }
  // Iterating through the structure
  hvm_obj_struct *strct = structref->data.v;
  fprintf(stdout, "structure(%p)\n", strct);
  unsigned int idx;
  for(idx = 0; idx < strct->heap_length; idx++) {
    hvm_obj_struct_heap_pair *pair = strct->heap[idx];
    char        *sym  = hvm_desymbolicate(vm->symbols, pair->id);
    hvm_obj_ref *ref  = pair->obj;
    const char  *name = hvm_human_name_for_obj_type(ref->type);
    fprintf(stdout, "  %s = %s(%p)\n", sym, name, ref);
  }
  return hvm_const_null;
}

hvm_obj_ref *hvm_prim_debug_print_current_frame_trace(hvm_vm *vm) {
  hvm_frame *frame = vm->top;
  if(frame->trace == NULL) {
    fprintf(stdout, "error: No trace in current frame\n");
    goto end;
  }
  hvm_call_trace *trace = frame->trace;
  printf("frame(%p) = [%u]{\n", frame, trace->sequence_length);
  hvm_jit_tracer_dump_trace(vm, trace);
  printf("}\n");

end:
  return hvm_const_null;
}

hvm_obj_ref *hvm_prim_gc_run(hvm_vm *vm) {
  hvm_gc1_run(vm, vm->obj_space);
  return hvm_const_null;
}

hvm_obj_ref *hvm_prim_rand(hvm_vm *vm) {
  int ret = rand();
  // Create the full object reference with our random integer
  hvm_obj_ref *ref = hvm_new_obj_int(vm);
  ref->data.i64 = (int64_t)ret;
  // Make sure it's in the object space
  hvm_obj_space_add_obj_ref(vm->obj_space, ref);
  return ref;
}
