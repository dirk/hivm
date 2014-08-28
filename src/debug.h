#ifndef HVM_DEBUG_H
#define HVM_DEBUG_H
/// @file debug.h

#ifndef bool
#define bool char
#endif

/// Called by `hvm_new_vm` to add the debugger primitives to the VM.
void hvm_debug_setup(hvm_vm*);

/// Hook called by `hvm_vm_run` before it begins executing an instruction.
/// @param    vm
/// @param    instr  Instruction the VM is about to excute.
/// @retval   bool   Whether the VM should continue execution.
bool hvm_debug_before_instruction(hvm_vm*, byte);

void hvm_debug_begin();

#endif
