#include "vm.h"

int main(int argc, char **argv) {
  hvm_vm *vm = hvm_new_vm();

  vm->program[0] = HVM_OP_NOOP;
  vm->program[1] = HVM_OP_DIE;

  hvm_vm_run(vm);

  return 0;
}
