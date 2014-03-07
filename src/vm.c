#include <stdlib.h>

#include "vm.h"
#include "frame.h"

hvm_vm *hvm_new_vm() {
  hvm_vm *vm = malloc(sizeof(hvm_vm));
  vm->ip = 0;
  return vm;
}
