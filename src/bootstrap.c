#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "vm.h"
#include "object.h"
#include "symbol.h"
#include "bootstrap.h"

#define SYM(V) hvm_symbolicate(vm->symbols, V)
#define PRIM_SET(K, V) hvm_obj_struct_internal_set(vm->primitives, SYM(K), (void*)V);

void hvm_bootstrap_primitives(hvm_vm *vm) {
  hvm_obj_ref* (*prim)(hvm_vm *vm);

  prim = hvm_prim_print;
  PRIM_SET("print", prim);

  prim = hvm_prim_exit;
  PRIM_SET("exit", hvm_prim_exit);
}

hvm_obj_ref *hvm_prim_exit(hvm_vm *vm) {
  exit(0);
}

hvm_obj_ref *hvm_prim_print(hvm_vm *vm) {
  hvm_obj_ref *strref = vm->param_regs[0];
  assert(strref != NULL);
  assert(strref->type == HVM_STRING);
  hvm_obj_string *str = strref->data.v;
  fputs(str->data, stdout);
  return hvm_const_null;
}
