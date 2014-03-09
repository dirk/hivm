#include <stdio.h>

#include "hvm.h"
#include "hvm_symbol.h"
#include "hvm_object.h"

void test_heap() {
  hvm_obj_string *str = hvm_new_obj_string();
  str->data = "test";
  hvm_obj_ref *ref = hvm_new_obj_ref();
  hvm_obj_ref_set_string(ref, str);

  hvm_obj_string *str2;
  hvm_obj_ref    *ref2;;

  hvm_obj_struct *s = hvm_new_obj_struct();
  hvm_obj_struct_set(s, 2, ref);
  hvm_obj_struct_set(s, 3, ref);
  hvm_obj_struct_set(s, 1, ref);

  ref2 = hvm_obj_struct_get(s, 2);
  str2 = (hvm_obj_string*)(ref2->data);
  printf("str2->data: %s\n", str2->data);
}

int main(int argc, char **argv) {
  test_heap();

  /*
  hvm_vm *vm = hvm_new_vm();

  hvm_obj_string *str = hvm_new_obj_string();
  str->data = "test";
  hvm_obj_ref *ref = hvm_new_obj_ref();
  hvm_obj_ref_set_string(ref, str);
  hvm_vm_set_const(vm, 1234567, ref);

  vm->program[0] = HVM_OP_NOOP;
  vm->program[1] = HVM_OP_SETSTRING;
  vm->program[2] = 1;// register destination
  *(uint32_t*)&vm->program[3] = 1234567;// 32-bit integer const index

  vm->program[7] = HVM_OP_NOOP;
  vm->program[8] = HVM_OP_DIE;

  hvm_vm_run(vm);

  hvm_obj_ref *reg;
  reg = vm->general_regs[1];
  printf("reg: %p\n", reg);
  printf("reg->type: %d\n", reg->type);
  hvm_obj_string *str2;
  str2 = (hvm_obj_string*)(reg->data);
  printf("str2->data: %s\n", str2->data);
  */

  /*
  hvm_symbol_table *st = new_hvm_symbol_table();
  printf("size = %llu\n", st->size);
  uint64_t a, b, c, a2;
  a = hvm_symbolicate(st, "a");
  b = hvm_symbolicate(st, "b");
  c = hvm_symbolicate(st, "c");
  a2 = hvm_symbolicate(st, "a");
  printf("a:  %llu\n", a);
  printf("b:  %llu\n", b);
  printf("c:  %llu\n", c);
  printf("a2: %llu\n", a2);
  printf("size = %llu\n", st->size);
  */
  return 0;
}
