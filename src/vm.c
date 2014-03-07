#include <stdlib.h>
#include <stdio.h>

#include "vm.h"
#include "frame.h"

hvm_vm *hvm_new_vm() {
  hvm_vm *vm = malloc(sizeof(hvm_vm));
  vm->ip = 0;
  vm->program_size = HVM_PROGRAM_INITIAL_SIZE;
  vm->program = calloc(sizeof(byte), vm->program_size);
  return vm;
}

void hvm_vm_run(hvm_vm *vm) {
  byte instr;
  
  for(;;) {
    instr = vm->program[vm->ip];
    switch(instr) {
      case HVM_OP_NOOP:
        fprintf(stderr, "NOOP\n");
        break;
      case HVM_OP_DIE:
        fprintf(stderr, "DIE\n");
        goto end;
      default:
        fprintf(stderr, "Unknown instruction: %u\n", instr);
    }
    vm->ip++;
  }
end:
  return;
  //pass
}
