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

void hvm_bootstrap_primitives(hvm_vm *vm) {
  hvm_obj_ref* (*prim)(hvm_vm *vm);

  prim = hvm_prim_print;
  PRIM_SET("print", prim);

  PRIM_SET("int_to_string", hvm_prim_int_to_string);

  PRIM_SET("exit", hvm_prim_exit);
}

hvm_obj_ref *hvm_prim_exit(hvm_vm *vm) {
  exit(0);
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
  if(strref->type != HVM_STRING) {
    hvm_exception *exc = hvm_new_exception();
    char *type = hvm_human_name_for_obj_type(strref);
    char buff[256];
    buff[0] = '\0';
    strcat(buff, "`print` expects string, got ");
    strcat(buff, type);
    hvm_obj_ref *obj = hvm_new_obj_ref_string_data(hvm_util_strclone(buff));
    exc->message = obj;

    hvm_location *loc = hvm_new_location();
    loc->name = hvm_util_strclone("hvm_prim_print");
    hvm_exception_push_location(exc, loc);

    vm->exception = exc;
    return NULL;
  }
  hvm_obj_string *str = strref->data.v;
  fputs(str->data, stdout);
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
