#ifndef HVM_BOOTSTRAP_H
#define HVM_BOOTSTRAP_H

void hvm_bootstrap_primitives(hvm_vm *vm);

hvm_obj_ref *hvm_prim_int_to_string(hvm_vm *vm);
hvm_obj_ref *hvm_prim_exit(hvm_vm *vm);
hvm_obj_ref *hvm_prim_print(hvm_vm *vm);
hvm_obj_ref *hvm_prim_print_exception(hvm_vm *vm);
hvm_obj_ref *hvm_prim_print_char(hvm_vm *vm);
hvm_obj_ref *hvm_prim_gc_run(hvm_vm *vm);
hvm_obj_ref *hvm_prim_rand(hvm_vm *vm);
hvm_obj_ref *hvm_prim_array_clone(hvm_vm *vm);
hvm_obj_ref *hvm_prim_time_as_int(hvm_vm *vm);

hvm_obj_ref *hvm_prim_debug_print_struct(hvm_vm *vm);
hvm_obj_ref *hvm_prim_debug_print_current_frame_trace(hvm_vm *vm);

#endif
