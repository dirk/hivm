#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "vm.h"
#include "object.h"
#include "symbol.h"
#include "bootstrap.h"
#include "frame.h"
#include "exception.h"
#include "gc1.h"
#include "chunk.h"

#define SYM(V) hvm_symbolicate(vm->symbols, V)
#define PRIM_SET(K, V) hvm_obj_struct_internal_set(vm->primitives, SYM(K), (void*)V);

#define bool char

void hvm_bootstrap_primitives(hvm_vm *vm) {
  // hvm_obj_ref* (*prim)(hvm_vm *vm);
  // prim = hvm_prim_print;
  PRIM_SET("print", hvm_prim_print);
  PRIM_SET("print_char", hvm_prim_print_char);
  PRIM_SET("print_exception", hvm_prim_print_exception);
  PRIM_SET("int_to_string", hvm_prim_int_to_string);
  PRIM_SET("exit", hvm_prim_exit);
  PRIM_SET("debug_print_struct", hvm_prim_debug_print_struct);
  PRIM_SET("gc_run", hvm_prim_gc_run);
}

hvm_obj_ref *hvm_prim_exit(hvm_vm *vm) {
  exit(0);
}

hvm_obj_ref *hvm_prim_print_exception(hvm_vm *vm) {
  // hvm_obj_ref *excstruct = vm->param_regs[0];
  // assert(excstruct != NULL);
  // assert(excstruct->type == HVM_STRUCTURE);
  // excref = hvm_obj_struct_internal_get(excstruct->data.v, hvm_symbolicate(vm->symbols, "hvm_exception"));
  hvm_obj_ref *excref = vm->param_regs[0];
  assert(excref != NULL);
  assert(excref->type == HVM_INTERNAL);
  hvm_exception *exc = excref->data.v;
  hvm_exception_print(exc);
  return hvm_const_null;
}

bool hvm_type_check(char *name, hvm_obj_type type, hvm_obj_ref* ref, hvm_vm *vm) {
  if(ref->type != type) {
    hvm_exception *exc = hvm_new_exception();
    char buff[256];
    buff[0] = '\0';
    strcat(buff, "`");
    strcat(buff, name);
    strcat(buff, "` expects ");
    strcat(buff, hvm_human_name_for_obj_type(type));
    strcat(buff, ", got ");
    strcat(buff, hvm_human_name_for_obj_type(ref->type));
    hvm_obj_ref *obj = hvm_new_obj_ref_string_data(hvm_util_strclone(buff));
    exc->message = obj;
    // Push the primitive as the first location
    hvm_location *loc = hvm_new_location();
    loc->name = hvm_util_strclone(name);
    hvm_exception_push_location(exc, loc);
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
    hvm_exception *exc = hvm_new_exception();
    static char *buff = "`print` expects 1 argument";
    hvm_obj_ref *obj = hvm_new_obj_ref_string_data(hvm_util_strclone(buff));
    exc->message = obj;

    hvm_location *loc = hvm_new_location();
    loc->name = hvm_util_strclone("hvm_prim_print");
    hvm_exception_push_location(exc, loc);

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
    char        *name = hvm_human_name_for_obj_type(ref->type);
    fprintf(stdout, "  %s = %s(%p)\n", sym, name, ref);
  }
  return hvm_const_null;
}

hvm_obj_ref *hvm_prim_gc_run(hvm_vm *vm) {
  hvm_gc1_run(vm, vm->obj_space);
  return hvm_const_null;
}
