#ifndef HVM_BOOTSTRAP_H
#define HVM_BOOTSTRAP_H

void hvm_bootstrap_primitives(hvm_vm *vm);

hvm_obj_ref *hvm_prim_int_to_string(hvm_vm *vm);
hvm_obj_ref *hvm_prim_exit(hvm_vm *vm);
hvm_obj_ref *hvm_prim_print(hvm_vm *vm);
hvm_obj_ref *hvm_prim_print_exception(hvm_vm *vm);

#endif
