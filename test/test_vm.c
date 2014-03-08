#include <stdio.h>

#include "hvm.h"
#include "hvm_symbol.h"

int main(int argc, char **argv) {
  hvm_vm *vm = hvm_new_vm();

  vm->program[0] = HVM_OP_NOOP;
  vm->program[1] = HVM_OP_SETSTRING;
  vm->program[2] = 1;// register destination
  *(uint32_t*)&vm->program[3] = 1234567;// 32-bit integer const index
  
  vm->program[7] = HVM_OP_NOOP;
  vm->program[8] = HVM_OP_DIE;

  hvm_vm_run(vm);
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